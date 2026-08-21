#include <WiFi.h>
#include <WebServer.h>
#include "secrets.h"
#include "index.h"

WebServer server(80);

// Simplified struct containing only rendered fields
struct Order {
  const char* name;
  const char* item;
};

// 62 Sample Orders
Order orders[] = {
  {"Alice", "Burger & Fries"},
  {"Bob", "Iced Coffee"},
  {"Charlie", "Pepperoni Pizza"},
  {"Diana", "Caesar Salad"},
  {"Evan", "Club Sandwich"},
  {"Fiona", "Espresso"},
  {"George", "Truffle Fries"},
  {"Hannah", "Matcha Latte"},
  {"Ian", "Margherita Pizza"},
  {"Julia", "Chicken Wrap"},
  {"Kevin", "Bacon Cheeseburger"},
  {"Laura", "Fresh Lemonade"},
  {"Michael", "Fish & Chips"},
  {"Nina", "Avocado Toast"},
  {"Oliver", "Double Espresso"},
  {"Paula", "Greek Salad"},
  {"Quinn", "Steak Sandwich"},
  {"Rachel", "Iced Peach Tea"},
  {"Sam", "BBQ Chicken Wings"},
  {"Tina", "Caprese Salad"},
  {"Ulysses", "Nitro Cold Brew"},
  {"Victoria", "Veggie Burger"},
  {"Will", "Philly Cheesesteak"},
  {"Xena", "Berry Fruit Smoothie"},
  {"Yusuf", "Falafel Wrap"},
  {"Zoe", "Mushroom Risotto"},
  {"Aaron", "Chicken Tenders"},
  {"Bella", "Vanilla Cappuccino"},
  {"Chris", "Garlic Bread"},
  {"Daisy", "Mango Smoothie"},
  {"Ethan", "Pulled Pork Sandwich"},
  {"Faith", "Chicken Caesar Wrap"},
  {"Gavin", "Hot Chocolate"},
  {"Holly", "Crispy Onion Rings"},
  {"Isaac", "Baked Mac & Cheese"},
  {"Jack", "Iced Americano"},
  {"Kara", "Cobb Salad"},
  {"Leo", "Breakfast Burrito"},
  {"Mia", "Chai Tea Latte"},
  {"Noah", "Loaded Nachos"},
  {"Olivia", "Turkey Panini"},
  {"Peter", "Flat White"},
  {"Quentin", "Chicken Quesadilla"},
  {"Rose", "Sparkling Water"},
  {"Sean", "Classic Hot Dog"},
  {"Tara", "Jasmine Green Tea"},
  {"Umar", "Clam Chowder"},
  {"Valerie", "Mozzarella Sticks"},
  {"Wyatt", "Chocolate Milkshake"},
  {"Xander", "Grilled Cheese"},
  {"Yara", "Caramel Macchiato"},
  {"Zack", "Buffalo Wings"},
  {"Amber", "Club Wrap"},
  {"Brian", "Caffè Latte"},
  {"Chloe", "Taco Salad"},
  {"David", "Beef Sliders"},
  {"Emma", "Sweet Iced Tea"},
  {"Frank", "Fish Tacos"},
  {"Grace", "Acai Smoothie Bowl"},
  {"Henry", "BLT Sandwich"},
  {"Isla", "Caffè Mocha"},
  {"Jake", "Chicken Parm Sub"}
};

void handleRoot() {
  String rows = "";
  int count = sizeof(orders) / sizeof(orders[0]);

  for (int i = 0; i < count; i++) {
    rows += "<tr>";
    rows += "<td>" + String(orders[i].name) + "</td>";
    rows += "<td>" + String(orders[i].item) + "</td>";
    rows += "</tr>";
  }

  String html = INDEX_HTML;
  html.replace("%TABLE_ROWS%", rows);

  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(SECRET_SSID, SECRET_PASS);

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