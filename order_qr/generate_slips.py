#!/usr/bin/env python3
"""
Test-order / slip generator — Rassence Caterers distribution system.

WHAT THIS IS
    A standalone testing aid that stands in for the real ordering system
    (which is being built separately and is out of scope here). It produces
    the two physical slips a real order would generate, so you can test the
    distribution pipeline end-to-end -- QR -> staff scanner -> backend ->
    ESP32 display / student tracker -- without waiting on that other system.

    - customer slip: order ID + contents, NO QR (matches the real design).
    - kitchen slip:  order ID + contents + a QR encoding exactly the JSON
      payload POST /scan expects: {"id", "cname", "order_contents"}.

    This script never talks to the backend or the real ordering system --
    it only produces PDFs. You scan the QR with the staff scanner
    (frontend/staff.html) just like you would a real printed slip.

USAGE
    # One random test order (default)
    python generate_slips.py

    # One specific order
    python generate_slips.py --name "Manaswi" --items "Masala Dosa:2,Filter Coffee:1"

    # Batch of random orders -- for stress-testing the ESP32 display /
    # ready-queue with volume, NOT a claim that real orders arrive in
    # batches. Always random (per-order --name/--items doesn't make sense
    # for a batch of different orders).
    python generate_slips.py --count 15

OUTPUT
    output/<order_id>_customer.pdf   -- one per order
    output/<order_id>_kitchen.pdf    -- one per order, has the QR
    output/orders.json               -- manifest of every payload generated
    output/ALL_customer_slips.pdf    -- only when --count > 1, all merged
    output/ALL_kitchen_slips.pdf     -- only when --count > 1, all merged

    Slips are sized for an 80mm receipt/thermal printer (the common format
    for kitchen order slips). Change RECEIPT_WIDTH below if your canteen
    uses a different printer/paper size.
"""
import argparse
import json
import os
import random
from datetime import datetime, timedelta

import qrcode
from pypdf import PdfReader, PdfWriter
from reportlab.lib.units import mm
from reportlab.pdfgen import canvas

# ---------------------------------------------------------------------------
# Test data pools -- purely for --random / batch generation. Not real data.
# ---------------------------------------------------------------------------
SAMPLE_NAMES = [
    "Aarav", "Vivaan", "Aditi", "Diya", "Kabir", "Meera", "Rohan", "Sanya",
    "Ishaan", "Anaya", "Arjun", "Priya", "Vihaan", "Kavya", "Reyansh", "Myra",
    "Manaswi", "Riya", "Aman", "Neha", "Karan", "Tanvi", "Yash", "Sara",
]

SAMPLE_MENU = [
    "Masala Dosa", "Plain Dosa", "Idli Sambar", "Veg Puff", "Samosa",
    "Chole Bhature", "Paneer Roll", "Veg Sandwich", "Maggi", "French Fries",
    "Cold Coffee", "Filter Coffee", "Masala Chai", "Lemon Soda", "Lassi",
    "Veg Fried Rice", "Paneer Wrap", "Chicken Roll", "Chowmein", "Momos",
]

# 80mm is the standard width for receipt/thermal kitchen-slip printers.
RECEIPT_WIDTH = 80 * mm
MARGIN = 5 * mm
QR_SIZE = 30 * mm  # large enough to stay reliably scannable after printing


# ---------------------------------------------------------------------------
# Order generation
# ---------------------------------------------------------------------------
def make_order_id(base_time: datetime, offset_seconds: int = 0) -> str:
    """YYMMDDHHMMSS -- identical format/contract to canteen-backend.
    Offsetting by whole seconds per order guarantees uniqueness in a batch,
    same as the real system relies on (no two orders share a second)."""
    return (base_time + timedelta(seconds=offset_seconds)).strftime("%y%m%d%H%M%S")


def random_items() -> list[dict]:
    n = random.randint(1, 3)
    chosen = random.sample(SAMPLE_MENU, n)
    return [{"item": it, "qty": random.randint(1, 3)} for it in chosen]


