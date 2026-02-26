#include <iostream>
#include <fstream>
#include <random>
#include <iomanip>
#include <string>
#include <stdexcept>

struct Config {
    std::string filename   = "orderbook.csv";
    int         levels     = 10;      // number of price levels per side
    double      midPrice   = 100.0;   // starting mid price
    double      tickSize   = 0.10;    // price step between levels
    double      maxSize    = 50.0;    // max order size
    double      minSize    = 1.0;     // min order size
};

void generateCSV(const Config& cfg) {
    std::ofstream file(cfg.filename);
    if (!file.is_open())
        throw std::runtime_error("Cannot open file for writing: '" + cfg.filename + "'");

    // Random number generator seeded from hardware entropy
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_real_distribution<double> sizeDist(cfg.minSize, cfg.maxSize);

    file << std::fixed << std::setprecision(2);
    file << "side,price,size\n";

    // Bids: from best bid downward
    // Best bid is one tick below mid price
    for (int i = 1; i <= cfg.levels; i++) {
        double price = cfg.midPrice - i * cfg.tickSize;
        double size  = sizeDist(rng);
        file << "bid," << price << "," << size << "\n";
    }

    // Asks: from best ask upward
    // Best ask is one tick above mid price
    for (int i = 1; i <= cfg.levels; i++) {
        double price = cfg.midPrice + i * cfg.tickSize;
        double size  = sizeDist(rng);
        file << "ask," << price << "," << size << "\n";
    }

    std::cout << "Generated " << cfg.filename
              << " (" << cfg.levels << " bids + " << cfg.levels << " asks"
              << ", mid = " << cfg.midPrice << ")\n";
}

int main(int argc, char* argv[]) {
    Config cfg;

    if (argc > 1) cfg.filename = argv[1];
    if (argc > 2) cfg.levels   = std::stoi(argv[2]);
    if (argc > 3) cfg.midPrice = std::stod(argv[3]);

    try {
        generateCSV(cfg);
    }
    catch (const std::exception& e) {
        std::cerr << "\n[ERROR] " << e.what() << "\n\n";
        return 1;
    }

    return 0;
}