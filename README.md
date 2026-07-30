# Canteen Order & Token Management System (Rassence Caterers)

> **Eliminating queue bottlenecks, manual inspection overhead, and counter rush for Rassence Caterers at VIT Bhopal.**

---

## Problem Overview

Rassence Caterers operates a pre-ordering system at the VIT Bhopal campus canteen. Students place orders ahead of time and receive a physical paper slip with a unique token or order number. 

However, the current food collection process relies entirely on a single staff member manually cross-checking paper slips against prepared food trays. This creates a severe operational throughput bottleneck that collapses during peak hours and batch food arrivals.

### Key Friction Points

* **Manual Search Overhead:** Staff must visually scan every prepared food tray one-by-one to locate a matching order number for each incoming slip.
* **Information Asymmetry:** Students have zero real-time visibility into whether their meal is ready, forcing them to crowd the counter and verbally disrupt staff for updates.
* **Batch-Arrival Chaos:** When fresh trays arrive from the kitchen, an unmanaged crowd rush ensues, overwhelming the single counter operator.
* **Single Point of Failure:** Assigning one staff member to handle verification, sorting, customer management, and handoff leads to extreme transaction latency and high fatigue.

---

## Operational Impact

* **Severe Latency:** Order handoff times spike dramatically during peak dining hours.
* **Crowd & Queue Stagnation:** Physical congestion blocks access paths and creates unnecessary noise and stress.
* **Staff Burnout & Errors:** Constant verbal interruptions increase cognitive load, leading to higher rates of order mix-ups and cooled food.

---

## Proposed Solution

A lightweight, real-time **Token Display & Verification System** designed to decouple food preparation from customer collection and remove manual searching from the handoff process.

### Core Features

* **Real-Time Order Status Board:** A digital display mounted above the counter (and accessible via mobile web) showing orders categorized under **Preparing** vs. **Ready for Pickup**.
* **Quick Verification (QR / Barcode):** Scanning a digital or printed barcode on the order slip validates pickup in under 2 seconds.
* **Batch Notification Alerts:** Audio/visual prompts notify students immediately when a batch of fresh food is logged into the ready queue.
* **Role-Based Interfaces:**
  * **Kitchen Dashboard:** Staff tap order numbers to push them from "Preparing" to "Ready".
  * **Counter Dispatcher:** Quick-scan verification screen for rapid order handoffs.
  * **Student Web Tracker:** Lightweight mobile interface allowing students to monitor order status from their tables.

---

## Workflow Architecture