# Rassence Caterers: Canteen Token Distribution System

A lightweight order-distribution system for the Rassence Caterers canteen at VIT Bhopal. It covers the last step of the pre-order workflow: **food is ready, then the customer collects it**. Staff scan a QR code on the kitchen slip, students track their order from their phones, and a live display board near the counter shows which orders are ready.

The ordering, payment, and slip-generation system is separate and out of scope. This backend knows nothing about an order until its QR code is scanned.

## Problem

Under the pre-order system, one staff member manually matched paper slips against food trays. At peak hours this caused latency, mix-ups, and crowding at the counter, because students had no visibility into order status.

## How it works

Each order produces two slips:

- **Customer slip:** order ID and contents, no QR code.
- **Kitchen slip:** order contents plus one QR code encoding `{id, cname, order_contents}`. It stays with the food tray.

The same QR code is scanned twice:

1. **Food ready scan:** the order is created with status `READY`. It appears on the display board and becomes visible to the student tracker.
2. **Handoff scan:** the status flips to `COLLECTED`, and the order leaves the display board.

A third scan of an already-collected order is rejected with `409 Conflict`.

```
                 +-------------------+
 Staff phone --> |                   | --> Turso (libSQL)
 (QR scanner)    |  FastAPI backend  |
                 |    on Render      | <-- Student tracker (polls /status/{id})
                 |                   | <-- ESP32 display  (polls /ready-queue)
                 +-------------------+
```

All components communicate through the central backend; there is no direct device-to-device link.

## Components

| Component | Path | Description |
|---|---|---|
| Backend API | `canteen-backend/` | FastAPI + SQLModel service, backed by Turso |
| Staff scanner | `frontend/staff.html` | Mobile web page that scans QR codes with the phone camera and calls `/scan` |
| Student tracker | `frontend/student.html` | Page where a student enters their order ID and sees live status |
| Landing page | `frontend/index.html` | Entry page linking to the tracker and staff panel |
| Display board | `mcu1/` | ESP32 firmware that polls the backend and serves a live status board |
| Slip generator | `order_qr/` | Testing aid that produces sample customer and kitchen slips (see below) |

The frontend is served by the backend itself under `/app`.

## Tech stack

- **Backend:** Python, FastAPI, SQLModel, slowapi (rate limiting)
- **Database:** Turso (libSQL) through `sqlalchemy-libsql`
- **Hosting:** Render
- **Staff scanner:** vanilla JS with `html5-qrcode`, bundled locally in `frontend/lib/`
- **Student tracker:** vanilla HTML/JS, 4-second polling
- **Display board:** ESP32 (Arduino framework) with ArduinoJson

## Data model

A single table, `orders`, with no history:

| Column | Type | Notes |
|---|---|---|
| `id` | string, primary key | `YYMMDDHHMMSS` timestamp taken from the QR payload |
| `cname` | string | Customer name |
| `order_contents` | string (JSON) | `[{"item": "...", "qty": N}, ...]`, returned as a real JSON list by the API |
| `status` | string | `READY` or `COLLECTED` |

Order IDs are timestamps. No two orders are placed in the same second, so the ID is safe as a primary key.

## API

Interactive docs are available at `/docs` on the running service.

### `GET /verify-staff-key`
Checks that the `X-Staff-Key` header matches the configured staff key. Returns `{"status": "ok"}` or `401`. Used by the scanner page's access-code screen.

### `POST /scan`  (requires `X-Staff-Key`)
Handles both scans of the same QR code.

Request body (exactly what the QR encodes):

```json
{
  "id": "260821143045",
  "cname": "Manaswi",
  "order_contents": [
    {"item": "Masala Dosa", "qty": 2},
    {"item": "Filter Coffee", "qty": 1}
  ]
}
```

| Situation | Result |
|---|---|
| `id` not in the database | Row created, `{"status": "READY", "action": "created"}` |
| `id` exists with status `READY` | Updated, `{"status": "COLLECTED", "action": "updated"}` |
| `id` exists with status `COLLECTED` | `409 Conflict` |
| Missing or invalid key | `401 Unauthorized` |
| Malformed payload | `422 Unprocessable Entity` |

### `GET /ready-queue`
Returns every order currently `READY`, oldest first. IDs are chronological timestamps, so a string sort is a time sort. Polled by the ESP32.

```json
[
  {"id": "260821143045", "cname": "Manaswi", "order_contents": [{"item": "Masala Dosa", "qty": 2}]}
]
```

### `GET /status/{order_id}`
Returns `{"id": "...", "status": "READY" | "COLLECTED"}`, or `404`. Polled by the student tracker.

The `404` is intentionally ambiguous: the order may not be ready yet, or the student may have mistyped the ID. Nothing exists in the database before the first scan, so the two cases cannot be told apart. The tracker words this honestly: "Not ready yet, or double-check your Order ID."

## Configuration

The backend reads three environment variables:

| Variable | Purpose |
|---|---|
| `TURSO_DATABASE_URL` | Turso database URL (`libsql://...`). The host is extracted and connected over HTTPS. |
| `TURSO_AUTH_TOKEN` | Turso auth token |
| `STAFF_API_KEY` | Access code staff enter on the scanner page. If unset, all staff-protected requests are rejected. |

If the Turso variables are not set, the backend falls back to a local SQLite file (`orders.db`), which is convenient for local development.

## Deployment (Render + Turso)

The service runs as a Render web service connected to a Turso database.

