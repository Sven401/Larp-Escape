#include "Esp32StartupIdentityEntropy.h"
#include <bootloader_random.h>
#include <esp_random.h>
bool Esp32StartupIdentityEntropy::make(larp::companion::Identity &destination) noexcept {
    larp::companion::Identity selected;
    bootloader_random_enable();
    esp_fill_random(selected.bytes.data(), selected.bytes.size());
    bootloader_random_disable();
    if (!selected.valid())
        return false;
    destination = selected;
    return true;
}
