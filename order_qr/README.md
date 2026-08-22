# Test Order / Slip Generator

A **testing aid**, not part of the production system. Stands in for the real
ordering system (built separately, out of scope here) so you can test the
distribution pipeline — QR → staff scanner → backend → ESP32 display /
student tracker — without waiting on that other system to exist.

It produces the same two physical slips a real order generates:

- **Customer slip** — order ID + contents, **no QR** (matches production).
- **Kitchen slip** — order ID + contents + a QR encoding exactly the JSON
  `POST /scan` expects: `{"id": ..., "cname": ..., "order_contents": [...]}`.

This script never talks to the backend or the ordering system — it only
produces PDFs. To actually test the flow, open `frontend/staff.html` and
scan the QR on a printed (or on-screen) kitchen slip, same as a real one.

## Setup

```bash
pip install -r requirements.txt
```

## Usage

```bash
# One random test order
python generate_slips.py

# One specific order
python generate_slips.py --name "Manaswi" --items "Masala Dosa:2,Filter Coffee:1"

# Batch of 15 random orders — for loading up the ready-queue/ESP32 display
# with volume to see how it behaves, NOT a claim that real orders arrive in
# batches. --count > 1 is always random (per-order --name/--items on a batch
# of different orders doesn't make sense).
python generate_slips.py --count 15
```

## Output (in `output/`)

- `<order_id>_customer.pdf` — one per order
- `<order_id>_kitchen.pdf` — one per order, has the QR
- `orders.json` — manifest of every payload generated this run (handy for
  debugging / feeding into other tests)
- `ALL_customer_slips.pdf`, `ALL_kitchen_slips.pdf` — only with `--count > 1`,
  every slip merged into one file each so you can print a batch in one shot

## Design notes

- **Slip size**: 80mm width, the standard for receipt/thermal kitchen-slip
  printers. If Rassence uses a different printer/paper, change
  `RECEIPT_WIDTH` (and `QR_SIZE`) at the top of `generate_slips.py`.
- **QR encoding**: compact JSON (no whitespace) at high error correction
  (`ERROR_CORRECT_H`), sized generously (30mm) — chosen so the QR stays
  reliably scannable even after printing, creasing, or minor print smudging
  on receipt paper.
- **Order IDs**: same `YYMMDDHHMMSS` format as the backend. Batch orders get
  offset by whole seconds to guarantee uniqueness, same assumption the real
  system relies on (confirmed: no two real orders land in the same second).
  Running the script twice within the same wall-clock second would collide
  and overwrite — a non-issue in practice for manual test runs.

## Verified

The full pipeline was tested end-to-end: generate a kitchen slip → render
the PDF to an image at 300dpi (as if printed and photographed) → decode the
QR → confirm the decoded JSON matches both the run's manifest and the exact
shape `ScanPayload` in `canteen-backend/models.py` expects
(`id`, `cname`, `order_contents: [{item, qty}]`).
