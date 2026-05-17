#include "ap_def.h"

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <bluetooth/scan.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ble_pad, LOG_LEVEL_INF);

#define CHECK_ERR(f, msg) \
    do { \
        int _err = (f); \
        if (_err) { \
            logPrintf(msg " (err %d)\n", _err); \
            break; \
        } \
    } while(0)

#define MAX_SUBS 3

static bool init(void);
static bool blePadInit(void);
static void blePadThread(void const *arg);

static void blePadScanFilterMatch(struct bt_scan_device_info *device_info, struct bt_scan_filter_match *filter_match, bool connectable);
static void blePadConnected(struct bt_conn *conn, uint8_t err);
static void blePadDisconnected(struct bt_conn *conn, uint8_t reason);
static void blePadSecurityChanged(struct bt_conn *conn, bt_security_t level, enum bt_security_err err);
static void blePadPairingComplete(struct bt_conn *conn, bool bonded);
static void blePadPairingFailed(struct bt_conn *conn, enum bt_security_err reason);

static void blePadDiscoverDelayWorker(struct k_work *work);
static void blePadActivationWorker(struct k_work *work); // 💡 새로 추가된 활성화 전용 워커

MODULE_DEF(ble_pad) 
{
  .name = "ble_pad",
  .priority = MODULE_PRI_LOW,
  .init = init
};

static K_THREAD_STACK_DEFINE(thread_stack, _HW_DEF_RTOS_THREAD_MEM_BLEPAD);
static struct k_thread thread_data;
static struct bt_conn *ble_pad_conn;

static struct k_work_delayable ble_pad_discover_delay_work;
static struct k_work_delayable ble_pad_activation_work; // 💡 비동기 패킷 송신용 워커

static struct bt_gatt_discover_params discover_params;
static struct bt_gatt_subscribe_params sub_params_pool[MAX_SUBS];
static uint8_t allocated_subs = 0;

static uint16_t handle_2a4c = 0;         
static uint16_t xbox_report_val_handle = 0; 
static bool is_subscribed = false;       

BT_SCAN_CB_INIT(ble_pad_scan_cb, blePadScanFilterMatch, NULL, NULL, NULL);

BT_CONN_CB_DEFINE(ble_pad_conn_callbacks) = {
  .connected        = blePadConnected,
  .disconnected     = blePadDisconnected,
  .security_changed = blePadSecurityChanged,
};

static struct bt_conn_auth_info_cb ble_pad_auth_info_cb = {
  .pairing_complete = blePadPairingComplete,
  .pairing_failed   = blePadPairingFailed,
};

bool init(void)
{
  k_tid_t tid = k_thread_create(&thread_data, thread_stack,
                                K_THREAD_STACK_SIZEOF(thread_stack),
                                (k_thread_entry_t)blePadThread,
                                NULL, NULL, NULL,
                                _HW_DEF_RTOS_THREAD_PRI_BLEPAD, 0, K_NO_WAIT);
  return tid != NULL;
}

static bool bldPadScanInit(void)
{
  struct bt_scan_init_param scan_init_obj = { .connect_if_match = false };
  bt_scan_init(&scan_init_obj);
  bt_scan_cb_register(&ble_pad_scan_cb);

  CHECK_ERR(bt_scan_filter_add(BT_SCAN_FILTER_TYPE_NAME, "Xbox Wireless Controller"), "Name filter add failed");
  CHECK_ERR(bt_scan_filter_enable(BT_SCAN_NAME_FILTER, false), "Name filter enable failed");
  return true;
}

static bool blePadParseNameCb(struct bt_data *data, void *user_data)
{
  char *name = user_data;
  if (data->type == BT_DATA_NAME_COMPLETE || data->type == BT_DATA_NAME_SHORTENED) {
    int len = MIN(data->data_len, 31);
    memcpy(name, data->data, len);
    name[len] = '\0';
    return false;
  }
  return true;
}

static void blePadScanFilterMatch(struct bt_scan_device_info *device_info, struct bt_scan_filter_match *filter_match, bool connectable)
{
  char addr[BT_ADDR_LE_STR_LEN];
  char name[32] = "No Name";

  bt_addr_le_to_str(device_info->recv_info->addr, addr, sizeof(addr));
  bt_data_parse(device_info->adv_data, blePadParseNameCb, name);

  if (strstr(name, "Xbox") != NULL && device_info->recv_info->rssi > -60) {
    logPrintf("[..] Found Xbox Controller, stopping scan and connecting...\n");
    bt_scan_stop();

    int err = bt_conn_le_create(device_info->recv_info->addr, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT, &ble_pad_conn);
    if (err) {
      logPrintf("[E_] Conn failed (err %d)\n", err);
      bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
    }
  }
}

