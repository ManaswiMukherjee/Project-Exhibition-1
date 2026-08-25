#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"
#include "config.h"
#include "index.h"

WebServer server(80);

// Fixed-size char buffers rather than Arduino String -- this board runs
// continuously (kiosk display), and repeatedly reassigning String objects
// every 15s risks heap fragmentation over long uptimes. Fixed buffers
// sidestep that entirely at the cost of a hard length cap (truncated with
// strncpy if a name/item list is ever longer than the buffer).
struct DisplayOrder {
  char cname[32];
  char items[100];  // joined display string, e.g. "Masala Dosa x2, Filter Coffee x1"
};

DisplayOrder currentOrders[MAX_ORDERS];
int orderCount = 0;

unsigned long lastFetchTime = 0;
unsigned long lastSuccessfulFetchMillis = 0;  // 0 = never succeeded yet
unsigned long lastReconnectAttempt = 0;

// ---------------------------------------------------------------------
// Pulls GET /ready-queue from the backend, parses it, and refills
// currentOrders[]. On ANY failure (WiFi down, backend unreachable,
// timeout, bad JSON) this just returns without touching currentOrders[],
// so the board keeps its last successfully fetched list rather than
// going blank. lastSuccessfulFetchMillis only updates on real success,
// which is what the staleness indicator on the display page is based on.
// ---------------------------------------------------------------------
void fetchReadyQueue() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[fetch] WiFi not connected, skipping this cycle.");
    return;
  }

  HTTPClient http;
  String url = String("http://") + BACKEND_HOST + ":" + String(BACKEND_PORT) + "/ready-queue";

  http.begin(url);
  http.setTimeout(HTTP_TIMEOUT_MS);
  int httpCode = http.GET();

  if (httpCode != 200) {
    Serial.print("[fetch] GET failed, HTTP code: ");
    Serial.println(httpCode);
    http.end();
    return;
  }

  // Deserialize straight from the response stream rather than buffering
  // the whole body into a String first -- lighter on memory.
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();

  if (err) {
    Serial.print("[fetch] JSON parse failed: ");
    Serial.println(err.c_str());
    return;
  }

  JsonArray arr = doc.as<JsonArray>();
  if (arr.size() > MAX_ORDERS) {
    Serial.print("[fetch] ready-queue has more orders than MAX_ORDERS (");
    Serial.print(arr.size());
    Serial.print(" > ");
    Serial.print(MAX_ORDERS);
    Serial.println("), truncating to the oldest ones.");
  }

  int count = 0;
  for (JsonObject order : arr) {
    if (count >= MAX_ORDERS) break;

    const char* cname = order["cname"] | "Unknown";
    strncpy(currentOrders[count].cname, cname, sizeof(currentOrders[count].cname) - 1);
    currentOrders[count].cname[sizeof(currentOrders[count].cname) - 1] = '\0';

    // Join order_contents into one line, e.g. "Masala Dosa x2, Filter Coffee x1"
    String joined = "";
    JsonArray contents = order["order_contents"].as<JsonArray>();
    for (JsonObject item : contents) {
      if (joined.length() > 0) joined += ", ";
      const char* itemName = item["item"] | "";
      int qty = item["qty"] | 1;
      joined += itemName;
      joined += " x";
      joined += qty;
    }
    strncpy(currentOrders[count].items, joined.c_str(), sizeof(currentOrders[count].items) - 1);
    currentOrders[count].items[sizeof(currentOrders[count].items) - 1] = '\0';

    count++;
  }

  orderCount = count;
  lastSuccessfulFetchMillis = millis();
  Serial.print("[fetch] OK, orders loaded: ");
  Serial.println(orderCount);
}

// ---------------------------------------------------------------------
// GET / -- serves the display page. The page no longer needs any
// server-side templating; its own JS fills in rows and the status
// indicator by polling GET /data below.
// ---------------------------------------------------------------------
void handleRoot() {
  server.send(200, "text/html", INDEX_HTML);
}

// ---------------------------------------------------------------------
// GET /data -- returns the ESP32's cached order list PLUS freshness info
// as JSON:
//   { "orders": [...], "ageSeconds": N, "wifiConnected": true }
// ageSeconds is -1 if the ESP32 has never successfully fetched from the
// backend yet (vs. 0+ once it has, even if the queue itself is empty).
// This is what the display page's JS polls every 5s.
// ---------------------------------------------------------------------
void handleData() {
  JsonDocument doc;

  JsonArray arr = doc["orders"].to<JsonArray>();
  for (int i = 0; i < orderCount; i++) {
    JsonObject o = arr.add<JsonObject>();
    o["cname"] = currentOrders[i].cname;
    o["items"] = currentOrders[i].items;
  }

  if (lastSuccessfulFetchMillis == 0) {
    doc["ageSeconds"] = -1;
  } else {
    doc["ageSeconds"] = (millis() - lastSuccessfulFetchMillis) / 1000;
  }
  doc["wifiConnected"] = (WiFi.status() == WL_CONNECTED);

  String output;
  serializeJson(doc, output);
  server.send(200, "application/json", output);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  delay(3000);
  Serial.println("starting wifi");
  WiFi.begin(SECRET_SSID, SECRET_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
  Serial.print("Display board is at: http://");
  Serial.println(WiFi.localIP());
  Serial.print("Pulling orders from backend at: http://");
  Serial.print(BACKEND_HOST);
  Serial.print(":");
  Serial.println(BACKEND_PORT);

  // Get one batch of real data before the display goes live, so the
  // first page load isn't stuck on an empty/loading table.
  fetchReadyQueue();
  lastFetchTime = millis();

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
}

void loop() {
  server.handleClient();

  unsigned long now = millis();

  // If WiFi has dropped, periodically try to reconnect using the stored
  // credentials rather than waiting for a manual power-cycle. This is a
  // kiosk device meant to run unattended, so recovering from a router
  // blip or brief signal drop on its own matters.
  if (WiFi.status() != WL_CONNECTED) {
    if (now - lastReconnectAttempt >= RECONNECT_INTERVAL_MS) {
      Serial.println("[wifi] Connection lost, attempting reconnect...");
      WiFi.reconnect();
      lastReconnectAttempt = now;
    }
  }

  if (now - lastFetchTime >= FETCH_INTERVAL_MS) {
    fetchReadyQueue();
    lastFetchTime = now;
  }
}
