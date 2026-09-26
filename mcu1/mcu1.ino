#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "secrets.h"
#include "config.h"
#include "index.h"

WebServer server(80);

struct DisplayOrder {
  char cname[32];
  char items[100];
};

DisplayOrder currentOrders[MAX_ORDERS];
int orderCount = 0;

unsigned long lastFetchTime = 0;
unsigned long lastSuccessfulFetchMillis = 0;
unsigned long lastReconnectAttempt = 0;

void fetchReadyQueue() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[fetch] WiFi not connected, skipping this cycle.");
    return;
  }

  HTTPClient http;

#if USE_CLOUD_BACKEND
  WiFiClientSecure client;
  client.setInsecure();
  String url = String("https://") + BACKEND_CLOUD_HOST + "/ready-queue";
  http.begin(client, url);
#else
  String url = String("http://") + BACKEND_LOCAL_HOST + ":" + String(BACKEND_LOCAL_PORT) + "/ready-queue";
  http.begin(url);
#endif

  http.setTimeout(HTTP_TIMEOUT_MS);
  int httpCode = http.GET();

  if (httpCode != 200) {
    Serial.print("[fetch] GET failed, HTTP code: ");
    Serial.println(httpCode);
    http.end();
    return;
  }

  // Buffer response body as a String to prevent TLS stream timeouts
  String payload = http.getString();
  http.end();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);

  if (err) {
    Serial.print("[fetch] JSON parse failed: ");
    Serial.println(err.c_str());
    Serial.print("[fetch] Raw response received: ");
    Serial.println(payload); // Prints exact server response for easy debugging
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

void handleRoot() {
  server.send(200, "text/html", INDEX_HTML);
}

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

#if USE_CLOUD_BACKEND
  Serial.print("Pulling orders from Cloud at: https://");
  Serial.println(BACKEND_CLOUD_HOST);
#else
  Serial.print("Pulling orders from Local at: http://");
  Serial.print(BACKEND_LOCAL_HOST);
  Serial.print(":");
  Serial.println(BACKEND_LOCAL_PORT);
#endif

  fetchReadyQueue();
  lastFetchTime = millis();

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
}

void loop() {
  server.handleClient();

  unsigned long now = millis();

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