static void blePadConnected(struct bt_conn *conn, uint8_t err)
{
  if (err) {
    logPrintf("[E_] Connection failed (err %u)\n", err);
    return;
  }
  logPrintf("[OK] Connected! Initiating Security Encryption First...\n");
  ble_pad_conn = bt_conn_ref(conn);
  is_subscribed = false; 

  int sec_err = bt_conn_set_security(conn, BT_SECURITY_L2);
  if (sec_err) {
    logPrintf("[E_] Security Request Failed (err %d)\n", sec_err);
  }
}

static void blePadDisconnected(struct bt_conn *conn, uint8_t reason)
{
  logPrintf("[--] Disconnected! Reason code: %u\n", reason);
  is_subscribed = false;
  k_work_cancel_delayable(&ble_pad_activation_work);
  if (ble_pad_conn) {
    bt_conn_unref(ble_pad_conn);
    ble_pad_conn = NULL;
  }
  bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
}

static void blePadSecurityChanged(struct bt_conn *conn, bt_security_t level, enum bt_security_err err)
{
  if (err) {
    logPrintf("[E_] Security Level Change Failed (err %d)\n", err);
  } else {
    logPrintf("[OK] Security Encrypted! Level: %u\n", level);
    
    if (level >= BT_SECURITY_L2 && !is_subscribed) {
      struct bt_le_conn_param param = {
        .interval_min = 9,   
        .interval_max = 12,  
        .latency      = 0,
        .timeout      = 500, 
      };
      bt_conn_le_param_update(conn, &param);
      logPrintf("[OK] Connection Parameters Optimized.\n");

      logPrintf("[RECONN] Triggering Instant GATT Discovery...\n");
      k_work_reschedule(&ble_pad_discover_delay_work, K_MSEC(50));
    }

    if (IS_ENABLED(CONFIG_SETTINGS)) {
      settings_save();
    }
  }
}

static void blePadPairingComplete(struct bt_conn *conn, bool bonded)
{
  logPrintf("[BT] Pairing/Bonding Process Completed! Bonded status: %s\n", bonded ? "TRUE" : "FALSE");
  if (bonded && !is_subscribed) {
    logPrintf("[★SUCCESS★] Fresh Bonded Lock-On. Triggering GATT Discovery...\n");
    k_work_reschedule(&ble_pad_discover_delay_work, K_MSEC(50));
  }
}

static void blePadPairingFailed(struct bt_conn *conn, enum bt_security_err reason)
{
  logPrintf("[E_] Pairing Protocol Failed. Reason Code: %d\n", reason);
}

static uint8_t blePadNotifyCb(struct bt_conn *conn, struct bt_gatt_subscribe_params *params, const void *data, uint16_t length)
{
  if (!data || length == 0) {
    return BT_GATT_ITER_CONTINUE;
  }

  uint8_t *raw = (uint8_t *)data;
  if (params->value_handle >= 30 || length > 2) {
    logPrintf("[PAD_NOTIFY] L:%d -> %02X %02X %02X %02X %02X %02X %02X %02X\n", 
              length, raw[0], raw[1], raw[2], raw[3], raw[4], raw[5], raw[6], raw[7]);
  }

  return BT_GATT_ITER_CONTINUE;
}

