import asyncio
import json
import os
import secrets
from contextlib import asynccontextmanager
from typing import List

from fastapi import FastAPI, HTTPException, Depends, Header, Request
from fastapi.middleware.cors import CORSMiddleware
from slowapi import Limiter, _rate_limit_exceeded_handler
from slowapi.errors import RateLimitExceeded
from slowapi.util import get_remote_address
from sqlmodel import Session, select
from fastapi.staticfiles import StaticFiles

from database import engine, init_db
from models import Order, OrderPublic, ScanPayload, StatusPublic


@asynccontextmanager
async def lifespan(app: FastAPI):
    # Startup: make sure the table exists, then kick off the daily-clear
    # background task for the lifetime of the app.
    init_db()
    
    yield
    # Shutdown: stop the background task cleanly.
    


app = FastAPI(title="Rassence Caterers - Token Distribution System", lifespan=lifespan)

limiter = Limiter(key_func=get_remote_address)
app.state.limiter = limiter
app.add_exception_handler(RateLimitExceeded, _rate_limit_exceeded_handler)

# Wide open CORS: the student tracker (plain HTML/JS served from wherever)
# and the staff scanner are both on the local canteen network, not behind
# any auth yet (by design, for now). Tighten this later if auth is added.
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)
app.mount("/app", StaticFiles(directory="../frontend", html=True), name="frontend")

STAFF_API_KEY = os.getenv("STAFF_API_KEY", "")


def verify_staff_key(x_staff_key: str = Header(default="")):
    if not STAFF_API_KEY or not secrets.compare_digest(x_staff_key, STAFF_API_KEY):
        raise HTTPException(status_code=401, detail="Invalid or missing staff key")

@app.get("/verify-staff-key", dependencies=[Depends(verify_staff_key)])
@limiter.limit("5/minute")
def verify_key(request: Request):
    """Endpoint used by staff.html to test if an access key is valid."""
    return {"status": "ok"}

@app.post("/scan", dependencies=[Depends(verify_staff_key)])
@limiter.limit("30/minute")
def scan(request: Request, payload: ScanPayload):
    """
    Single endpoint for BOTH scans of the same physical QR code:

    - id NOT in DB yet -> this is the "food is ready" scan. Create the row,
      pulled entirely from the QR payload (order-taking system is fully
      separate; we have zero prior knowledge of this order).
    - id in DB with status READY -> this is the handoff scan. Flip to COLLECTED.
    - id in DB with status COLLECTED already -> shouldn't happen in normal
      operation (collected slips get trashed), but guarded against with a
      409 rather than silently no-op-ing or crashing.
    """
    with Session(engine) as session:
        order = session.get(Order, payload.id)

        if order is None:
            new_order = Order(
                id=payload.id,
                cname=payload.cname,
                order_contents=json.dumps([item.model_dump() for item in payload.order_contents]),
                status="READY",
            )
            session.add(new_order)
            session.commit()
            return {"id": payload.id, "status": "READY", "action": "created"}

        if order.status == "READY":
            order.status = "COLLECTED"
            session.add(order)
            session.commit()
            return {"id": order.id, "status": "COLLECTED", "action": "updated"}

        raise HTTPException(
            status_code=409,
            detail=f"Order {payload.id} is already COLLECTED.",
        )

#esp32 poll logic
@app.get("/ready-queue", response_model=List[OrderPublic])
def ready_queue():
    """Polled by the ESP32. Every order currently READY, oldest first
    (id is YYMMDDHHMMSS, so a plain string sort on id IS chronological order)."""
    with Session(engine) as session:
        orders = session.exec(
            select(Order).where(Order.status == "READY").order_by(Order.id)
        ).all()
        return [
            OrderPublic(
                id=o.id,
                cname=o.cname,
                order_contents=json.loads(o.order_contents),
            )
            for o in orders
        ]

#student web tracker logic
@app.get("/status/{order_id}", response_model=StatusPublic)
def get_status(order_id: str):
    """Polled by the student web tracker every 3-5s.

    404 covers two indistinguishable cases by design (agreed for v1):
    the order genuinely isn't ready yet, OR the student mistyped their ID.
    We have no way to tell these apart since nothing exists in the DB
    before the first scan -- the tracker UI should phrase this honestly
    (e.g. "Not ready yet - check your ID") rather than implying a
    definite "still preparing" state.
    """
    with Session(engine) as session:
        order = session.get(Order, order_id)
        if order is None:
            raise HTTPException(status_code=404, detail="Order not found")
        return StatusPublic(id=order.id, status=order.status)


if __name__ == "__main__":
    import uvicorn

    # host="0.0.0.0" so the staff phone / ESP32 / tracker devices on the
    # same canteen WiFi can reach this machine by its local IP.
    uvicorn.run("main:app", host="0.0.0.0", port=8000, reload=True)