def parse_items(items_str: str) -> list[dict]:
    """'Masala Dosa:2,Filter Coffee:1' -> [{"item": "Masala Dosa", "qty": 2}, ...]"""
    items = []
    for part in items_str.split(","):
        part = part.strip()
        if not part:
            continue
        name, _, qty = part.rpartition(":")
        if not name or not qty.strip().isdigit():
            raise ValueError(f"Bad --items entry: '{part}' (expected 'Item Name:qty')")
        items.append({"item": name.strip(), "qty": int(qty)})
    if not items:
        raise ValueError("--items produced no valid entries")
    return items


def random_order(order_id: str) -> dict:
    return {
        "id": order_id,
        "cname": random.choice(SAMPLE_NAMES),
        "order_contents": random_items(),
    }


# ---------------------------------------------------------------------------
# QR
# ---------------------------------------------------------------------------
def make_qr_image(order: dict):
    """Encodes exactly the /scan payload shape, compact (no spaces) to keep
    the QR as low-density (and therefore as scan-reliable) as possible.
    High error correction so creases/print smudges on receipt paper don't
    break the scan."""
    data = json.dumps(
        {"id": order["id"], "cname": order["cname"], "order_contents": order["order_contents"]},
        separators=(",", ":"),
    )
    qr = qrcode.QRCode(error_correction=qrcode.constants.ERROR_CORRECT_H, box_size=10, border=2)
    qr.add_data(data)
    qr.make(fit=True)
    return qr.make_image(fill_color="black", back_color="white").convert("RGB")


# ---------------------------------------------------------------------------
# Slip rendering (both 80mm receipt-style, print-ready)
# ---------------------------------------------------------------------------
def draw_customer_slip(path: str, order: dict) -> None:
    items = order["order_contents"]
    height = 46 * mm + len(items) * 6 * mm
    c = canvas.Canvas(path, pagesize=(RECEIPT_WIDTH, height))
    y = height - MARGIN

    c.setFont("Helvetica-Bold", 13)
    c.drawCentredString(RECEIPT_WIDTH / 2, y, "RASSENCE CATERERS")
    y -= 7 * mm

    c.setFont("Helvetica", 8)
    c.drawCentredString(RECEIPT_WIDTH / 2, y, "Your Order")
    y -= 8 * mm

    c.setFont("Helvetica-Bold", 11)
    c.drawCentredString(RECEIPT_WIDTH / 2, y, f"Order #{order['id']}")
    y -= 6 * mm

    c.setFont("Helvetica", 9)
    c.drawCentredString(RECEIPT_WIDTH / 2, y, order["cname"])
    y -= 6 * mm

    c.line(MARGIN, y, RECEIPT_WIDTH - MARGIN, y)
    y -= 6 * mm

    c.setFont("Helvetica", 9)
    for it in items:
        c.drawString(MARGIN, y, it["item"])
        c.drawRightString(RECEIPT_WIDTH - MARGIN, y, f"x{it['qty']}")
        y -= 6 * mm

    y -= 2 * mm
    c.line(MARGIN, y, RECEIPT_WIDTH - MARGIN, y)
    y -= 6 * mm

    c.setFont("Helvetica-Oblique", 7)
    c.drawCentredString(RECEIPT_WIDTH / 2, y, "Track status on the canteen tracker page")
    y -= 4 * mm
    c.drawCentredString(RECEIPT_WIDTH / 2, y, "Keep this slip until you collect your order")

    c.save()