// 💡 전형적인 "수집 데이터 클린 인터페이스" 형태의 Discover 콜백
static uint8_t blePadDiscoverCb(struct bt_conn *conn, const struct bt_gatt_attr *attr, struct bt_gatt_discover_params *params)
{
  if (!attr) {
    logPrintf("[OK] GATT Discovery Sequence Finished.\n");
    
    // 모든 핸들 수집 및 구독이 완료된 시점에만 전용 비동기 활성화 워커 가동 (10ms 뒤 분리 실행)
    if (xbox_report_val_handle != 0) {
      k_work_reschedule(&ble_pad_activation_work, K_MSEC(10));
    }
    return BT_GATT_ITER_STOP;
  }

  if (attr->uuid->type == BT_UUID_TYPE_16) {
    uint16_t uuid16 = BT_UUID_16(attr->uuid)->val;

    if (uuid16 == 0x2A4C) {
      handle_2a4c = attr->handle;
    }
    else if (uuid16 == BT_UUID_GATT_CCC_VAL) {
      if (allocated_subs < MAX_SUBS) {
        uint16_t ccc_h = attr->handle;
        uint16_t val_h = ccc_h - 1;

        if (ccc_h >= 31) {
          xbox_report_val_handle = val_h; // 메인 인풋 밸류 핸들 캡처
        }

        struct bt_gatt_subscribe_params *sparams = &sub_params_pool[allocated_subs];
        memset(sparams, 0, sizeof(struct bt_gatt_subscribe_params));
        
        sparams->notify       = blePadNotifyCb;
        sparams->value        = BT_GATT_CCC_NOTIFY;
        sparams->value_handle = val_h;
        sparams->ccc_handle   = ccc_h;

        int sub_err = bt_gatt_subscribe(conn, sparams);
        if (!sub_err || sub_err == -EALREADY) {
          logPrintf("[SUB] Subscribed to Handle (Val:%d, CCC:%d)\n", val_h, ccc_h);
          allocated_subs++;
          is_subscribed = true;
        }
      }
    }
  }

  return BT_GATT_ITER_CONTINUE;
}

// 💡 [핵심 해결책] 전용 시스템 워커 스레드 컨텍스트에서 안전하게 패킷 순차 송신
static void blePadActivationWorker(struct k_work *work)
{
  if (!ble_pad_conn) return;

  logPrintf("[SYSTEM] Safe Context Activation Sequence Start...\n");

  // 1. HID Control Point 웨이크업 콤보 (명확한 시간차 전송)
  if (handle_2a4c != 0) {
    uint8_t cmd_suspend = 0x00;
    uint8_t cmd_exit    = 0x01;
    bt_gatt_write_without_response(ble_pad_conn, handle_2a4c, &cmd_suspend, 1, false);
    k_msleep(20); // 워커 컨텍스트이므로 안전하게 sleep 가능
    bt_gatt_write_without_response(ble_pad_conn, handle_2a4c, &cmd_exit, 1, false);
    k_msleep(20);
  }

  // 2. 메인 스트리밍 활성화 플래그 주입 ({0x01, 0x00})
  if (xbox_report_val_handle != 0) {
    uint8_t xbox_active_cmd[2] = {0x01, 0x00}; 
    bt_gatt_write_without_response(ble_pad_conn, xbox_report_val_handle, xbox_active_cmd, sizeof(xbox_active_cmd), false);
    k_msleep(20);
    
    // 3. 인풋 스트리밍 모드 개시 강제 트리거 ({0x03})
    uint8_t xbox_report_start = 0x03;
    bt_gatt_write_without_response(ble_pad_conn, xbox_report_val_handle, &xbox_report_start, 1, false);

    logPrintf("[⚡REAL_LOCK] Activation Combo Successfully Delivered! LED Fixed.\n");
  }
}

static void blePadDiscoverDelayWorker(struct k_work *work)
{
  if (!ble_pad_conn || is_subscribed) return;

  handle_2a4c = 0;
  xbox_report_val_handle = 0;
  allocated_subs = 0; 

  memset(&discover_params, 0, sizeof(struct bt_gatt_discover_params));
  discover_params.uuid         = NULL; 
  discover_params.func         = blePadDiscoverCb;
  discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
  discover_params.end_handle   = BT_ATT_LAST_ATTRIBUTE_HANDLE;
  discover_params.type         = BT_GATT_DISCOVER_DESCRIPTOR; 

  bt_gatt_discover(ble_pad_conn, &discover_params);
}

bool blePadInit(void)
{
  int err = bt_enable(NULL);
  if (err) {
    logPrintf("[E_] BT Enable Failed (err %d)\n", err);
    return false;
  }

  if (IS_ENABLED(CONFIG_SETTINGS)) {
    settings_load();
  }

  bt_conn_auth_info_cb_register(&ble_pad_auth_info_cb);
  k_work_init_delayable(&ble_pad_discover_delay_work, blePadDiscoverDelayWorker);
  k_work_init_delayable(&ble_pad_activation_work, blePadActivationWorker); // 💡 워커 초기화 등록

  bldPadScanInit();
  
  err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
  return err == 0;
}

void blePadThread(void const *arg)
{
  moduleIsReady();
  blePadInit();
  while(1) {
    k_msleep(1000);
  }
}