- Set the three environment variables above in the Render service settings.
- Install dependencies from `canteen-backend/requirements.txt`.
- Run the app from the `canteen-backend/` directory, for example:
  ```bash
  uvicorn main:app --host 0.0.0.0 --port $PORT
  ```
- The repository must be checked out with `frontend/` as a sibling of `canteen-backend/`, because the app mounts `../frontend` at `/app`.
- Render serves the app over HTTPS, which the staff phone's camera access requires.

### Local development

```bash
cd canteen-backend
python -m venv .venv
source .venv/bin/activate        # Windows: .venv\Scripts\activate
pip install -r requirements.txt
export STAFF_API_KEY=choose-a-code
uvicorn main:app --host 0.0.0.0 --port 8000 --reload
```

Browsers only allow camera access on HTTPS or `localhost`. To test the scanner from a phone against a local server, use an HTTPS tunnel (for example `ngrok http 8000`) or USB port forwarding (`adb reverse tcp:8000 tcp:8000`).

## Staff scanner (`frontend/staff.html`)

- Access-code gate: the code is verified against `/verify-staff-key` and held in `sessionStorage` for the session.
- Auto-submits every successful decode; there is no confirm button.
- Debounces repeat reads of the same QR within 3 seconds.
- Each result produces a full-screen colour flash (green, amber, or red) and a distinct vibration pattern, so outcomes are noticeable at a busy counter. Vibration works on Android only.
- Keeps a running log of recent scans.
- Prompts for the code again if the server answers `401`.

## Student tracker (`frontend/student.html`)

The student types the order ID printed on their slip. The page polls `/status/{id}` every 4 seconds and shows one of: not ready yet, ready to collect, already collected, or a connection error. Polling stops once the order is `COLLECTED`.

## Display board (`mcu1/`)

An ESP32 that pulls the ready queue from the backend and hosts its own status page on the local network.

- Fetches `/ready-queue` every 10 seconds, with a 5-second HTTP timeout.
- Shows up to 25 orders (`MAX_ORDERS`), oldest first. If more are ready, it truncates to the oldest 25.
- Serves the board at `/` and its current data at `/data`. The page refreshes every 5 seconds, auto-scrolls continuously, and rebuilds rows only when the data actually changes.
- Shows a freshness indicator: green up to 30 seconds since the last successful fetch, amber up to 60 seconds, red beyond that. It also shows when WiFi is disconnected, and keeps displaying the last known data.
- Reconnects to WiFi automatically.

Configuration lives in `mcu1/config.h`:

| Setting | Meaning |
|---|---|
| `USE_CLOUD_BACKEND` | `true` to use the Render host, `false` to use a local server |
| `BACKEND_CLOUD_HOST` | Hostname of the Render service |
| `BACKEND_LOCAL_HOST` / `BACKEND_LOCAL_PORT` | Local server address, used when the cloud flag is `false` |
| `FETCH_INTERVAL_MS`, `HTTP_TIMEOUT_MS`, `RECONNECT_INTERVAL_MS`, `MAX_ORDERS` | Timing and capacity |

WiFi credentials go in `mcu1/secrets.h` (`SECRET_SSID`, `SECRET_PASS`). Keep real credentials out of version control. Required Arduino libraries: ArduinoJson (v7 API), plus the ESP32 core's `WiFi`, `WebServer`, `HTTPClient`, and `WiFiClientSecure`.

## Test slip generator (`order_qr/`)

A standalone testing aid that stands in for the separate ordering system. It generates 80 mm receipt-format PDFs: a customer slip (no QR) and a kitchen slip carrying the QR that `/scan` expects. It never contacts the backend.

```bash
cd order_qr
pip install -r requirements.txt
python generate_slips.py                                                     # one random order
python generate_slips.py --name "Manaswi" --items "Masala Dosa:2,Filter Coffee:1"
python generate_slips.py --count 15                                          # random batch
```

Output is written to `output/`: per-order PDFs, an `orders.json` manifest, and merged `ALL_*` PDFs for batches. The QR uses compact JSON at high error correction so it stays scannable after printing.

## Project structure

```
.
├── canteen-backend/
│   ├── main.py            # FastAPI app and endpoints
│   ├── models.py          # Order table and request/response schemas
│   ├── database.py        # Turso / SQLite engine setup
│   └── requirements.txt
├── frontend/
│   ├── index.html         # Landing page
│   ├── staff.html         # QR scanner
│   ├── student.html       # Order tracker
│   └── lib/               # Local html5-qrcode bundle and CSS
├── mcu1/                  # ESP32 display board firmware
├── order_qr/              # Test slip generator
└── README.md
```

## Security notes and known limitations

- `POST /scan` and `/verify-staff-key` require the `X-Staff-Key` header, compared in constant time. A slowapi limit of 15 requests per minute is configured on this check.
- `GET /ready-queue` and `GET /status/{id}` are public and unthrottled. `/ready-queue` exposes customer names and order contents to anyone who can reach the service.
- CORS allows all origins.
- The ESP32 connects to the cloud backend with certificate validation disabled (`setInsecure()`), so the connection is encrypted but the server's identity is not verified.
- Orders are kept until they are cleared. The deployed app has no automatic daily reset: `canteen-backend/scheduler.py` contains a midnight-clear task, but `main.py` does not start it.
- The `404` from `/status/{id}` cannot distinguish "not ready" from "wrong ID" (see above).