#include <WiFi.h>
#include <WebServer.h>

// Enter your Wi-Fi credentials
const char* ssid = "Redmi Valid";
const char* password = "12345678";

WebServer server(80);

// Sample Data Structure
struct Order {
  int id;
  const char* name;
  const char* time;
  const char* item;
  const char* status;
};

// Sample Array of Rows
Order orders[] = {
  {101, "Alice", "12:30 PM", "Burger & Fries", "Done"},
  {102, "Bob", "12:32 PM", "Iced Coffee", "Preparing"},
  {103, "Charlie", "12:35 PM", "Pepperoni Pizza", "Preparing"},
  {104, "Diana", "12:38 PM", "Caesar Salad", "Done"},
  {105, "Evan", "12:40 PM", "Club Sandwich", "Preparing"}
};

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><title>Display Board</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  
  // Custom Dark Mode CSS for an Airport/Restaurant Board look
  html += "<style>";
  html += "body { background-color: #0b0e14; color: #e6edf3; font-family: monospace, sans-serif; padding: 30px; text-align: center; }";
  html += "h1 { color: #f2a900; font-size: 2.5rem; letter-spacing: 3px; margin-bottom: 30px; text-transform: uppercase; }";
  html += "table { width: 100%; border-collapse: collapse; font-size: 1.5rem; }";
  html += "th, td { padding: 18px; border-bottom: 1px solid #30363d; text-align: left; }";
  html += "th { background-color: #161b22; color: #8b949e; text-transform: uppercase; letter-spacing: 1px; }";
  html += "tr:nth-child(even) { background-color: #161b22; }";
  html += ".done { color: #3fb950; font-weight: bold; }";
  html += ".preparing { color: #d29922; font-weight: bold; }";
  html += "</style>";
  
  // Auto-refresh the page every 5 seconds to load updated data
  html += "<meta http-equiv='refresh' content='5'>"; 
  html += "</head><body>";
  
  html += "<h1>LIVE STATUS BOARD</h1>";
  html += "<table>";
  html += "<tr><th>ID</th><th>Name</th><th>Time</th><th>Order</th><th>Status</th></tr>";

  // Build the table rows dynamically
  int count = sizeof(orders) / sizeof(orders[0]);
  for (int i = 0; i < count; i++) {
    html += "<tr>";
    html += "<td>#" + String(orders[i].id) + "</td>";
    html += "<td>" + String(orders[i].name) + "</td>";
    html += "<td>" + String(orders[i].time) + "</td>";
    html += "<td>" + String(orders[i].item) + "</td>";
    
    // Style status color conditionally
    String statusClass = (String(orders[i].status) == "Done") ? "done" : "preparing";
    html += "<td class='" + statusClass + "'>" + String(orders[i].status) + "</td>";
    html += "</tr>";
  }

  html += "</table></body></html>";
  
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi Connected!");
  Serial.print("Access your board at IP: http://");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.begin();
}

void loop() {
  server.handleClient();
}