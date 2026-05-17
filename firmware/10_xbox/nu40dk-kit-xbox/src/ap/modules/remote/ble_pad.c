#include "ap_def.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

LOG_MODULE_REGISTER(ble_pad, LOG_LEVEL_INF);

#define MAX_SUBS 3

static bool init(void);
static bool blePadInit(void);
static void blePadThread(void const *arg);

static void blePadScanRecvCb(const struct bt_le_scan_recv_info *info, struct net_buf_simple *ad);
static void blePadConnected(struct bt_conn *conn, uint8_t err);
static void blePadDisconnected(struct bt_conn *conn, uint8_t reason);
static void blePadSecurityChanged(struct bt_conn *conn, bt_security_t level, enum bt_security_err err);
static void blePadPairingComplete(struct bt_conn *conn, bool bonded);
static void blePadPairingFailed(struct bt_conn *conn, enum bt_security_err reason);

static void blePadDiscoverDelayWorker(struct k_work *work);
static void blePadActivationWorker(struct k_work *work); 
static void blePadCountBondedCb(const struct bt_bond_info *info, void *user_data);

MODULE_DEF(ble_pad){
  .name     = "ble_pad",
  .priority = MODULE_PRI_LOW,
  .init     = init};

static K_THREAD_STACK_DEFINE(thread_stack, _HW_DEF_RTOS_THREAD_MEM_BLEPAD);
static struct k_thread thread_data;
static struct bt_conn *ble_pad_conn;

static struct k_work_delayable ble_pad_discover_delay_work;
static struct k_work_delayable ble_pad_activation_work; 

static struct bt_gatt_discover_params  discover_params;
static struct bt_gatt_subscribe_params sub_params_pool[MAX_SUBS];
static uint8_t                         allocated_subs = 0;

static uint16_t     handle_2a4c            = 0;
static uint16_t     xbox_report_val_handle = 0;
static bool         is_subscribed          = false;
static bt_addr_le_t bonded_pad_addr;
static bool         has_bonded_pad = false;

static struct bt_le_scan_cb ble_pad_scan_callbacks = {
  .recv = blePadScanRecvCb,
};

BT_CONN_CB_DEFINE(ble_pad_conn_callbacks) = {
  .connected        = blePadConnected,
  .disconnected     = blePadDisconnected,
  .security_changed = blePadSecurityChanged,
};

static struct bt_conn_auth_info_cb ble_pad_auth_info_cb = {
  .pairing_complete = blePadPairingComplete, // 하단 정의와 일치하도록 수정
  .pairing_failed   = blePadPairingFailed,
};

