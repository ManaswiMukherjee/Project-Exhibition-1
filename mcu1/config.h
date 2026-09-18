#ifndef CONFIG_H
#define CONFIG_H

// ---- Backend connection ------------------------------------------------
// The FastAPI backend's LAN IP and port. Find it by running `ip addr`
// (Linux) / `ipconfig` (Windows) on the machine running
// `uvicorn main:app --host 0.0.0.0 --port 8000`, then update BACKEND_HOST
// below and re-flash. See canteen-backend/README.md re: setting a static
// IP / DHCP reservation on the router so this doesn't drift and need
// re-flashing every session.
const char* BACKEND_HOST = "192.168.18.56";  // <-- CHANGE THIS before flashing
const int BACKEND_PORT = 8000;

// ---- Timing --------------------------------------------------------------
// How often the ESP32 itself asks the real backend for fresh data.
const unsigned long FETCH_INTERVAL_MS = 15000;

// How long to wait for the backend to respond before giving up on a single
// fetch attempt (keeps an unreachable/slow server from hanging the board).
const unsigned long HTTP_TIMEOUT_MS = 5000;

// How often to retry WiFi.reconnect() while disconnected.
const unsigned long RECONNECT_INTERVAL_MS = 5000;

// Max orders the board will hold/display at once. /ready-queue is already
// sorted oldest-first by the backend, so if it ever exceeds this, the
// oldest (most overdue) orders are kept and the newest are dropped from
// the board until they age into the front of the list.
const int MAX_ORDERS = 25;

#endif
