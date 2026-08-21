from typing import List

from sqlmodel import SQLModel, Field


class OrderItem(SQLModel):
    """One line item inside an order. Not a DB table on its own --
    a list of these gets stored as a JSON string inside Order.order_contents."""
    item: str
    qty: int


class ScanPayload(SQLModel):
    """Shape of the JSON that comes off the QR code and is POSTed to /scan.
    Field names intentionally match the DB columns 1:1 so there's no
    translation needed between 'what the QR says' and 'what we store'."""
    id: str
    cname: str
    order_contents: List[OrderItem]


class Order(SQLModel, table=True):
    """The one and only table. A row is created on the FIRST scan (food ready)
    and updated (never re-created) on the SECOND scan (handoff)."""
    id: str = Field(primary_key=True)          # YYMMDDHHMMSS, taken as-is from the QR
    cname: str
    order_contents: str                         # JSON string: [{"item": "...", "qty": N}, ...]
    status: str                                  # "READY" or "COLLECTED"


class OrderPublic(SQLModel):
    """What /ready-queue returns -- order_contents as real JSON (list),
    not the raw string SQLite stores it as."""
    id: str
    cname: str
    order_contents: List[OrderItem]


class StatusPublic(SQLModel):
    """What /status/{id} returns for the student tracker's polling."""
    id: str
    status: str