// 💡 [개방 튜닝] interval과 window를 동일하게 맞추어 라디오를 100% 상시 스캔 모드로 전환 (패킷 누락 방지)
static const struct bt_le_scan_param ble_pad_scan_param = {
  .type     = BT_LE_SCAN_TYPE_ACTIVE,
  .options  = BT_LE_SCAN_OPT_NONE,          
  .interval = BT_GAP_SCAN_FAST_INTERVAL, // 60ms
  .window   = BT_GAP_SCAN_FAST_INTERVAL, // 💡 30ms -> 60ms로 확장 (100% 무손실 캐치)
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

static bool blePadParseNameCb(struct bt_data *data, void *user_data)
{
  char *name = user_data;
  if (data->type == BT_DATA_NAME_COMPLETE || data->type == BT_DATA_NAME_SHORTENED)
  {
    int len = MIN(data->data_len, 31);
    memcpy(name, data->data, len);
    name[len] = '\0';
    return false;
  }
  return true;
}

static void blePadScanRecvCb(const struct bt_le_scan_recv_info *info, struct net_buf_simple *ad)
{
  char addr_str[BT_ADDR_LE_STR_LEN];
  char name[32] = "No Name";

  if (ble_pad_conn != NULL) {
    return;
  }

  bool is_target = false;

  if (has_bonded_pad)
  {
    struct bt_conn *check_conn = bt_conn_lookup_addr_le(BT_ID_DEFAULT, info->addr);
    if (check_conn)
    {
      struct bt_conn_info conn_info;
      if (bt_conn_get_info(check_conn, &conn_info) == 0)
      {
        if (conn_info.state != BT_CONN_STATE_DISCONNECTED)
        {
          is_target = true;
          bt_addr_le_to_str(info->addr, addr_str, sizeof(addr_str));
          logPrintf("[SYS] Auto-Reconnect: Resolved RPA Identity Address [%s]\n", addr_str);
        }
      }
      bt_conn_unref(check_conn); 
    }
    
    if (!is_target && bt_addr_le_eq(info->addr, &bonded_pad_addr))
    {
      bt_addr_le_to_str(info->addr, addr_str, sizeof(addr_str));
      logPrintf("[SYS] Auto-Reconnect: Physical Address Match [%s]\n", addr_str);
      is_target = true;
    }
  }
  
  if (!is_target)
  {
    struct net_buf_simple ad_copy;
    net_buf_simple_clone(ad, &ad_copy);
    bt_data_parse(&ad_copy, blePadParseNameCb, name);

    if (strstr(name, "Xbox") != NULL)
    {
      bt_addr_le_to_str(info->addr, addr_str, sizeof(addr_str));
      logPrintf("[SYS] Target Found by Name: [%s] (%s)\n", name, addr_str);
      is_target = true;
    }
  }

  if (is_target && info->rssi > -75)
  {
    logPrintf("[..] Stopping scan and connecting to target...\n");
    bt_le_scan_stop();

    // 💡 [속도 튜닝] 생성 시점부터 Xbox 최적 규격(15ms) 파라미터를 다이렉트로 강제 요청
    static struct bt_le_conn_param init_fast_param = {
      .interval_min = 12,  // 15ms
      .interval_max = 12,  // 15ms
      .latency      = 0,
      .timeout      = 200, // 2000ms
    };

    // BT_LE_CONN_PARAM_DEFAULT 대신 init_fast_param 주입
    int err = bt_conn_le_create(info->addr, BT_CONN_LE_CREATE_CONN, &init_fast_param, &ble_pad_conn);
    if (err)
    {
      logPrintf("[E_] Conn failed (err %d)\n", err);
      
      if (err == -EINVAL)
      {
        struct bt_conn *bad_conn = bt_conn_lookup_addr_le(BT_ID_DEFAULT, info->addr);
        if (bad_conn) {
          bt_conn_unref(bad_conn);
          bt_conn_unref(bad_conn); 
        }
      }
      
      ble_pad_conn = NULL;
      bt_le_scan_start(&ble_pad_scan_param, NULL);
    }
  }
}

static void blePadConnected(struct bt_conn *conn, uint8_t err)
{
  if (err)
  {
    logPrintf("[E_] Connection failed (err %u)\n", err);
    return;
  }
  logPrintf("[OK] Connected! Initiating Security Link...\n");
  ble_pad_conn  = bt_conn_ref(conn);
  is_subscribed = false;

  // 💡 기존의 후속 파라미터 업데이트 요청 코드는 제거 (생성 시점에 이미 고속 주입됨)

  int sec_err = bt_conn_set_security(conn, BT_SECURITY_L2);
  if (sec_err)
  {
    logPrintf("[E_] Security Request Failed (err %d)\n", sec_err);
  }
}

static void blePadDisconnected(struct bt_conn *conn, uint8_t reason)
{
  logPrintf("[--] Disconnected! Reason code: %u\n", reason);
  is_subscribed = false;
  k_work_cancel_delayable(&ble_pad_activation_work);
  
  if (ble_pad_conn)
  {
    bt_conn_unref(ble_pad_conn);
    ble_pad_conn = NULL;
  }

  has_bonded_pad = false;
  bt_foreach_bond(BT_ID_DEFAULT, blePadCountBondedCb, NULL);

  int err = bt_le_scan_start(&ble_pad_scan_param, NULL);
  if (err && err != -EALREADY) {
    logPrintf("[WARN] Scan start failed (err %d), retrying...\n", err);
  } else {
    logPrintf("[SCAN] Scanning restarted. Waiting for Xbox Controller power-on...\n");
  }
}

static void blePadSecurityChanged(struct bt_conn *conn, bt_security_t level, enum bt_security_err err)
{
  if (err)
  {
    logPrintf("[E_] Security Level Change Failed (err %d)\n", err);
  }
  else
  {
    logPrintf("[OK] Security Encrypted! Level: %u\n", level);

    if (level >= BT_SECURITY_L2)
    {
      logPrintf("[RECONN] Triggering Instant GATT Discovery...\n");
      k_work_reschedule(&ble_pad_discover_delay_work, K_MSEC(10));
    }

    if (IS_ENABLED(CONFIG_SETTINGS))
    {
      settings_save();
    }
  }
}

static void blePadPairingComplete(struct bt_conn *conn, bool bonded)
{
  logPrintf("[BT] Pairing/Bonding Process Completed! Bonded status: %s\n", bonded ? "TRUE" : "FALSE");

  if (bonded)
  {
    has_bonded_pad = false;
    bt_foreach_bond(BT_ID_DEFAULT, blePadCountBondedCb, NULL);
  }
}

static void blePadPairingFailed(struct bt_conn *conn, enum bt_security_err reason)
{
  logPrintf("[E_] Pairing Protocol Failed. Reason Code: %d\n", reason);
}

static uint8_t blePadNotifyCb(struct bt_conn *conn, struct bt_gatt_subscribe_params *params, const void *data, uint16_t length)
{
  if (!data || length == 0)
  {
    return BT_GATT_ITER_CONTINUE;
  }

  uint8_t *raw = (uint8_t *)data;
  if (params->value_handle >= 30 || length > 2)
  {
    logPrintf("[PAD_NOTIFY] L:%d -> %02X %02X %02X %02X %02X %02X %02X %02X\n",
              length, raw[0], raw[1], raw[2], raw[3], raw[4], raw[5], raw[6], raw[7]);
  }

  return BT_GATT_ITER_CONTINUE;
}

static uint8_t blePadDiscoverCb(struct bt_conn *conn, const struct bt_gatt_attr *attr, struct bt_gatt_discover_params *params)
{
  if (!attr)
  {
    logPrintf("[OK] GATT Discovery Sequence Finished.\n");

    if (xbox_report_val_handle != 0)
    {
      k_work_reschedule(&ble_pad_activation_work, K_MSEC(10));
    }
    return BT_GATT_ITER_STOP;
  }

  if (attr->uuid->type == BT_UUID_TYPE_16)
  {
    uint16_t uuid16 = BT_UUID_16(attr->uuid)->val;

    if (uuid16 == 0x2A4C)
    {
      handle_2a4c = attr->handle;
    }
    else if (uuid16 == BT_UUID_GATT_CCC_VAL)
    {
      if (allocated_subs < MAX_SUBS)
      {
        uint16_t ccc_h = attr->handle;
        uint16_t val_h = ccc_h - 1;

        if (ccc_h >= 31)
        {
          xbox_report_val_handle = val_h; 
        }

        struct bt_gatt_subscribe_params *sparams = &sub_params_pool[allocated_subs];
        memset(sparams, 0, sizeof(struct bt_gatt_subscribe_params));

        sparams->notify       = blePadNotifyCb;
        sparams->value        = BT_GATT_CCC_NOTIFY;
        sparams->value_handle = val_h;
        sparams->ccc_handle   = ccc_h;

        int sub_err = bt_gatt_subscribe(conn, sparams);
        if (!sub_err || sub_err == -EALREADY)
        {
          logPrintf("[SUB] Subscribed to Handle (Val:%d, CCC:%d)\n", val_h, ccc_h);
          allocated_subs++;
          is_subscribed = true;
        }
      }
    }
  }

  return BT_GATT_ITER_CONTINUE;
}

static void blePadActivationWorker(struct k_work *work)
{
  if (!ble_pad_conn) return;

  logPrintf("[SYSTEM] Safe Context Activation Sequence Start...\n");
  k_msleep(50);

  if (handle_2a4c != 0)
  {
    uint8_t cmd_suspend = 0x00;
    uint8_t cmd_exit    = 0x01;
    bt_gatt_write_without_response(ble_pad_conn, handle_2a4c, &cmd_suspend, 1, false);
    k_msleep(50);
    bt_gatt_write_without_response(ble_pad_conn, handle_2a4c, &cmd_exit, 1, false);
    k_msleep(50);
  }

  if (xbox_report_val_handle != 0)
  {
    uint8_t xbox_active_cmd[2] = {0x01, 0x00};
    bt_gatt_write_without_response(ble_pad_conn, xbox_report_val_handle, xbox_active_cmd, sizeof(xbox_active_cmd), false);
    k_msleep(50);

    uint8_t xbox_report_start = 0x03;
    bt_gatt_write_without_response(ble_pad_conn, xbox_report_val_handle, &xbox_report_start, 1, false);

    logPrintf("[⚡REAL_LOCK] Activation Combo Successfully Delivered! LED Fixed.\n");
  }
}

static void blePadDiscoverDelayWorker(struct k_work *work)
{
  if (!ble_pad_conn) return;

  handle_2a4c            = 0;
  xbox_report_val_handle = 0;
  allocated_subs         = 0;

  memset(&discover_params, 0, sizeof(struct bt_gatt_discover_params));
  discover_params.uuid         = NULL;
  discover_params.func         = blePadDiscoverCb;
  discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
  discover_params.end_handle   = BT_ATT_LAST_ATTRIBUTE_HANDLE;
  discover_params.type         = BT_GATT_DISCOVER_DESCRIPTOR;

  bt_gatt_discover(ble_pad_conn, &discover_params);
}

static void blePadCountBondedCb(const struct bt_bond_info *info, void *user_data)
{
  bt_addr_le_copy(&bonded_pad_addr, &info->addr);
  has_bonded_pad = true;
}

bool blePadInit(void)
{
  int err = bt_enable(NULL);
  if (err)
  {
    logPrintf("[E_] BT Enable Failed (err %d)\n", err);
    return false;
  }

  if (IS_ENABLED(CONFIG_SETTINGS))
  {
    logPrintf("[SYS] Loading saved Bluetooth settings & identity...\n");
    settings_load();
  }

  bt_conn_auth_info_cb_register(&ble_pad_auth_info_cb);
  bt_le_scan_cb_register(&ble_pad_scan_callbacks);

  k_work_init_delayable(&ble_pad_discover_delay_work, blePadDiscoverDelayWorker);
  k_work_init_delayable(&ble_pad_activation_work, blePadActivationWorker); 

  has_bonded_pad = false;
  bt_foreach_bond(BT_ID_DEFAULT, blePadCountBondedCb, NULL);

  err = bt_le_scan_start(&ble_pad_scan_param, NULL);
  if (!err)
  {
    logPrintf("[SYS] Scanning Started. Waiting for Pad...\n");
  }

  return err == 0;
}

void blePadThread(void const *arg)
{
  moduleIsReady();
  blePadInit();
  while (1)
  {
    k_msleep(1000);
  }
}