#pragma once
#include "LedOutput.h"
#include <array>
#include <cstdint>

namespace larp { namespace led { namespace near {
constexpr std::uint32_t Scale = 1000000U;
constexpr unsigned Slots = 2;
enum class State : std::uint8_t { Empty, Active, Stopping, Stopped };
enum class Operation : std::uint8_t { Start, Update, Stop };
struct Parameters {
    std::uint16_t level = 0, gain = 1000, duration = 1000;
    bool valid = false;
    Pixel colour;
    std::uint8_t curve = 0;
    Parameters() { colour.red = 24; colour.green = 12; colour.blue = 2; }
};
bool valid(const Parameters &p);
bool equal(const Parameters &a, const Parameters &b);
struct Command {
    unsigned slot = 0;
    std::uint32_t generation = 0, sequence = 0;
    Operation operation = Operation::Start;
    Parameters parameters;
};
struct Instance {
    State state = State::Empty;
    std::uint32_t generation = 0, sequence = 0, startedAt = 0;
    Parameters parameters;
    std::array<std::uint32_t, 3> from{}, to{};
    Operation lastOperation = Operation::Start;
    Parameters lastParameters;
};
// One serialized owner. No hardware, borrowed parameter storage, heap, or clock I/O.
// Call service before command dispatch; render also services completed ramps.
class Scene {
public:
    const char *apply(const Command &command, std::uint32_t now);
    void service(std::uint32_t now);
    Frame render(std::uint32_t now);
    void reset();
    bool background(Pixel pixel);
    Pixel background() const { return background_; }
    const Instance &instance(unsigned slot) const { return instances_[slot]; }
    std::uint32_t revision() const { return revision_; }
    static std::array<std::uint32_t, 3> value(const Instance &, std::uint32_t now);
private:
    std::array<Instance, Slots> instances_{};
    Pixel background_{};
    std::uint32_t revision_ = 1;
};
}}}
