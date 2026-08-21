# PROJECT HANDOFF — Canteen Token & Distribution System (Rassence Caterers, VIT Bhopal)

**Purpose of this file:** you are a Claude instance in a teammate's account with
no memory of the conversation that produced this project. This document gives
you everything discussed and decided so far, so you can continue the work
consistently instead of re-deriving or contradicting earlier decisions.
If the person you're talking to asks you to change an established decision
below, that's fine — just flag that it's a change from what was previously
agreed, don't silently override it.

---

## 1. Who's building this, and why

Team of students building a real-world system for Rassence Caterers, the
canteen operator at VIT Bhopal. Two-month timeline, prototype now with intent
to make it production-worthy. Team background: hardware/embedded systems, no
prior web dev experience, but comfortable with Python.

**Problem:** Rassence's canteen uses a pre-order system — students order
ahead, get a paper slip. Currently, one staff member manually cross-checks
paper slips against food trays to hand out orders. This collapses during
peak hours: staff must visually scan every tray for a matching slip, students
have no visibility into order status so they crowd the counter, and fresh
batches of food arriving cause crowd surges. One person handling verification
+ sorting + customer management = high latency, staff fatigue, order mix-ups.

## 2. What we're building vs. NOT building

**In scope (us):** the *distribution* side only — everything from "food is
ready" through "customer collects it." A backend server, a staff phone QR
scanner, a student-facing web status tracker, and an ESP32-driven LCD display
near the counter.

**Explicitly NOT in scope:** the *ordering* system (how students place
orders, pay, and how slips/QR codes get generated and printed in the first
place) is a **completely separate system**, built by someone else. Our
backend has **zero knowledge of any order until its QR is scanned** — there
is no pre-existing "PLACED" state in our database. Do not build order-taking,
payment, or QR-generation features into this backend; if asked, treat that as
a different system's job.

## 3. The physical / workflow flow (agreed, don't relitigate without cause)

1. Customer places an order (name + items) in the separate ordering system.
2. Two physical slips print:
   - **Customer's slip:** just the order ID + order contents as text (+ any
     decoration/branding). **No QR on this slip.**
   - **Caterer's/kitchen's slip:** items printed as text for the cook to
     read, **plus one QR code** encoding `{id, cname, order_contents}`. This
     slip stays with the food tray.
