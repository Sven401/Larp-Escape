#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef void *TaskHandle_t;
typedef uint32_t StackType_t;
typedef struct {
    int value;
} StaticTask_t;
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portBYTE_ALIGNMENT 16
void fake_enter(void);
void fake_exit(void);
#define portENTER_CRITICAL(p) ((void)(p),fake_enter())
#define portEXIT_CRITICAL(p) ((void)(p),fake_exit())
typedef void *QueueHandle_t;
QueueHandle_t xQueueCreate(unsigned,unsigned);
int xQueueSend(QueueHandle_t,const void*,int);
int xQueueReceive(QueueHandle_t,void*,int);
#define pdTRUE 1
TaskHandle_t xTaskCreateStaticPinnedToCore(void(*)(void*),const char*,uint32_t,void*,unsigned,StackType_t*,StaticTask_t*,int);
TaskHandle_t xTaskGetCurrentTaskHandle(void);
TaskHandle_t xTaskCreateStatic(void (*)(void *), const char *, uint32_t, void *, unsigned,
                               StackType_t *, StaticTask_t *);
void vTaskDelete(void *);
int64_t esp_timer_get_time(void);
#define ESP_OK 0
#define BLE_ERR_REM_USER_CONN_TERM 19
#define BLE_HS_CONN_HANDLE_NONE 0xffff
#define BLE_ATT_ERR_UNLIKELY 14
#define BLE_GAP_EVENT_ADV_COMPLETE 7
#define BLE_HS_EINVAL 1
#define BLE_HS_ENOMEM 2
#define BLE_HS_EBUSY 3
#define BLE_HS_IO_DISPLAY_YESNO 1
#define BLE_SM_PAIR_KEY_DIST_ENC 1
#define BLE_SM_PAIR_KEY_DIST_ID 2
#define BLE_GATT_CHR_F_WRITE 1
#define BLE_GATT_CHR_F_WRITE_ENC 2
#define BLE_GATT_CHR_F_WRITE_AUTHEN 4
#define BLE_GATT_CHR_F_NOTIFY 8
#define BLE_GATT_SVC_TYPE_PRIMARY 1
#define BLE_GATT_ACCESS_OP_WRITE_CHR 2
#define BLE_ATT_ERR_INSUFFICIENT_AUTHEN 5
#define BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN 13
#define BLE_ATT_ERR_INSUFFICIENT_RES 17
#define BLE_HS_ADV_F_DISC_GEN 1
#define BLE_HS_ADV_F_BREDR_UNSUP 2
#define BLE_GAP_CONN_MODE_UND 1
#define BLE_GAP_DISC_MODE_GEN 2
#define BLE_HS_FOREVER (-1)
#define BLE_GAP_EVENT_CONNECT 1
#define BLE_GAP_EVENT_DISCONNECT 2
#define BLE_GAP_EVENT_ENC_CHANGE 3
#define BLE_GAP_EVENT_SUBSCRIBE 4
#define BLE_GAP_EVENT_PASSKEY_ACTION 5
#define BLE_GAP_EVENT_REPEAT_PAIRING 6
#define BLE_GAP_REPEAT_PAIRING_IGNORE 0
#define BLE_SM_IOACT_NUMCMP 4
struct ble_uuid_t {
    uint8_t type;
};
struct ble_uuid128_t {
    struct ble_uuid_t u;
    uint8_t value[16];
};
#define BLE_UUID128_INIT(...)                                                                      \
    {                                                                                              \
        {128}, {                                                                                   \
            __VA_ARGS__                                                                            \
        }                                                                                          \
    }
