#include "NimblePeerSource.h"
#include "esp_timer.h"
extern "C" {
#include "host/ble_hs_mbuf.h"
#include "host/ble_sm.h"
#include "os/os_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "store/config/ble_store_config.h"
// Exported init entry used by the pinned SDK official bleprph example.
void ble_store_config_init(void);
}
#if !CONFIG_BT_NIMBLE_ENABLED || !CONFIG_BT_NIMBLE_SM_SC_ONLY || CONFIG_BT_NIMBLE_SM_LEGACY
#error "Qualify this source with NimBLE SC-only and legacy pairing disabled"
#endif
#if CONFIG_BT_NIMBLE_SM_SC_DEBUG_KEYS
#error "Debug pairing keys are forbidden"
#endif
#if CONFIG_BT_NIMBLE_NVS_PERSIST
#error "Old bond import requires a separate native format qualification"
#endif
namespace larp {
namespace companion {
NimblePeerSource *NimblePeerSource::selected_ = nullptr;
namespace {
const ble_uuid128_t serviceId = BLE_UUID128_INIT(0x20, 0x47, 0x0f, 0x8a, 0x5f, 0x87, 0x7c, 0x89,
                                                 0x83, 0x4e, 0x55, 0xb3, 0xe0, 0x57, 0x81, 0x6d);
const ble_uuid128_t inputId = BLE_UUID128_INIT(0x20, 0x47, 0x0f, 0x8a, 0x5f, 0x87, 0x7c, 0x89, 0x83,
                                               0x4e, 0x55, 0xb3, 0xe1, 0x57, 0x81, 0x6d);
const ble_uuid128_t outputId = BLE_UUID128_INIT(0x20, 0x47, 0x0f, 0x8a, 0x5f, 0x87, 0x7c, 0x89,
                                                0x83, 0x4e, 0x55, 0xb3, 0xe2, 0x57, 0x81, 0x6d);
std::uint64_t monotonicNow() noexcept {
    return std::uint64_t(esp_timer_get_time() / 1000);
}
std::uint32_t now() noexcept {
    return std::uint32_t(monotonicNow());
}
} // namespace
bool NimblePeerSource::onHost() const noexcept {
    return host_ && xTaskGetCurrentTaskHandle() == host_;
}
bool NimblePeerSource::ready() const noexcept {
    const auto observed = monotonicNow();
    portENTER_CRITICAL(&mutex_);
    // The host may have advanced heartbeat_ after this worker sampled time.
    if (ready_ && observed >= heartbeat_ && observed - heartbeat_ >= HeartbeatMilliseconds) {
        heartbeatFailed_ = true;
        ready_ = eligible_ = false;
        banned_ = true;
    }
    const bool value = ready_ && !heartbeatFailed_;
    portEXIT_CRITICAL(&mutex_);
    return value;
}
bool NimblePeerSource::current(const TlsOrigin &v) const noexcept {
    const auto observed = monotonicNow();
    portENTER_CRITICAL(&mutex_);
    if (eligible_ && observed >= heartbeat_ && observed - heartbeat_ >= HeartbeatMilliseconds) {
        heartbeatFailed_ = true;
        ready_ = eligible_ = false;
        banned_ = true;
    }
    // open() may still be handing the copied origin to another worker.
    // The deadline remains visible until the host completes that handoff.
    if (eligible_ && admissionPending_ && observed >= admissionStarted_ &&
        observed - admissionStarted_ >= AdmissionMilliseconds) {
        eligible_ = false;
        banned_ = true;
    }
    const bool value = eligible_ && !banned_ && !heartbeatFailed_ && view_ == v;
    portEXIT_CRITICAL(&mutex_);
    return value;
}
void NimblePeerSource::revoke(const TlsOrigin &v) noexcept {
    portENTER_CRITICAL(&mutex_);
    if (view_ == v) {
        banned_ = true;
        eligible_ = false;
    }
    portEXIT_CRITICAL(&mutex_);
}
bool NimblePeerSource::confirm(const TlsOrigin &v, std::uint32_t number, bool accepted) noexcept {
    portENTER_CRITICAL(&mutex_);
    const bool allowed = view_ == v && !banned_ && !heartbeatFailed_ && !confirmationPending_;
    if (allowed) {
        confirmation_.origin = v;
        confirmation_.number = number;
        confirmation_.accepted = accepted;
        confirmationPending_ = true;
    }
    portEXIT_CRITICAL(&mutex_);
    return allowed;
}
void NimblePeerSource::publish(bool eligible) noexcept {
    portENTER_CRITICAL(&mutex_);
    if (!(view_ == original_)) {
        view_ = original_;
        banned_ = false;
        admissionStarted_ = connectedAt_;
        admissionPending_ = true;
    }
    eligible_ = eligible && !banned_ && !heartbeatFailed_;
    portEXIT_CRITICAL(&mutex_);
}
void NimblePeerSource::fence() noexcept {
    publish(false);
    revoke(original_);
    comparing_ = false;
    portENTER_CRITICAL(&mutex_);
    ready_ = false;
    portEXIT_CRITICAL(&mutex_);
    if (live_)
        ble_gap_terminate(original_.handle, BLE_ERR_REM_USER_CONN_TERM);
}
bool NimblePeerSource::secure() const noexcept {
    ble_gap_conn_desc descriptor{};
    return onHost() && live_ && !ble_gap_conn_find(original_.handle, &descriptor) &&
           descriptor.sec_state.encrypted && descriptor.sec_state.authenticated &&
           descriptor.sec_state.bonded && descriptor.sec_state.key_size == 16;
}
bool NimblePeerSource::expireAdmission() noexcept {
    if (live_ && !opened_ && monotonicNow() - connectedAt_ >= AdmissionMilliseconds) {
        fence();
        return true;
    }
    return false;
}
void NimblePeerSource::update() noexcept {
    if (!live_ || expireAdmission())
        return;
    if (opened_) {
        if (!subscribed_ || !secure() || !current(original_))
            fence();
    } else if (subscribed_ && secure()) {
        if (expireAdmission())
            return;
        publish(true);
        if (!current(original_) || !ingress_.open(original_))
            fence();
        else {
            const auto completed = monotonicNow();
            portENTER_CRITICAL(&mutex_);
            const bool admitted = view_ == original_ && !banned_ && !heartbeatFailed_ &&
                                  completed - admissionStarted_ < AdmissionMilliseconds;
            if (admitted)
                admissionPending_ = false;
            portEXIT_CRITICAL(&mutex_);
            if (admitted)
                opened_ = true;
            else
                fence();
        }
    }
}
bool NimblePeerSource::begin() noexcept {
    if (attempted_ || selected_)
        return false;
    attempted_ = true;
    selected_ = this;
    if (nimble_port_init() != ESP_OK) {
        broken_ = true;
        return false;
    }
    ble_hs_cfg.sync_cb = sync;
    ble_hs_cfg.reset_cb = reset;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_DISPLAY_YESNO;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_store_config_init(); // RAM-only under the enforced configuration.
    ble_svc_gap_init();
    if (ble_svc_gap_device_name_set("Larp native TLS qualification"))
        return false;
    characteristics_[0].uuid = &inputId.u;
    characteristics_[0].access_cb = access;
    characteristics_[0].arg = this;
    characteristics_[0].val_handle = &receiveHandle_;
    characteristics_[0].flags =
        BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC | BLE_GATT_CHR_F_WRITE_AUTHEN;
    characteristics_[0].min_key_size = 16;
    characteristics_[1].uuid = &outputId.u;
    characteristics_[1].access_cb = access;
    characteristics_[1].arg = this;
    characteristics_[1].val_handle = &transmitHandle_;
    characteristics_[1].flags = BLE_GATT_CHR_F_NOTIFY;
    characteristics_[1].min_key_size = 16;
    services_[0].type = BLE_GATT_SVC_TYPE_PRIMARY;
    services_[0].uuid = &serviceId.u;
    services_[0].characteristics = characteristics_;
    if (ble_gatts_count_cfg(services_) || ble_gatts_add_svcs(services_) ||
        ble_npl_callout_init(&poll_, nimble_port_get_dflt_eventq(), tick, this)) {
        broken_ = true;
        return false;
    }
    const auto launched = xTaskCreateStatic(run, "larp-nimble", sizeof(hostStack_), this, 5,
                                            hostStack_.data(), &hostStorage_);
    if (!launched) {
        broken_ = true;
        return false;
    }
    return true; // Startup only; ready() confirms sync/registration/advertising.
}
void NimblePeerSource::run(void *argument) {
    auto &self = *static_cast<NimblePeerSource *>(argument);
    // Task may run before xTaskCreateStatic returns to begin().
    self.host_ = xTaskGetCurrentTaskHandle();
    nimble_port_run();
    self.publish(false);
    self.revoke(self.original_);
    self.broken_ = true;
    portENTER_CRITICAL(&self.mutex_);
    self.ready_ = false;
    portEXIT_CRITICAL(&self.mutex_);
    vTaskDelete(nullptr);
}
void NimblePeerSource::sync() {
    auto *self = selected_;
    if (!self || !self->onHost() || self->broken_ || !self->receiveHandle_ ||
        !self->transmitHandle_ || ble_hs_id_infer_auto(0, &self->addressType_)) {
        if (self)
            self->broken_ = true;
        return;
    }
    const auto syncedAt = monotonicNow();
    portENTER_CRITICAL(&self->mutex_);
    self->heartbeat_ = syncedAt;
    portEXIT_CRITICAL(&self->mutex_);
    self->advertise();
    if (ble_npl_callout_reset(&self->poll_, ble_npl_time_ms_to_ticks32(10))) {
        self->broken_ = true;
        self->fence();
    }
}
void NimblePeerSource::reset(int) {
    if (selected_) {
        selected_->broken_ = true;
        selected_->publish(false);
        selected_->revoke(selected_->original_);
        portENTER_CRITICAL(&selected_->mutex_);
        selected_->ready_ = false;
        portEXIT_CRITICAL(&selected_->mutex_);
    }
}
void NimblePeerSource::advertise() noexcept {
    if (broken_ || live_)
        return;
    portENTER_CRITICAL(&mutex_);
    const bool healthy = !heartbeatFailed_;
    portEXIT_CRITICAL(&mutex_);
    if (!healthy) {
        broken_ = true;
        return;
    }
    ble_hs_adv_fields fields{};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = &serviceId;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;
    ble_gap_adv_params parameters{};
    parameters.conn_mode = BLE_GAP_CONN_MODE_UND;
    parameters.disc_mode = BLE_GAP_DISC_MODE_GEN;
    const bool okay =
        !ble_gap_adv_set_fields(&fields) &&
        !ble_gap_adv_start(addressType_, nullptr, BLE_HS_FOREVER, &parameters, gap, this);
    portENTER_CRITICAL(&mutex_);
    ready_ = okay;
    portEXIT_CRITICAL(&mutex_);
    if (!okay)
        broken_ = true;
}
int NimblePeerSource::gap(ble_gap_event *event, void *argument) {
    auto &self = *static_cast<NimblePeerSource *>(argument);
    if (!self.onHost() || self.broken_)
        return BLE_HS_EINVAL;
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status) {
            self.advertise();
            break;
        }
        if (self.live_ || self.epoch_ == UINT32_MAX) {
            ble_gap_terminate(event->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
            break;
        }
        self.original_.registration = 1;
        self.original_.epoch = ++self.epoch_;
        self.original_.handle = event->connect.conn_handle;
        self.live_ = true;
        self.connectedAt_ = monotonicNow();
        self.opened_ = self.subscribed_ = self.comparing_ = false;
        self.publish(false);
        if (ble_gap_security_initiate(self.original_.handle))
            self.fence();
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        if (self.live_ && event->disconnect.conn.conn_handle == self.original_.handle) {
            self.publish(false);
            self.revoke(self.original_);
            self.live_ = false;
            self.opened_ = self.subscribed_ = self.comparing_ = false;
            self.advertise();
        }
        break;
    case BLE_GAP_EVENT_ENC_CHANGE:
        if (self.live_ && event->enc_change.conn_handle == self.original_.handle) {
            if (event->enc_change.status || !self.secure())
                self.fence();
            else
                self.update();
        }
        break;
    case BLE_GAP_EVENT_SUBSCRIBE:
        if (self.live_ && event->subscribe.conn_handle == self.original_.handle &&
            event->subscribe.attr_handle == self.transmitHandle_) {
            self.subscribed_ = event->subscribe.cur_notify;
            self.update();
        }
        break;
    case BLE_GAP_EVENT_PASSKEY_ACTION:
        if (self.expireAdmission())
            break;
        if (!self.live_ || event->passkey.conn_handle != self.original_.handle) {
            ble_gap_terminate(event->passkey.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
            break;
        }
        if (self.live_ && event->passkey.conn_handle == self.original_.handle &&
            event->passkey.params.action == BLE_SM_IOACT_NUMCMP && !self.comparing_) {
            self.comparing_ = true;
            self.compareNumber_ = event->passkey.params.numcmp;
            self.compareAt_ = now();
            self.ingress_.compare(self.original_, self.compareNumber_);
        } else
            self.fence();
        break;
    case BLE_GAP_EVENT_REPEAT_PAIRING:
        return BLE_GAP_REPEAT_PAIRING_IGNORE; // Never delete/replace a bond implicitly.
    default:
        break;
    }
    return 0;
}
int NimblePeerSource::access(std::uint16_t handle, std::uint16_t attribute,
                             ble_gatt_access_ctxt *context, void *argument) {
    auto &self = *static_cast<NimblePeerSource *>(argument);
    if (!self.onHost() || !context || attribute != self.receiveHandle_ ||
        context->op != BLE_GATT_ACCESS_OP_WRITE_CHR || context->offset || !self.live_ ||
        handle != self.original_.handle || !self.current(self.original_) || !self.secure())
        return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
    PeerCipherFragment copy;
    copy.origin = self.original_;
    const auto count = OS_MBUF_PKTLEN(context->om);
    if (!count || count > copy.bytes.size() ||
        os_mbuf_copydata(context->om, 0, count, copy.bytes.data()))
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    copy.count = count;
    if (!self.ingress_.push(copy)) {
        self.fence();
        return BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    return 0;
}
PeerNotify NimblePeerSource::notify(const PeerCipherFragment &copy) noexcept {
    if (!onHost() || broken_)
        return PeerNotify::Failed;
    if (!current(copy.origin) || !secure())
        return PeerNotify::Stale;
    // NimBLE may silently truncate larger notifications at ATT MTU 23.
    // The outgoing owner must split before draining/queuing TLS ciphertext.
    if (!copy.count || copy.count > PeerCipherFragment::NotifyBytes)
        return PeerNotify::Failed;
    auto *buffer = ble_hs_mbuf_from_flat(copy.bytes.data(), copy.count);
    if (!buffer)
        return PeerNotify::Busy;
    const int result = ble_gatts_notify_custom(copy.origin.handle, transmitHandle_, buffer);
    if (!result)
        return PeerNotify::Sent;
    if (result == BLE_HS_ENOMEM || result == BLE_HS_EBUSY)
        return PeerNotify::Busy;
    fence();
    return PeerNotify::Failed;
}
void NimblePeerSource::tick(ble_npl_event *event) {
    auto *self = static_cast<NimblePeerSource *>(ble_npl_event_get_arg(event));
    if (self)
        self->poll();
}
void NimblePeerSource::poll() noexcept {
    if (!onHost() || broken_)
        return;
    Confirmation confirmation;
    bool pending = false;
    const auto observed = monotonicNow();
    portENTER_CRITICAL(&mutex_);
    if (observed - heartbeat_ >= HeartbeatMilliseconds)
        heartbeatFailed_ = true;
    const bool heartbeatFailed = heartbeatFailed_;
    if (!heartbeatFailed)
        heartbeat_ = observed;
    if (confirmationPending_) {
        confirmation = confirmation_;
        confirmationPending_ = false;
        pending = true;
    }
    const bool banned = banned_;
    portEXIT_CRITICAL(&mutex_);
    if (heartbeatFailed) {
        broken_ = true;
        fence();
        ble_gap_adv_stop();
        return;
    }
    if (live_ && banned)
        fence();
    // Before a queued comparison can be injected, not only before TLS open.
    expireAdmission();
    if (pending && comparing_ && confirmation.origin == original_ &&
        confirmation.number == compareNumber_ && now() - compareAt_ < 30000) {
        ble_sm_io io{};
        io.action = BLE_SM_IOACT_NUMCMP;
        io.numcmp_accept = confirmation.accepted;
        comparing_ = false;
        if (ble_sm_inject_io(original_.handle, &io) || !confirmation.accepted)
            fence();
    }
    if (comparing_ && now() - compareAt_ >= 30000)
        fence();
    update();
    if (live_ && opened_ && current(original_) && !ingress_.poll(*this))
        fence();
    if (ble_npl_callout_reset(&poll_, ble_npl_time_ms_to_ticks32(10))) {
        broken_ = true;
        fence();
    }
}
} // namespace companion
} // namespace larp
