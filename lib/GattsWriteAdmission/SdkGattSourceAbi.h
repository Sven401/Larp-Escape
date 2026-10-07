#pragma once
// Narrow C ABI derived from Apache-2.0 ESP-IDF v4.4.6 sources. See
// docs/companion/gatts-source-sdk-v1.md. No installed SDK file is modified.
#include <cstddef>
#include <cstdint>
#ifndef LARP_GATT_SOURCE_HOST_MODEL
#include <esp_gatt_defs.h>
#include <esp_idf_version.h>
static_assert(ESP_IDF_VERSION == ESP_IDF_VERSION_VAL(4, 4, 6),
              "private GATT ABI requires IDF4.4.6");
static_assert(ESP_GATT_MAX_ATTR_LEN == 600, "private GATT value extent changed");
#endif
namespace larp {
namespace companion {
namespace sdk446 {
struct Uuid {
    std::uint16_t len;
    union {
        std::uint16_t uuid16;
        std::uint32_t uuid32;
        std::uint8_t uuid128[16];
    } uu;
};
struct Read {
    std::uint16_t handle, offset;
    bool isLong, needResponse;
};
struct Write {
    std::uint16_t handle, offset, length;
    std::uint8_t value[600];
    bool needResponse, prepared;
};
union Request {
    Read read;
    Write write;
    std::uint16_t handle, mtu;
    std::uint8_t execute;
};
struct Value {
    std::uint16_t connection, handle, offset, length;
    std::uint8_t authentication, value[600];
};
union Response {
    Value value;
    std::uint16_t handle;
};
union Complete {
    Value value;
    std::uint16_t mtu, handle;
};
struct Discovery; // Passed through as an opaque pointer, never read/called here.
using ConnectionCallback = void (*)(std::uint8_t, std::uint8_t *, std::uint16_t, bool,
                                    std::uint16_t, std::uint8_t);
using RequestCallback = void (*)(std::uint16_t, std::uint32_t, std::uint8_t, Request *);
struct Callbacks {
    ConnectionCallback connection;
    void (*complete)(std::uint16_t, std::uint8_t, std::uint8_t, Complete *);
    void (*discovery)(std::uint16_t, std::uint8_t, Discovery *);
    void (*discoveryComplete)(std::uint16_t, std::uint8_t, std::uint8_t);
    RequestCallback request;
    void (*encrypted)(std::uint8_t, std::uint8_t *);
    void (*congestion)(std::uint16_t, bool);
};
static_assert(sizeof(bool) == 1 && sizeof(Uuid) == 20 && offsetof(Uuid, uu) == 4,
              "source scalar/UUID ABI");
static_assert(sizeof(Read) == 6 && offsetof(Read, handle) == 0 && offsetof(Read, offset) == 2 &&
                  offsetof(Read, isLong) == 4 && offsetof(Read, needResponse) == 5,
              "source read ABI");
static_assert(sizeof(Write) == 608 && sizeof(Request) == 608 && offsetof(Write, value) == 6 &&
                  offsetof(Write, needResponse) == 606 && offsetof(Write, prepared) == 607,
              "source write ABI");
static_assert(sizeof(Response) == 610 && offsetof(Value, value) == 9, "source response ABI");
static_assert(sizeof(Callbacks) == 7 * sizeof(ConnectionCallback) &&
                  offsetof(Callbacks, request) == 4 * sizeof(ConnectionCallback),
              "source callback ABI");
} // namespace sdk446
} // namespace companion
} // namespace larp
struct osi_thread;
extern "C" {
std::uint8_t GATTS_SendRsp(std::uint16_t, std::uint32_t, std::uint8_t,
                           larp::companion::sdk446::Response *);
std::uint8_t __real_GATT_Register(larp::companion::sdk446::Uuid *,
                                  const larp::companion::sdk446::Callbacks *);
std::uint8_t __wrap_GATT_Register(larp::companion::sdk446::Uuid *,
                                  const larp::companion::sdk446::Callbacks *);
bool osi_thread_post(osi_thread *, void (*)(void *), void *, int, std::uint32_t);
extern void *btu_thread;
}
