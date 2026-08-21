# Canteen Token Distribution System — Backend

FastAPI + SQLite backend for the Rassence Caterers token distribution system.
This is **only** the distribution side (kitchen-ready → collected). Order-taking
and QR/slip generation are a separate system and out of scope here.

## What this is

- One table, `orders`, with **no history** — a row is created on the first
  scan (food ready) and updated in place on the second scan (handoff), then
  wiped every night at midnight.
- Meant to run on a small machine physically at the canteen (Raspberry Pi,
  mini-PC, old laptop) on the local WiFi — not on cloud hosting. No internet
  dependency for core operation, and no risk of a cloud free-tier disk wipe
  losing an in-progress order.

## Setup

```bash
python -m venv .venv
source .venv/bin/activate      # Windows: .venv\Scripts\activate
pip install -r requirements.txt
```

## Running

```bash
uvicorn main:app --host 0.0.0.0 --port 8000 --reload
```

`--host 0.0.0.0` is required (not `127.0.0.1`) so the staff phone, ESP32, and
any student-tracker devices on the same canteen WiFi can reach this machine
by its local IP, e.g. `http://192.168.1.42:8000`.

**Finding that IP:** run `ip addr` (Linux) / `ipconfig` (Windows) on the host
machine. For it not to change on reboot, set a static IP or DHCP reservation
on your router for this machine — otherwise every client (scanner, ESP32,
tracker) will need reconfiguring whenever the IP changes.

A SQLite file `orders.db` is created automatically on first run, in the same
folder. Deleting it (or restarting at midnight, which the app does
automatically) gives you a clean slate.

## Testing it

```bash
python smoke_test.py
```

Runs the full flow (create → ready-queue → handoff → status → duplicate-scan
guard → sort order → malformed-payload handling) against an in-memory test
client — no server needs to be running. All 14 checks should print `PASS`.

## API

### `POST /scan`
Single endpoint for **both** scans of the same physical QR code (kitchen slip).

Request body — exactly what's encoded in the QR:
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

- `id` not in DB → **creates** the row, `status: READY` (this is the "food's
  ready" scan — the row's entire content comes from the QR, since nothing
  is known about the order beforehand).
- `id` in DB with `status: READY` → **updates** to `status: COLLECTED`
  (handoff scan).
- `id` in DB already `COLLECTED` → `409 Conflict`. Shouldn't happen in normal
  operation (collected slips get trashed) — this is just a guard rail.

```bash
curl -X POST http://localhost:8000/scan \
  -H "Content-Type: application/json" \
  -d '{"id":"260821143045","cname":"Manaswi","order_contents":[{"item":"Masala Dosa","qty":2}]}'
```

### `GET /ready-queue`
Polled by the ESP32. Every currently-`READY` order, oldest first (a plain
string sort on `id` works because the ID format `YYMMDDHHMMSS` is already
chronological).

```json
[
  {"id": "260821143045", "cname": "Manaswi", "order_contents": [{"item": "Masala Dosa", "qty": 2}]}
]
```

### `GET /status/{id}`
Polled by the student web tracker every 3–5s.

- Found → `{"id": "...", "status": "READY"}` or `"COLLECTED"`
- Not found → `404`. **By design**, this covers two cases we can't tell
  apart: the order genuinely isn't ready yet, or the student mistyped their
  ID. The tracker's UI should phrase this honestly (e.g. "Not ready yet —
  double check your ID") rather than implying a definite "still preparing"
  state.

## Daily reset

A background task (in `scheduler.py`) sleeps until local midnight, wipes the
`orders` table, and repeats — no cron or external scheduler needed. It starts
automatically with the app and needs nothing extra installed. As long as the
process is running at midnight, it fires; if the machine happens to be off at
midnight, the table just carries yesterday's leftovers until the app is next
started (at which point you can also just delete `orders.db` manually if
needed).

## Not in this repo yet

- ESP32 firmware (polls `/ready-queue`, displays name + order — planned for
  later per your notes)
- Staff phone scanner (whatever QR-scanning app/webpage POSTs to `/scan`)
- Student web tracker frontend (plain HTML/CSS/JS polling `/status/{id}`)
- Auth on `/scan` — intentionally open for now (trusted local network), CORS
  is wide open (`*`) to match. Tighten both if/when auth is added.
