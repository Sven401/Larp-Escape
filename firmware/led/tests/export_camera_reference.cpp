// Host-only oracle: execute the product engine, never duplicate effect formulas.
#include "LedOutput.h"
#include <cstdlib>
#include <iostream>
#include <string>
using namespace larp::led;
namespace {
std::uint32_t now = 0;
std::uint32_t clockNow() { return now; }
struct Bus final : LedBus {
    bool begin() override { return true; }
    bool submit(const Frame &frame) override {
        for (std::size_t i = 0; i < frame.size(); ++i)
            std::cout << now << ',' << i << ',' << unsigned(frame[i].red) << ','
                      << unsigned(frame[i].green) << ',' << unsigned(frame[i].blue) << '\n';
        return true;
    }
    TxResult poll() override { return TxResult::Complete; }
    bool reset() override { return true; }
};
}
int main(int argc, char **argv) {
    if (argc != 2) return EXIT_FAILURE;
    const std::string name = argv[1];
    const Effect effect = name == "twinkle" ? Effect::Twinkle :
                          name == "burst" ? Effect::Burst :
                          name == "ripple" ? Effect::Ripple : Effect::Off;
    if (effect == Effect::Off) return EXIT_FAILURE;
    Bus bus;
    LedOutput output(bus, clockNow);
    if (!output.begin()) return EXIT_FAILURE;
    // Complete initial darkness before exporting the effect's time origin.
    std::cout.setstate(std::ios_base::failbit);
    output.service(); output.service();
    std::cout.clear();
    std::cout << "t_ms,led,red,green,blue\n";
    if (!output.start(effect)) return EXIT_FAILURE;
    for (now = 0; now <= 6000; ++now) output.service();
}
