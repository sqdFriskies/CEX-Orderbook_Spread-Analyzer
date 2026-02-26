# CEX Orderbook & Spread Analyzer

A C++ program that reads a static orderbook snapshot from a CSV file and computes key market metrics: best bid/ask, spread, market depth, and VWAP.

---

## Concepts

**Best Bid** — the highest price a buyer is currently willing to pay. If you want to sell immediately, this is the price you get.

**Best Ask** — the lowest price a seller is currently willing to accept. If you want to buy immediately, this is the price you pay.

**Spread** — the difference between best ask and best bid:
```
spread = best_ask - best_bid
```
A tight spread means a liquid market. A wide spread means it's costly to trade immediately.

**Mid Price** — the reference price between both sides:
```
mid = (best_bid + best_ask) / 2
```

**Depth** — total volume of orders within ±X% of mid price. Shows how much you can trade without moving the price significantly.

**VWAP (Volume Weighted Average Price)** — the average execution price when buying or selling a given quantity. The program walks through orders from the best price, filling them one by one, and computes the weighted average:
```
VWAP = Σ(price_i × filled_i) / total_quantity
```
Buy VWAP is always ≥ best ask. Sell VWAP is always ≤ best bid.

---

## Project Structure

```
CEX-ORDERBOOK_SPREAD-ANALYZER/
├── build/                  — compiled output (generated, not committed)
├── src/
│   ├── main.cpp            — source code
│   └── orderbook.csv       — sample data (12 orders)
├── .gitignore
├── CMakeLists.txt          — build configuration
└── README.md
```

---

## Build & Run

**Requirements:** CMake 3.10+, C++17 compiler (g++ 7+ or clang++ 5+)

```bash
# 1. Create and enter the build directory
mkdir build && cd build

# 2. Generate build files
cmake ..

# 3. Compile
cmake --build .

# 4. Run with default CSV
./orderbook

# 5. Or pass a custom CSV file as argument
./orderbook /path/to/your/data.csv
```

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.10)
project(orderbook)

set(CMAKE_CXX_STANDARD 17)

add_executable(orderbook src/main.cpp)
```

---

## CSV Format

```
side,price,size
bid,99.80,5
ask,100.20,8
```

- `side` — `bid` or `ask` (case-insensitive)
- `price` — must be a finite positive number
- `size` — must be a finite positive number

---

## Configuration

The CSV path can be passed as a command-line argument. If omitted, the program falls back to `orderbook.csv` in the working directory:

```bash
./orderbook                        # uses orderbook.csv by default
./orderbook ../src/orderbook.csv   # relative path
./orderbook /home/user/btc.csv     # absolute path
```

Depth window and VWAP quantity are constants in `main()`:

```cpp
const double DEPTH_PCT  = 0.5;   // depth window: ±0.5% from mid
const double TARGET_QTY = 40.0;  // quantity for VWAP calculation
```

---

## Example Output

Using the provided `orderbook.csv` (6 bids, 6 asks):

```
============================================
         ORDERBOOK ANALYSIS
============================================
  Best Bid    : 99.8000
  Best Ask    : 100.2000
  Mid Price   : 100.0000
  Spread      : 0.4000  (0.4000%)
--------------------------------------------
  Depth (±0.5% from mid):
    Bids : 20.0000 units
    Asks : 8.0000 units
--------------------------------------------
  VWAP (qty = 40.0000 units):
    Buy  : 100.5900
    Sell : 99.3875
============================================
```

### Manual VWAP verification (buy 40 units)

Walking asks from cheapest to most expensive:

| Ask Price | Available | Filled | Cost        |
|-----------|-----------|--------|-------------|
| 100.20    | 8         | 8      | 801.60      |
| 100.50    | 12        | 12     | 1206.00     |
| 100.80    | 25        | 20     | 2016.00     |
| **Total** |           | **40** | **4023.60** |

```
VWAP = 4023.60 / 40 = 100.59 ✓
```

---

## Error Handling

The program validates input at every step and throws descriptive errors:

| Situation | Error message |
|---|---|
| File not found | `Cannot open file: 'orderbook.csv'` |
| Empty field in a row | `Line 3 has empty fields.` |
| Unknown side value | `Unknown order side: 'buy'` |
| Non-numeric value | `Invalid value for field 'price': 'abc'` |
| Negative or zero value | `Value must be finite and > 0.` |
| Crossed book (bid ≥ ask) | `Crossed book: best bid (101.00) >= best ask (100.00).` |
| Not enough liquidity | `Not enough liquidity to buy 500.0000 units.` |

---

## Code Architecture

The program is organized into 6 sections:

| Section | Responsibility |
|---|---|
| **Data types** | `Order`, `Orderbook`, `Stats` structs; `Side` enum |
| **Helpers** | `trim`, `parseSide`, `parseDouble` — safe input parsing |
| **CSV loading** | `parseRow`, `loadCSV` — reads, validates, sorts the book |
| **Calculations** | `calcDepth`, `calcVWAPBuy`, `calcVWAPSell`, `calcStats` |
| **Output** | `printStats` — formatted console output |
| **main** | Wires everything together, handles top-level errors |

Key design decisions:
- Bids and asks are sorted once on load (descending/ascending), so best prices are always `[0]` — O(1) access.
- Each parsing function throws a specific exception with field name and bad value, making CSV errors easy to diagnose.
- `enum class Side` is used internally after parsing — no raw string comparisons in business logic.