3. **First scan** (kitchen slip's QR, when food is ready): staff scans it →
   this is what *creates* the row in our DB, populated entirely from the QR
   payload → status becomes `READY` → shows up on ESP32 display and is
   pollable by the student tracker.
4. Customer arrives, hands over their own (QR-less) slip, staff visually
   matches it against the tray/kitchen slip.
5. **Second scan** — staff scans the **same physical QR again** → status
   flips to `COLLECTED`. One endpoint (`/scan`) handles both transitions;
   which one happens is inferred from whether the ID already exists in the
   DB and what its current status is.
6. Order IDs are timestamps, format `YYMMDDHHMMSS`. Confirmed by the team:
   no two orders will ever be placed in the same second, so this is safe as
   a primary key with no collision risk (do not "fix" this with added entropy
   unless the team says otherwise).
7. **Zero order history is retained.** Both physical slips get trashed once
   a customer collects their order, so there's no need to keep completed
   orders around. The entire `orders` table is wiped every night at midnight.
8. Student tracker identifies orders by the student **manually typing in
   their order ID** (v1 — no QR on the customer slip, may change later).

## 4. Architecture decisions (agreed, with reasoning)

- **All communication routes through the central backend** — no direct
  phone-to-ESP32 connection. This was an early correction specifically
  because it makes each component independently testable.
- **Stack:** FastAPI backend, SQLModel ORM, **local SQLite** (not cloud
  Postgres). Decision reasoning: this system should run on a small machine
  physically at the canteen (Raspberry Pi / mini-PC / old laptop) on the
  local WiFi, not on free-tier cloud hosting — free-tier disks are often
  ephemeral and would risk losing in-progress orders on a mid-day restart.
  Since the DB is wiped daily anyway, local SQLite is simpler and removes
  the internet-dependency risk entirely.
- **Student tracker:** plain HTML/CSS/vanilla JS, polling `/status/{id}`
  every 3–5 seconds. Deliberately NOT WebSockets for v1 — team is new to web
  dev, keep it simple. (WebSockets are a considered, deferred v2 idea.)
- **ESP32:** HTTP GET polling `/ready-queue` using the ArduinoJson library.
  Plays to the team's existing embedded skills. ESP32 firmware itself is not
  yet built — planned to show customer name + order contents (confirmed,
  not just the ID).
- **No auth** on any component for v1 (open on trusted local canteen
  network) — explicitly deferrable, add later if needed. CORS is
  wide-open (`*`) to match.
- **Ambiguous 404 accepted for v1:** `/status/{id}` returns 404 both when an
  order genuinely isn't ready yet AND when a student mistyped their ID —
  there's no way to distinguish these since nothing exists in the DB before
  the first scan. Team explicitly signed off on this being fine for now; the
  tracker UI should phrase it honestly ("Not ready yet — check your ID")
  rather than implying a definite state.
- **Duplicate/already-collected scan:** returns `409 Conflict`. Shouldn't
  happen in normal operation since collected slips get trashed immediately,
  but implemented as a safety guard rather than silently no-op'ing or
  crashing.

## 5. Backend status: BUILT AND SMOKE-TESTED (v1 complete)

Location (as delivered to the user): a `canteen-backend/` folder containing:

- **`main.py`** — FastAPI app. Three endpoints:
  - `POST /scan` — takes `{id, cname, order_contents: [{item, qty}]}`
    (exactly the QR payload shape). Creates row with `status=READY` if `id`
    unseen; flips `READY → COLLECTED` if seen and still `READY`; returns
    `409` if already `COLLECTED`.
  - `GET /ready-queue` — all `READY` orders, sorted oldest-first (plain
    string sort on `id` works since `YYMMDDHHMMSS` is already chronological).
  - `GET /status/{id}` — current status, or `404`.
  - Uses FastAPI's `lifespan` context manager to init the DB table and kick
    off the midnight-clear background task on startup.
- **`models.py`** — SQLModel table `Order` (id PK, cname, order_contents as
  JSON-string TEXT column, status) plus separate Pydantic-style schemas for
  the incoming QR payload and outgoing API responses (order_contents is a
  real JSON list in API responses, not the raw stored string).
- **`database.py`** — SQLite engine (`orders.db` local file), session
  dependency.
- **`scheduler.py`** — background `asyncio` task (no external scheduler
  dependency) that sleeps until local midnight, wipes the `orders` table,
  repeats. Runs for the lifetime of the app process.
- **`requirements.txt`** — fastapi, uvicorn[standard], sqlmodel.
- **`smoke_test.py`** — 14-assertion end-to-end test using FastAPI's
  `TestClient` (used as a context manager, which is required for
  `lifespan` startup to fire and create tables — noted here because it
  tripped up the first test run). Covers: unknown-id 404, first scan
  create, ready-queue contents, status after ready, second scan
  collect, ready-queue empties after collect, status after collect,
  409 on third scan, multi-order sort ordering, and 422 on malformed
  payload. **All 14 passed**, both via `TestClient` and via a live
  `uvicorn` server hit with real `curl` requests.
- **`README.md`** — setup/run instructions, API reference, notes on
  finding/pinning the local IP so staff phone / ESP32 / tracker devices on
  canteen WiFi can reach the host machine.
- **`.gitignore`** — excludes `orders.db`, `__pycache__/`, `server.log`, etc.

**Known caveat documented in the README:** if the host machine is off at
midnight, the daily clear (an in-process `asyncio` task) won't fire until the
app is next started — not a cron job, so it depends on the process being
alive at midnight. Acceptable for v1; flagged, not yet addressed.

## 6. Not yet built (next steps, in the order they were being considered)

- **ESP32 firmware** — polls `GET /ready-queue`, displays customer name +
  order contents on an LCD near the counter.
- **Staff phone QR scanner** — whatever scans the kitchen slip's QR and
  POSTs the decoded JSON to `/scan`. Not yet decided whether this is a
  native app, a mobile web page with camera access, or something else —
  ask the team if this comes up.
- **Student web tracker frontend** — plain HTML/CSS/vanilla JS, polls
  `GET /status/{id}` every 3–5s, student manually types in their order ID.

## 7. How to work with this team

- They explicitly want to be asked clarifying questions before being given
  answers/solutions — don't guess and fill gaps silently, especially on
  workflow/architecture decisions. This has been the working style for the
  whole project so far.
- They're comfortable with Python, new to web dev — lean on SQLModel/FastAPI
  patterns that reduce boilerplate (e.g. one class doing double duty as
  Pydantic validation + DB table) rather than assuming deep web framework
  familiarity.
- Prefer verifying code actually runs (smoke tests, live server + curl)
  before considering something "done," rather than just producing code that
  looks right.
