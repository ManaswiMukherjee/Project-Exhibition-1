#ifndef CONFIG_H
#define CONFIG_H

// Set to true for Render cloud deployment, false for local server testing
#define USE_CLOUD_BACKEND true

// ---- Cloud Backend Settings ----
const char* BACKEND_CLOUD_HOST = "rassence-backend.onrender.com"; // Replace with your Render host

// ---- Local Backend Settings ----
const char* BACKEND_LOCAL_HOST = "192.168.18.56";
const int BACKEND_LOCAL_PORT = 8000;

// ---- Timing & Limits ----
const unsigned long FETCH_INTERVAL_MS = 10000;
const unsigned long HTTP_TIMEOUT_MS = 5000;
const unsigned long RECONNECT_INTERVAL_MS = 5000;
const int MAX_ORDERS = 25;

#endif