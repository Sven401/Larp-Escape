#pragma once
#include "LedWire.h"
namespace larp { namespace espnowradio {
bool begin();
void service(std::uint32_t now);
void cancel();
ledwire::EspNowSnapshot snapshot();
bool submit(const std::array<std::uint8_t, 6> &, std::uint32_t now);
}}
