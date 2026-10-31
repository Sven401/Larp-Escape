#pragma once

// This S3 bring-up profile owns only the onboard RGB LED and radios.
// No legacy-box I2C, audio, matrix or NFC pins are initialized.
#if defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(LARP_S3_LED_TEST)
#error "ESP32-S3 requires the explicit S3 LED test profile"
#endif
#if defined(LARP_S3_LED_TEST) && (defined(LARP_NFC_TEST) || defined(LARP_MATRIX_TEST))
#error "The S3 LED test profile has no NFC or matrix wiring"
#endif
#ifndef LARP_LED_PIN
#define LARP_LED_PIN 15
#endif

namespace larp { namespace board {
#ifdef LARP_S3_LED_TEST
constexpr bool LegacyPeripherals = false;
static_assert(LARP_LED_PIN == 21,
              "Waveshare ESP32-S3-Zero onboard RGB LED uses GPIO21");
#else
constexpr bool LegacyPeripherals = true;
#endif
constexpr unsigned LedPin = LARP_LED_PIN;
constexpr unsigned OutputPixels = LegacyPeripherals ? 23 : 1;
}}
