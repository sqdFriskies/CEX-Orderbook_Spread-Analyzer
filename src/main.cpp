#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <iomanip>
#include <cmath>

enum class Side { BID, ASK };

struct Order {
    Side   side;
    double price;
    double size;
};

// Invariant: after loadCSV, bids are sorted descending and asks ascending,
// so [0] is always the best price on each side.
struct Orderbook {
    std::vector<Order> bids;
    std::vector<Order> asks;
};

struct Stats {
    double bestBid, bestAsk, midPrice;
    double spread, spreadPct;
    double bidDepth, askDepth;
    double vwapBuy, vwapSell;
};

std::string trim(const std::string& s) {
    const std::string ws = " \t\r\n";
    size_t start = s.find_first_not_of(ws);
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(ws);
    return s.substr(start, end - start + 1);
}

Side parseSide(const std::string& raw) {
    std::string s = raw;
    for (char& c : s) c = static_cast<char>(::tolower(c));
    if (s == "bid") return Side::BID;
    if (s == "ask") return Side::ASK;
    throw std::invalid_argument("Unknown order side: '" + raw + "'");
}

double parseDouble(const std::string& raw, const std::string& fieldName) {
    try {
        double value = std::stod(raw);
        if (!std::isfinite(value) || value <= 0.0)
            throw std::invalid_argument("Value must be finite and > 0.");
        return value;
    } catch (const std::exception&) {
        throw std::invalid_argument(
            "Invalid value for field '" + fieldName + "': '" + raw + "'"
        );
    }
}

Order parseRow(const std::string& line, int lineNumber) {
    std::stringstream ss(line);
    std::string sideStr, priceStr, sizeStr;

    std::getline(ss, sideStr,  ',');
    std::getline(ss, priceStr, ',');
    std::getline(ss, sizeStr,  ',');

    if (trim(sideStr).empty() || trim(priceStr).empty() || trim(sizeStr).empty())
        throw std::invalid_argument(
            "Line " + std::to_string(lineNumber) + " has empty fields."
        );

    Order order;
    order.side  = parseSide  (trim(sideStr));
    order.price = parseDouble(trim(priceStr), "price");
    order.size  = parseDouble(trim(sizeStr),  "size");
    return order;
}

Orderbook loadCSV(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file: '" + filename + "'");

    Orderbook book;
    std::string line;
    int lineNumber = 0;

    std::getline(file, line); // skip header
    lineNumber++;

    while (std::getline(file, line)) {
        lineNumber++;
        if (trim(line).empty()) continue;

        Order order = parseRow(line, lineNumber);
        if (order.side == Side::BID) book.bids.push_back(order);
        else                         book.asks.push_back(order);
    }

    if (book.bids.empty()) throw std::runtime_error("No bids found in file.");
    if (book.asks.empty()) throw std::runtime_error("No asks found in file.");

    std::sort(book.bids.begin(), book.bids.end(),
        [](const Order& a, const Order& b) { return a.price > b.price; });
    std::sort(book.asks.begin(), book.asks.end(),
        [](const Order& a, const Order& b) { return a.price < b.price; });

    // Crossed book means the data is corrupt
    if (book.bids[0].price >= book.asks[0].price)
        throw std::runtime_error(
            "Crossed book: best bid (" + std::to_string(book.bids[0].price) +
            ") >= best ask ("          + std::to_string(book.asks[0].price) + ")."
        );

    return book;
}

double calcDepth(const std::vector<Order>& orders, double minPrice, double maxPrice) {
    double total = 0.0;
    for (const auto& order : orders)
        if (order.price >= minPrice && order.price <= maxPrice)
            total += order.size;
    return total;
}

// Walk asks from cheapest to most expensive, accumulate volume-weighted cost
double calcVWAPBuy(const std::vector<Order>& asks, double targetQty) {
    double remaining = targetQty;
    double totalCost = 0.0;

    for (const auto& ask : asks) {
        if (remaining <= 0.0) break;
        double filled  = std::min(remaining, ask.size);
        totalCost     += filled * ask.price;
        remaining     -= filled;
    }

    if (remaining > 0.0)
        throw std::runtime_error(
            "Not enough liquidity to buy " + std::to_string(targetQty) + " units."
        );
    return totalCost / targetQty;
}

// Walk bids from most expensive to cheapest, accumulate volume-weighted revenue
double calcVWAPSell(const std::vector<Order>& bids, double targetQty) {
    double remaining    = targetQty;
    double totalRevenue = 0.0;

    for (const auto& bid : bids) {
        if (remaining <= 0.0) break;
        double filled    = std::min(remaining, bid.size);
        totalRevenue    += filled * bid.price;
        remaining       -= filled;
    }

    if (remaining > 0.0)
        throw std::runtime_error(
            "Not enough liquidity to sell " + std::to_string(targetQty) + " units."
        );
    return totalRevenue / targetQty;
}

Stats calcStats(const Orderbook& book, double depthPct, double targetQty) {
    Stats s;

    s.bestBid   = book.bids[0].price;
    s.bestAsk   = book.asks[0].price;
    s.midPrice  = (s.bestBid + s.bestAsk) / 2.0;
    s.spread    = s.bestAsk - s.bestBid;
    s.spreadPct = (s.spread / s.midPrice) * 100.0;

    double lower = s.midPrice * (1.0 - depthPct / 100.0);
    double upper = s.midPrice * (1.0 + depthPct / 100.0);
    s.bidDepth  = calcDepth(book.bids, lower, upper);
    s.askDepth  = calcDepth(book.asks, lower, upper);

    s.vwapBuy   = calcVWAPBuy (book.asks, targetQty);
    s.vwapSell  = calcVWAPSell(book.bids, targetQty);

    return s;
}

void printStats(const Stats& s, double depthPct, double targetQty) {
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "\n============================================\n";
    std::cout << "         ORDERBOOK ANALYSIS\n";
    std::cout << "============================================\n";
    std::cout << "  Best Bid    : " << s.bestBid  << "\n";
    std::cout << "  Best Ask    : " << s.bestAsk  << "\n";
    std::cout << "  Mid Price   : " << s.midPrice << "\n";
    std::cout << "  Spread      : " << s.spread << "  (" << s.spreadPct << "%)\n";
    std::cout << "--------------------------------------------\n";
    std::cout << "  Depth (±"   << depthPct << "% from mid):\n";
    std::cout << "    Bids : "  << s.bidDepth << " units\n";
    std::cout << "    Asks : "  << s.askDepth << " units\n";
    std::cout << "--------------------------------------------\n";
    std::cout << "  VWAP (qty = " << targetQty << " units):\n";
    std::cout << "    Buy  : "  << s.vwapBuy  << "\n";
    std::cout << "    Sell : "  << s.vwapSell << "\n";
    std::cout << "============================================\n\n";
}

int main(int argc, const char * argv[]) {
    const std::string default_FILENAME = "orderbook.csv";
    std::string FILENAME = default_FILENAME;
    if(argc > 1){
        FILENAME = argv[1];
    }
    const double      DEPTH_PCT  = 0.5;   // ±0.5% from mid price
    const double      TARGET_QTY = 40.0;  // quantity for VWAP calculation

    try {
        Orderbook book  = loadCSV(FILENAME);
        Stats     stats = calcStats(book, DEPTH_PCT, TARGET_QTY);
        printStats(stats, DEPTH_PCT, TARGET_QTY);
    }
    catch (const std::exception& e) {
        std::cerr << "\n[ERROR] " << e.what() << "\n\n";
        return 1;
    }

    return 0;
}