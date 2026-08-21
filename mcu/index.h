#ifndef INDEX_H
#define INDEX_H

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Display Board</title>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <meta http-equiv='refresh' content='60'>
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
      margin-bottom: 20px; 
      text-transform: uppercase; 
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
  
  <div class="scroll-container" id="boardContainer">
    <table>
      <thead>
        <tr><th>Name</th><th>Order</th></tr>
      </thead>
      <tbody id="tableBody">
        %TABLE_ROWS%
      </tbody>
    </table>
  </div>

  <script>
    const container = document.getElementById('boardContainer');
    const tbody = document.getElementById('tableBody');

    // 1. Assign initial alternating row colors on load
    Array.from(tbody.children).forEach((row, index) => {
      row.className = (index % 2 === 0) ? 'row-dark' : 'row-light';
    });

    function continuousScroll() {
      container.scrollTop += 1;

      const firstRow = tbody.firstElementChild;
      if (firstRow) {
        if (container.scrollTop >= firstRow.offsetHeight) {
          container.scrollTop -= firstRow.offsetHeight;
          
          // 2. Check current bottom row's color and set recycled row to the opposite
          const lastRow = tbody.lastElementChild;
          const isLastDark = lastRow.classList.contains('row-dark');
          firstRow.className = isLastDark ? 'row-light' : 'row-dark';

          // 3. Move row to bottom seamlessly
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