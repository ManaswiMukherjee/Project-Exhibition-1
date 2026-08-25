#ifndef INDEX_H
#define INDEX_H

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Display Board</title>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <style>
    body { 
      background-color: #0b0e14; 
      color: #e6edf3; 
      font-family: monospace, sans-serif; 
      padding: 20px; 
      text-align: center; 
      overflow: hidden; 
    }
    
    h1 { 
      color: #f2a900; 
      font-size: 2.5rem; 
      letter-spacing: 3px; 
      margin-bottom: 10px; 
      text-transform: uppercase; 
    }

    #statusIndicator {
      font-size: 0.9rem;
      color: #8b949e;
      margin-bottom: 15px;
    }

    #statusDot {
      display: inline-block;
      width: 10px;
      height: 10px;
      border-radius: 50%;
      background-color: #8b949e;
      margin-right: 6px;
      vertical-align: middle;
    }
    
    .scroll-container {
      height: 75vh;
      overflow-y: hidden;
      border: 1px solid #30363d;
      border-radius: 8px;
    }

    table { 
      width: 100%; 
      border-collapse: collapse; 
      font-size: 1.5rem; 
    }

    th, td { 
      padding: 18px; 
      border-bottom: 1px solid #30363d; 
      text-align: left; 
    }

    th { 
      position: sticky; 
      top: 0; 
      background-color: #161b22; 
      color: #8b949e; 
      text-transform: uppercase; 
      letter-spacing: 1px; 
      z-index: 10; 
      box-shadow: 0 2px 5px rgba(0,0,0,0.5);
    }

    /* Fixed Alternating Row Styles */
    .row-dark { background-color: #161b22; }
    .row-light { background-color: #0b0e14; }
  </style>
</head>
<body>
  <h1>LIVE STATUS BOARD</h1>

  <div id="statusIndicator">
    <span id="statusDot"></span>
    <span id="statusText">Connecting...</span>
  </div>
  
  <div class="scroll-container" id="boardContainer">
    <table>
      <thead>
        <tr><th>Name</th><th>Order</th></tr>
      </thead>
      <tbody id="tableBody">
        <tr class="row-dark"><td colspan="2" style="text-align:center; color:#8b949e;">Loading orders...</td></tr>
      </tbody>
    </table>
  </div>

  <script>
    const container = document.getElementById('boardContainer');
    const tbody = document.getElementById('tableBody');
    const statusDot = document.getElementById('statusDot');
    const statusText = document.getElementById('statusText');

    let lastOrdersStr = '';

    // ---- Smooth data refresh -----------------------------------------
    // Polls the ESP32's OWN /data endpoint (not the backend directly)
    // every 5s. Table rows are only rebuilt when the order data itself
    // has actually changed (compared separately from the freshness info
    // below) -- otherwise every poll would trigger a rebuild just because
    // "ageSeconds" ticked up, causing needless flicker/scroll resets.
    async function refreshData() {
      try {
        const res = await fetch('/data');
        if (!res.ok) return; // keep showing whatever is currently on screen
        const data = await res.json();

        const ordersStr = JSON.stringify(data.orders);
        if (ordersStr !== lastOrdersStr) {
          lastOrdersStr = ordersStr;
          renderRows(data.orders);
        }

        updateStatus(data.ageSeconds, data.wifiConnected);
      } catch (e) {
        // couldn't reach the ESP32's own /data endpoint -- rare since it's
        // the same device; just keep showing the current rows/status
      }
    }

    function renderRows(orders) {
      tbody.innerHTML = '';

      if (orders.length === 0) {
        const row = document.createElement('tr');
        row.className = 'row-dark';
        row.innerHTML = '<td colspan="2" style="text-align:center; color:#8b949e;">No orders ready</td>';
        tbody.appendChild(row);
        return;
      }

      orders.forEach((order, index) => {
        const row = document.createElement('tr');
        row.className = (index % 2 === 0) ? 'row-dark' : 'row-light';

        const nameCell = document.createElement('td');
        nameCell.textContent = order.cname;

        const itemsCell = document.createElement('td');
        itemsCell.textContent = order.items;

        row.appendChild(nameCell);
        row.appendChild(itemsCell);
        tbody.appendChild(row);
      });
    }

    // ---- Staleness indicator -------------------------------------------
    // Thresholds are set relative to the ESP32's 15s backend-fetch
    // interval: green within one missed cycle's buffer, amber within a
    // couple of missed cycles, red beyond that (likely a real problem,
    // not just network jitter). If FETCH_INTERVAL_MS changes in config.h,
    // adjust these thresholds to match.
    function updateStatus(ageSeconds, wifiConnected) {
      if (!wifiConnected) {
        statusDot.style.backgroundColor = '#ef4444'; // red
        statusText.textContent = 'WiFi disconnected — showing last known data';
        return;
      }

      if (ageSeconds < 0) {
        statusDot.style.backgroundColor = '#8b949e'; // gray
        statusText.textContent = 'Waiting for first update...';
        return;
      }

      if (ageSeconds <= 30) {
        statusDot.style.backgroundColor = '#10b981'; // green
      } else if (ageSeconds <= 60) {
        statusDot.style.backgroundColor = '#f59e0b'; // amber
      } else {
        statusDot.style.backgroundColor = '#ef4444'; // red
      }
      statusText.textContent = 'Updated ' + ageSeconds + 's ago';
    }

    refreshData();
    setInterval(refreshData, 5000);

    // ---- Continuous auto-scroll ---------------------------------------
    // Unchanged from before -- scrolls whatever rows are currently in the
    // table and recycles the top row to the bottom seamlessly.
    function continuousScroll() {
      container.scrollTop += 1;

      const firstRow = tbody.firstElementChild;
      if (firstRow) {
        if (container.scrollTop >= firstRow.offsetHeight) {
          container.scrollTop -= firstRow.offsetHeight;

          const lastRow = tbody.lastElementChild;
          const isLastDark = lastRow.classList.contains('row-dark');
          firstRow.className = isLastDark ? 'row-light' : 'row-dark';

          tbody.appendChild(firstRow);
        }
      }
    }

    setInterval(continuousScroll, 33);
  </script>
</body>
</html>
)rawliteral";

#endif
