import asyncio
from datetime import datetime, timedelta

from sqlmodel import Session, delete

from database import engine
from models import Order


async def clear_orders_at_midnight() -> None:
    """Runs forever in the background. Sleeps until the next local midnight,
    wipes every row in `orders`, then goes back to sleep for the next day.

    Deliberately not using a cron-style external scheduler or extra
    dependency (e.g. APScheduler) -- this system is meant to run
    unattended on a small local machine, so one plain asyncio loop that
    starts with the app and needs nothing else installed is the simplest
    thing that can't silently fail to be scheduled.
    """
    while True:
        now = datetime.now()
        next_midnight = (now + timedelta(days=1)).replace(
            hour=0, minute=0, second=0, microsecond=0
        )
        wait_seconds = (next_midnight - now).total_seconds()
        await asyncio.sleep(wait_seconds)

        with Session(engine) as session:
            session.exec(delete(Order))
            session.commit()

        print(f"[{datetime.now().isoformat()}] Daily clear executed - orders table wiped.")
