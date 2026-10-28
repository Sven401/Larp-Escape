#include "NimblePeerSource.h"
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace larp::companion;
#define CHECK(v)                                                                                   \
    do {                                                                                           \
        if (!(v))                                                                                  \
            throw std::runtime_error(#v);                                                          \
    } while (false)
namespace {
TaskHandle_t task = reinterpret_cast<void *>(1);
void (*taskEntry)(void *) = nullptr;
void *taskArgument = nullptr;
int (*gapCallback)(ble_gap_event *, void *) = nullptr;
void *gapArgument = nullptr;
const ble_gatt_svc_def *definitions = nullptr;
ble_npl_callout *timer = nullptr;
ble_gap_conn_desc descriptor{};
std::function<void()> hostHook;
int advertisingError = 0, securityError = 0;
int registrationError = 0, timerError = 0, notifyError = 0, terminateHandle = -1, notified = 0,
    confirms = 0;
bool allocationFailure = false, returnHost = false;
std::vector<unsigned char> sent;
std::uint32_t clockValue = 0;
struct HostYield {};
} // namespace
extern "C" {
ble_hs_config ble_hs_cfg{};
TaskHandle_t xTaskGetCurrentTaskHandle() {
    return task;
}
TaskHandle_t xTaskCreateStatic(void (*entry)(void *), const char *, uint32_t, void *arg, unsigned,
                               StackType_t *, StaticTask_t *) {
    taskEntry = entry;
    taskArgument = arg;
    return task;
}
void vTaskDelete(void *) {}
int64_t esp_timer_get_time() {
    return int64_t(clockValue) * 1000;
}
int nimble_port_init() {
    return 0;
}
void nimble_port_run() {
    ble_hs_cfg.sync_cb();
    if (hostHook)
        hostHook();
    if (!returnHost)
        throw HostYield{};
}
void *nimble_port_get_dflt_eventq() {
    return nullptr;
}
int ble_gap_conn_find(uint16_t handle, ble_gap_conn_desc *out) {
    if (handle != descriptor.conn_handle)
        return 1;
    *out = descriptor;
    return 0;
}
int ble_gap_terminate(uint16_t handle, uint8_t) {
    terminateHandle = handle;
    return 0;
}
int ble_gap_security_initiate(uint16_t) {
    return securityError;
}
int ble_gap_adv_stop() {
    return 0;
}
int ble_gap_adv_set_fields(const ble_hs_adv_fields *) {
    return advertisingError;
}
int ble_gap_adv_start(uint8_t, const void *, int, const ble_gap_adv_params *,
                      int (*fn)(ble_gap_event *, void *), void *arg) {
    gapCallback = fn;
    gapArgument = arg;
    return 0;
}
int ble_hs_id_infer_auto(int, uint8_t *out) {
    *out = 0;
    return 0;
}
void ble_svc_gap_init() {}
int ble_svc_gap_device_name_set(const char *) {
    return 0;
}
void ble_store_config_init() {}
int ble_gatts_count_cfg(const ble_gatt_svc_def *) {
    return registrationError;
}
int ble_gatts_add_svcs(const ble_gatt_svc_def *defs) {
    definitions = defs;
    *defs[0].characteristics[0].val_handle = 101;
    *defs[0].characteristics[1].val_handle = 102;
    return 0;
}
int ble_npl_callout_init(ble_npl_callout *out, void *, void (*fn)(ble_npl_event *), void *arg) {
    timer = out;
    out->callback = fn;
    out->event.arg = arg;
    return timerError;
}
int ble_npl_callout_reset(ble_npl_callout *, uint32_t) {
    return timerError;
}
uint32_t ble_npl_time_ms_to_ticks32(uint32_t v) {
    return v;
}
void *ble_npl_event_get_arg(ble_npl_event *v) {
    return v->arg;
}
int os_mbuf_copydata(const os_mbuf *v, int offset, int bytes, void *out) {
    std::memcpy(out, v->bytes + offset, bytes);
    return 0;
}
os_mbuf *ble_hs_mbuf_from_flat(const void *data, uint16_t bytes) {
    static os_mbuf v;
    if (allocationFailure)
        return nullptr;
    v.len = bytes;
    std::memcpy(v.bytes, data, bytes);
    return &v;
}
int ble_gatts_notify_custom(uint16_t, uint16_t, os_mbuf *v) {
    ++notified;
    sent.assign(v->bytes, v->bytes + v->len);
    return notifyError;
}
int ble_sm_inject_io(uint16_t, ble_sm_io *v) {
    ++confirms;
    return v->numcmp_accept ? 0 : 1;
}
}
struct Ingress final : IPeerCipherIngress {
    TlsOrigin origin;
    PeerCipherFragment copy;
    bool allowOpen = true, allowPush = true;
    int opens = 0, pushes = 0, comparisons = 0;
    bool open(const TlsOrigin &v) noexcept override {
        origin = v;
        ++opens;
        return allowOpen;
    }
    bool push(const PeerCipherFragment &v) noexcept override {
        copy = v;
        ++pushes;
        return allowPush;
    }
    bool poll(NimblePeerSource &) noexcept override {
        return true;
    }
    void compare(const TlsOrigin &v, std::uint32_t) noexcept override {
        origin = v;
        ++comparisons;
    }
};
struct Fixture {
    Ingress ingress;
    NimblePeerSource source{ingress};
    void start() {
        CHECK(source.begin());
        try {
            taskEntry(taskArgument);
        } catch (const HostYield &) {
        }
    }
    void event(ble_gap_event v) {
        CHECK(gapCallback(&v, gapArgument) == 0);
    }
    void connect(uint16_t handle = 7) {
        descriptor.conn_handle = handle;
        ble_gap_event v{};
        v.type = BLE_GAP_EVENT_CONNECT;
        v.connect.conn_handle = handle;
        event(v);
    }
    void encrypted(int status = 0) {
        ble_gap_event v{};
        v.type = BLE_GAP_EVENT_ENC_CHANGE;
        v.enc_change.conn_handle = 7;
        v.enc_change.status = status;
        event(v);
    }
    void subscribe() {
        ble_gap_event v{};
        v.type = BLE_GAP_EVENT_SUBSCRIBE;
        v.subscribe.conn_handle = 7;
        v.subscribe.attr_handle = 102;
        v.subscribe.cur_notify = 1;
        event(v);
    }
    void authenticate() {
        connect();
        descriptor.sec_state = {1, 1, 1, 16};
        encrypted();
        subscribe();
        CHECK(source.current(ingress.origin));
    }
    PeerCipherFragment output(std::uint16_t count = 20) {
        PeerCipherFragment v;
        v.origin = ingress.origin;
        v.count = count;
        v.bytes.fill(7);
        return v;
    }
    int write(std::uint16_t count = 20, std::uint16_t offset = 0) {
        os_mbuf bytes{};
        bytes.len = count;
        std::memset(bytes.bytes, 9, sizeof(bytes.bytes));
        ble_gatt_access_ctxt context{BLE_GATT_ACCESS_OP_WRITE_CHR, &bytes, offset};
        auto &c = definitions[0].characteristics[0];
        return c.access_cb(7, 101, &context, c.arg);
    }
    void disconnect(uint16_t handle) {
        ble_gap_event v{};
        v.type = BLE_GAP_EVENT_DISCONNECT;
        v.disconnect.conn.conn_handle = handle;
        event(v);
    }
};
int main(int argc, char **argv) {
    try {
        CHECK(argc == 2);
        const std::string name = argv[1];
        if (name.find("numeric-") == 0)
            clockValue = UINT32_MAX - 1000;
        Fixture f;
        if (name == "registration-failure") {
            registrationError = 1;
            CHECK(!f.source.begin());
            CHECK(!f.source.ready());
        } else if (name == "timer-failure") {
            timerError = 1;
            CHECK(!f.source.begin());
            CHECK(!f.source.ready());
        } else if (name == "host-return") {
            returnHost = true;
            TlsOrigin original;
            hostHook = [&]() {
                f.authenticate();
                original = f.ingress.origin;
            };
            f.start();
            CHECK(!f.source.ready());
            CHECK(!f.source.current(original));
            CHECK(f.ingress.opens == 1);
        } else if (name == "advertising-failure") {
            advertisingError = 1;
            f.start();
            CHECK(!f.source.ready());
        } else {
            f.start();
            CHECK(f.source.ready());
            if (name == "weak-security" || name == "unencrypted" || name == "unbonded") {
                f.connect();
                descriptor.sec_state = {1, 0, 1, 16};
                if (name == "unencrypted")
                    descriptor.sec_state = {0, 1, 1, 16};
                if (name == "unbonded")
                    descriptor.sec_state = {1, 1, 0, 16};
                f.encrypted();
                CHECK(f.ingress.opens == 0);
                CHECK(terminateHandle == 7);
            } else if (name == "short-key") {
                f.connect();
                descriptor.sec_state = {1, 1, 1, 15};
                f.encrypted();
                CHECK(f.ingress.opens == 0);
                CHECK(terminateHandle == 7);
            } else if (name == "security-start-failure") {
                securityError = 1;
                f.connect();
                CHECK(terminateHandle == 7);
                CHECK(f.ingress.opens == 0);
            } else if (name == "open-failure") {
                f.ingress.allowOpen = false;
                f.connect();
                descriptor.sec_state = {1, 1, 1, 16};
                f.encrypted();
                f.subscribe();
                CHECK(f.ingress.opens == 1);
                CHECK(!f.source.current(f.ingress.origin));
            } else if (name.find("numeric-") == 0) {
                f.connect();
                ble_gap_event v{};
                v.type = BLE_GAP_EVENT_PASSKEY_ACTION;
                v.passkey.conn_handle = 7;
                v.passkey.params.action = BLE_SM_IOACT_NUMCMP;
                v.passkey.params.numcmp = 123456;
                f.event(v);
                CHECK(confirms == 0);
                CHECK(f.ingress.comparisons == 1);
                if (name == "numeric-timeout") {
                    for (int i = 0; i < 3000; ++i) {
                        clockValue += 10;
                        timer->callback(&timer->event);
                    }
                    CHECK(confirms == 0);
                    CHECK(terminateHandle == 7);
                } else if (name == "numeric-reject") {
                    CHECK(f.source.confirm(f.ingress.origin, 123456, false));
                    timer->callback(&timer->event);
                    CHECK(confirms == 1);
                    CHECK(terminateHandle == 7);
                } else {
                    if (name == "numeric-wrong") {
                        CHECK(f.source.confirm(f.ingress.origin, 654321, true));
                        timer->callback(&timer->event);
                        CHECK(confirms == 0);
                    }
                    auto old = f.ingress.origin;
                    --old.epoch;
                    CHECK(!f.source.confirm(old, 123456, true));
                    CHECK(f.source.confirm(f.ingress.origin, 123456, true));
                    timer->callback(&timer->event);
                    CHECK(confirms == 1);
                }
            } else {
                f.authenticate();
                auto original = f.ingress.origin;
                if (name == "copy-and-notify") {
                    CHECK(f.write() == 0);
                    CHECK(f.ingress.copy.count == 20);
                    CHECK(f.ingress.copy.bytes[0] == 9);
                    CHECK(f.source.notify(f.output()) == PeerNotify::Sent);
                    CHECK(sent.size() == 20 && sent[0] == 7);
                } else if (name == "default-mtu") {
                    CHECK(f.source.notify(f.output(21)) == PeerNotify::Failed);
                    CHECK(notified == 0);
                    CHECK(f.source.notify(f.output(20)) == PeerNotify::Sent);
                } else if (name == "reconnect") {
                    f.disconnect(7);
                    CHECK(!f.source.current(original));
                    f.authenticate();
                    CHECK(f.ingress.origin.epoch > original.epoch);
                    auto old = f.output();
                    old.origin = original;
                    CHECK(f.source.notify(old) == PeerNotify::Stale);
                    CHECK(notified == 0);
                    CHECK(f.source.current(f.ingress.origin));
                } else if (name == "second-link") {
                    ble_gap_event v{};
                    v.type = BLE_GAP_EVENT_CONNECT;
                    v.connect.conn_handle = 8;
                    f.event(v);
                    CHECK(terminateHandle == 8);
                    f.disconnect(8);
                    v.type = BLE_GAP_EVENT_PASSKEY_ACTION;
                    v.passkey.conn_handle = 8;
                    f.event(v);
                    CHECK(f.source.current(original));
                } else if (name == "encryption-error") {
                    f.encrypted(1);
                    CHECK(!f.source.current(original));
                    CHECK(f.source.notify(f.output()) == PeerNotify::Stale);
                } else if (name == "queue-full") {
                    f.ingress.allowPush = false;
                    CHECK(f.write() == BLE_ATT_ERR_INSUFFICIENT_RES);
                    CHECK(!f.source.current(original));
                } else if (name == "wrong-task") {
                    task = reinterpret_cast<void *>(2);
                    CHECK(f.write() == BLE_ATT_ERR_INSUFFICIENT_AUTHEN);
                    CHECK(f.source.notify(f.output()) == PeerNotify::Failed);
                    CHECK(f.source.current(original));
                } else if (name == "invalid-write") {
                    CHECK(f.write(0) == BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN);
                    CHECK(f.write(129) == BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN);
                    CHECK(f.write(20, 1) == BLE_ATT_ERR_INSUFFICIENT_AUTHEN);
                    CHECK(f.ingress.pushes == 0);
                } else if (name == "notify-busy") {
                    allocationFailure = true;
                    CHECK(f.source.notify(f.output()) == PeerNotify::Busy);
                    CHECK(notified == 0 && f.source.current(original));
                    allocationFailure = false;
                    CHECK(f.source.notify(f.output()) == PeerNotify::Sent);
                } else if (name == "lost-poll") {
                    clockValue += 100;
                    CHECK(!f.source.current(original));
                    CHECK(!f.source.ready());
                    timer->callback(&timer->event);
                    CHECK(!f.source.current(original));
                    CHECK(!f.source.ready());
                    CHECK(terminateHandle == 7);
                } else if (name == "runtime-timer-failure") {
                    timerError = 1;
                    timer->callback(&timer->event);
                    CHECK(!f.source.ready());
                    CHECK(!f.source.current(original));
                } else if (name == "notify-sdk-busy") {
                    notifyError = BLE_HS_EBUSY;
                    CHECK(f.source.notify(f.output()) == PeerNotify::Busy);
                    CHECK(f.source.current(original));
                    notifyError = 0;
                    CHECK(f.source.notify(f.output()) == PeerNotify::Sent);
                } else if (name == "notify-sdk-fatal") {
                    notifyError = 99;
                    CHECK(f.source.notify(f.output()) == PeerNotify::Failed);
                    CHECK(!f.source.current(original));
                } else if (name == "reset") {
                    ble_hs_cfg.reset_cb(1);
                    CHECK(!f.source.ready());
                    CHECK(!f.source.current(original));
                } else if (name == "worker-revoke") {
                    f.source.revoke(original);
                    CHECK(!f.source.current(original));
                    timer->callback(&timer->event);
                    CHECK(terminateHandle == 7);
                    CHECK(f.source.notify(f.output()) == PeerNotify::Stale);
                } else
                    throw std::runtime_error("unknown case");
            }
        }
        std::cout << name << " passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