struct os_mempool { unsigned mp_num_free; };
struct os_mbuf_pool { struct os_mempool *omp_pool; };
int os_msys_num_free(void);
struct os_mbuf {
    struct os_mbuf_pool *om_omp;
    uint16_t len;
    unsigned char bytes[128];
};
#define OS_MBUF_PKTLEN(p) ((p)->len)
struct ble_gatt_access_ctxt {
    int op;
    struct os_mbuf *om;
    uint16_t offset;
};
struct ble_gatt_chr_def {
    const struct ble_uuid_t *uuid;
    int (*access_cb)(uint16_t, uint16_t, struct ble_gatt_access_ctxt *, void *);
    void *arg;
    unsigned flags;
    uint8_t min_key_size;
    uint16_t *val_handle;
};
struct ble_gatt_svc_def {
    int type;
    const struct ble_uuid_t *uuid;
    struct ble_gatt_chr_def *characteristics;
};
struct ble_gap_sec_state {
    unsigned encrypted, authenticated, bonded, key_size;
};
struct ble_gap_conn_desc {
    uint16_t conn_handle;
    struct ble_gap_sec_state sec_state;
};
struct ble_gap_event {
    int type;
    struct {
        int status;
        uint16_t conn_handle;
    } connect;
    struct {
        struct ble_gap_conn_desc conn;
    } disconnect;
    struct {
        int status;
        uint16_t conn_handle;
    } enc_change;
    struct {
        uint16_t conn_handle, attr_handle;
        int cur_notify;
    } subscribe;
    struct {
        uint16_t conn_handle;
        struct {
            int action;
            uint32_t numcmp;
        } params;
    } passkey;
};
struct ble_hs_config {
    void (*sync_cb)(void);
    void (*reset_cb)(int);
    int sm_io_cap, sm_bonding, sm_mitm, sm_sc, sm_our_key_dist, sm_their_key_dist;
};
extern struct ble_hs_config ble_hs_cfg;
struct ble_hs_adv_fields {
    unsigned flags;
    const struct ble_uuid128_t *uuids128;
    int num_uuids128, uuids128_is_complete;
    const uint8_t *name,*mfg_data;uint8_t name_len,name_is_complete,mfg_data_len;
};
struct ble_gap_adv_params {
    int conn_mode, disc_mode;
};
struct ble_sm_io {
    int action;
    uint8_t numcmp_accept;
};
struct ble_npl_event {
    void *arg;
};
struct ble_npl_callout {
    struct ble_npl_event event;
    void (*callback)(struct ble_npl_event *);
};
int nimble_port_init(void);
void nimble_port_run(void);
void *nimble_port_get_dflt_eventq(void);
int ble_gap_conn_find(uint16_t, struct ble_gap_conn_desc *);
int ble_gap_terminate(uint16_t, uint8_t);
int ble_gap_security_initiate(uint16_t);
int ble_gap_adv_stop(void);
int ble_gap_adv_set_fields(const struct ble_hs_adv_fields *);
int ble_gap_adv_rsp_set_fields(const struct ble_hs_adv_fields *);
void ble_svc_gatt_init(void);
int ble_hs_mbuf_to_flat(const struct os_mbuf*,void*,uint16_t,uint16_t*);
int ble_gap_adv_start(uint8_t, const void *, int, const struct ble_gap_adv_params *,
                      int (*)(struct ble_gap_event *, void *), void *);
int ble_hs_id_infer_auto(int, uint8_t *);
void ble_svc_gap_init(void);
int ble_svc_gap_device_name_set(const char *);
void ble_store_config_init(void);
int ble_gatts_count_cfg(const struct ble_gatt_svc_def *);
int ble_gatts_add_svcs(const struct ble_gatt_svc_def *);
int ble_npl_callout_init(struct ble_npl_callout *, void *, void (*)(struct ble_npl_event *),
                         void *);
int ble_npl_callout_reset(struct ble_npl_callout *, uint32_t);
uint32_t ble_npl_time_ms_to_ticks32(uint32_t);
void *ble_npl_event_get_arg(struct ble_npl_event *);
int os_mbuf_copydata(const struct os_mbuf *, int, int, void *);
struct os_mbuf *ble_hs_mbuf_from_flat(const void *, uint16_t);
int ble_gatts_notify_custom(uint16_t, uint16_t, struct os_mbuf *);
int ble_sm_inject_io(uint16_t, struct ble_sm_io *);
#ifdef __cplusplus
}
#endif
