"""
Quick end-to-end smoke test using FastAPI's TestClient (no real server/network
needed). Run with: python smoke_test.py
Deletes any existing orders.db first so this is repeatable.
"""
import os
import sys

# Fresh DB for a clean test run
if os.path.exists("orders.db"):
    os.remove("orders.db")

from fastapi.testclient import TestClient
from main import app

# Using TestClient as a context manager is required here -- it's what
# actually triggers FastAPI's `lifespan` startup (which creates the table).
# Without the `with`, requests would hit a DB with no table yet.
client = TestClient(app).__enter__()

PAYLOAD = {
    "id": "260821143045",
    "cname": "Manaswi",
    "order_contents": [
        {"item": "Masala Dosa", "qty": 2},
        {"item": "Filter Coffee", "qty": 1},
    ],
}

def check(label, cond):
    status = "PASS" if cond else "FAIL"
    print(f"[{status}] {label}")
    if not cond:
        sys.exit(1)

# 1. Status of an order that doesn't exist yet -> 404
r = client.get(f"/status/{PAYLOAD['id']}")
check("unknown id -> 404 on /status", r.status_code == 404)

# 2. First scan (food ready) -> creates row, status READY
r = client.post("/scan", json=PAYLOAD)
check("first scan -> 200", r.status_code == 200)
check("first scan -> action=created, status=READY", r.json() == {"id": PAYLOAD["id"], "status": "READY", "action": "created"})

# 3. Ready queue should now contain this order, with contents as real JSON
r = client.get("/ready-queue")
check("ready-queue -> 200", r.status_code == 200)
queue = r.json()
check("ready-queue contains exactly 1 order", len(queue) == 1)
check("ready-queue order matches payload", queue[0]["id"] == PAYLOAD["id"] and queue[0]["cname"] == PAYLOAD["cname"] and queue[0]["order_contents"] == PAYLOAD["order_contents"])

# 4. Status should now be READY
r = client.get(f"/status/{PAYLOAD['id']}")
check("status after first scan -> READY", r.status_code == 200 and r.json()["status"] == "READY")

# 5. Second scan (handoff) -> flips to COLLECTED
r = client.post("/scan", json=PAYLOAD)
check("second scan -> 200", r.status_code == 200)
check("second scan -> action=updated, status=COLLECTED", r.json() == {"id": PAYLOAD["id"], "status": "COLLECTED", "action": "updated"})

# 6. Ready queue should now be empty (order no longer READY)
r = client.get("/ready-queue")
check("ready-queue empty after collection", r.json() == [])

# 7. Status should now be COLLECTED
r = client.get(f"/status/{PAYLOAD['id']}")
check("status after second scan -> COLLECTED", r.status_code == 200 and r.json()["status"] == "COLLECTED")

# 8. Third scan on an already-COLLECTED order -> 409 guard rail
r = client.post("/scan", json=PAYLOAD)
check("third scan on COLLECTED order -> 409", r.status_code == 409)

# 9. A second, different order to confirm ready-queue ordering (oldest id first)
payload2 = {
    "id": "260821143010",  # earlier timestamp than PAYLOAD's id
    "cname": "Riya",
    "order_contents": [{"item": "Poha", "qty": 1}],
}
client.post("/scan", json=payload2)
payload3 = {
    "id": "260821143050",  # later than both
    "cname": "Aman",
    "order_contents": [{"item": "Samosa", "qty": 3}],
}
client.post("/scan", json=payload3)
r = client.get("/ready-queue")
ids_in_order = [o["id"] for o in r.json()]
check("ready-queue sorted oldest-id first", ids_in_order == sorted(ids_in_order))

# 10. Malformed payload (missing cname) -> 422 validation error, not a 500
r = client.post("/scan", json={"id": "x", "order_contents": []})
check("malformed payload -> 422 (not 500)", r.status_code == 422)

print("\nAll smoke tests passed.")