def draw_kitchen_slip(path: str, order: dict) -> None:
    items = order["order_contents"]
    height = 34 * mm + len(items) * 7 * mm + QR_SIZE + 12 * mm
    c = canvas.Canvas(path, pagesize=(RECEIPT_WIDTH, height))
    y = height - MARGIN

    c.setFont("Helvetica-Bold", 12)
    c.drawCentredString(RECEIPT_WIDTH / 2, y, "KITCHEN COPY")
    y -= 7 * mm

    c.setFont("Helvetica-Bold", 10)
    c.drawCentredString(RECEIPT_WIDTH / 2, y, f"#{order['id']}  \u2014  {order['cname']}")
    y -= 6 * mm

    c.line(MARGIN, y, RECEIPT_WIDTH - MARGIN, y)
    y -= 7 * mm

    c.setFont("Helvetica-Bold", 11)
    for it in items:
        c.drawString(MARGIN, y, it["item"])
        c.drawRightString(RECEIPT_WIDTH - MARGIN, y, f"x{it['qty']}")
        y -= 7 * mm

    y -= 2 * mm
    c.line(MARGIN, y, RECEIPT_WIDTH - MARGIN, y)
    y -= 6 * mm

    # QR -- rendered to a temp PNG, embedded, then cleaned up.
    qr_img = make_qr_image(order)
    qr_tmp_path = path + ".qr.png"
    qr_img.save(qr_tmp_path)
    qr_x = (RECEIPT_WIDTH - QR_SIZE) / 2
    y -= QR_SIZE
    c.drawImage(qr_tmp_path, qr_x, y, width=QR_SIZE, height=QR_SIZE)
    y -= 5 * mm

    c.setFont("Helvetica-Oblique", 6)
    c.drawCentredString(RECEIPT_WIDTH / 2, y, "Scan when READY. Scan again at HANDOFF.")

    c.save()
    os.remove(qr_tmp_path)


def merge_pdfs(paths: list[str], out_path: str) -> None:
    writer = PdfWriter()
    for p in paths:
        reader = PdfReader(p)
        for page in reader.pages:
            writer.add_page(page)
    with open(out_path, "wb") as f:
        writer.write(f)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------
def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate test customer/kitchen slips (with QR) for the distribution system.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument("--name", help="Customer name for a single order (default: random).")
    parser.add_argument(
        "--items",
        help="Single order's items, e.g. 'Masala Dosa:2,Filter Coffee:1' (default: random).",
    )
    parser.add_argument(
        "--count",
        type=int,
        default=1,
        help="How many test orders to generate (default 1). >1 is always random -- "
        "for loading up the ready-queue/display with volume, not a claim real "
        "orders arrive in batches.",
    )
    parser.add_argument("--outdir", default="output", help="Output directory (default: ./output).")
    args = parser.parse_args()

    if args.count > 1 and (args.name or args.items):
        parser.error("--name/--items only apply with --count 1. For --count > 1, orders are random.")
    if args.count < 1:
        parser.error("--count must be at least 1.")

    os.makedirs(args.outdir, exist_ok=True)
    base_time = datetime.now()
    orders = []

    for i in range(args.count):
        order_id = make_order_id(base_time, offset_seconds=i)

        if args.count == 1 and (args.name or args.items):
            order = {
                "id": order_id,
                "cname": args.name or random.choice(SAMPLE_NAMES),
                "order_contents": parse_items(args.items) if args.items else random_items(),
            }
        else:
            order = random_order(order_id)

        orders.append(order)

        cpath = os.path.join(args.outdir, f"{order_id}_customer.pdf")
        kpath = os.path.join(args.outdir, f"{order_id}_kitchen.pdf")
        draw_customer_slip(cpath, order)
        draw_kitchen_slip(kpath, order)
        print(f"[{i + 1}/{args.count}] {order['cname']} - #{order_id} -> {cpath}, {kpath}")

    manifest_path = os.path.join(args.outdir, "orders.json")
    with open(manifest_path, "w") as f:
        json.dump(orders, f, indent=2)
    print(f"\nManifest (all payloads): {manifest_path}")

    if args.count > 1:
        kitchen_all = os.path.join(args.outdir, "ALL_kitchen_slips.pdf")
        customer_all = os.path.join(args.outdir, "ALL_customer_slips.pdf")
        merge_pdfs([os.path.join(args.outdir, f"{o['id']}_kitchen.pdf") for o in orders], kitchen_all)
        merge_pdfs([os.path.join(args.outdir, f"{o['id']}_customer.pdf") for o in orders], customer_all)
        print(f"Merged for one-shot printing: {kitchen_all}, {customer_all}")


if __name__ == "__main__":
    main()
