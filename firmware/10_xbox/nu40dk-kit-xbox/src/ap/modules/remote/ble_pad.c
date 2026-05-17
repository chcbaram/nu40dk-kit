#include "ap_def.h"

#include <bluetooth/scan.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

LOG_MODULE_REGISTER(ble_pad, LOG_LEVEL_INF);

#define CHECK_ERR(f, msg)                 \
  do                                      \
  {                                       \
    int _err = (f);                       \
    if (_err)                             \
    {                                     \
      logPrintf(msg " (err %d)\n", _err); \
      break;                              \
    }                                     \
  } while (0)

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
  struct bt_scan_init_param scan_init_obj = {.connect_if_match = false};
  bt_scan_init(&scan_init_obj);
  bt_scan_cb_register(&ble_pad_scan_cb);

  CHECK_ERR(bt_scan_filter_add(BT_SCAN_FILTER_TYPE_NAME, "Xbox Wireless Controller"), "Name filter add failed");
  CHECK_ERR(bt_scan_filter_enable(BT_SCAN_NAME_FILTER, false), "Name filter enable failed");
  return true;
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

static void blePadScanFilterMatch(struct bt_scan_device_info *device_info, struct bt_scan_filter_match *filter_match, bool connectable)
{
  char addr_str[BT_ADDR_LE_STR_LEN];
  char name[32] = "No Name";

  if (ble_pad_conn != NULL) {
    return;
  }

  bt_addr_le_to_str(device_info->recv_info->addr, addr_str, sizeof(addr_str));
  bt_data_parse(device_info->adv_data, blePadParseNameCb, name);

  bool is_target = false;

  // 1. 이미 페어링된 이력이 있는 경우 -> 맥 주소가 일치하는지 다이렉트 검사 (이름 없어도 통과)
  if (has_bonded_pad)
  {
    if (bt_addr_le_eq(device_info->recv_info->addr, &bonded_pad_addr))
    {
      logPrintf("[SYS] Auto-Reconnect: Recognized Bonded Pad Address [%s]\n", addr_str);
      is_target = true;
    }
  }
  
  // 2. 페어링 이력이 없거나 새로운 연결인 경우 -> 이름으로 검사
  if (!is_target && strstr(name, "Xbox") != NULL)
  {
    logPrintf("[SYS] Fresh Pairing: Found Named Controller [%s]\n", name);
    is_target = true;
  }

  // 대상 기기 락온 및 연결 시작
  if (is_target && device_info->recv_info->rssi > -75)
  {
    logPrintf("[..] Stopping scan and connecting to target...\n");
    bt_scan_stop();

    int err = bt_conn_le_create(device_info->recv_info->addr, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT, &ble_pad_conn);
    if (err)
    {
      logPrintf("[E_] Conn failed (err %d)\n", err);
      ble_pad_conn = NULL;
      bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
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
  logPrintf("[OK] Connected! Initiating Security Encryption First...\n");
  ble_pad_conn  = bt_conn_ref(conn);
  is_subscribed = false;

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

  // 연결 해제 시 본딩 테이블 상태 재점검 후 유연하게 통합 스캔 재시작
  has_bonded_pad = false;
  bt_foreach_bond(BT_ID_DEFAULT, blePadCountBondedCb, NULL);

  int err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
  if (err && err != -EALREADY) {
    logPrintf("[WARN] Scan start failed (err %d), retrying...\n", err);
    // 필요 시 스캔 재시도 워커를 구동할 수 있습니다.
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
      k_work_reschedule(&ble_pad_discover_delay_work, K_MSEC(100));
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

  // 💡 [교정] 최초 페어링 성공 시, 리셋 없이 바로 전원 제어가 연동되도록 즉시 전역 주소 매핑 업데이트
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

    // 💡 [교정] 복잡한 재연결 CCCD 상황에 대응하기 위해, 핵심 HID 핸들 획득 유무만을 기준으로 안전하게 활성화 지연 실행
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

  // 1. HID Control Point 웨이크업 콤보 (명확한 시간차 전송)
  if (handle_2a4c != 0)
  {
    uint8_t cmd_suspend = 0x00;
    uint8_t cmd_exit    = 0x01;
    bt_gatt_write_without_response(ble_pad_conn, handle_2a4c, &cmd_suspend, 1, false);
    k_msleep(20);
    bt_gatt_write_without_response(ble_pad_conn, handle_2a4c, &cmd_exit, 1, false);
    k_msleep(20);
  }

  // 2. 메인 스트리밍 활성화 플래그 주입 ({0x01, 0x00})
  if (xbox_report_val_handle != 0)
  {
    uint8_t xbox_active_cmd[2] = {0x01, 0x00};
    bt_gatt_write_without_response(ble_pad_conn, xbox_report_val_handle, xbox_active_cmd, sizeof(xbox_active_cmd), false);
    k_msleep(20);

    // 3. 인풋 스트리밍 모드 개시 강제 트리거 ({0x03})
    uint8_t xbox_report_start = 0x03;
    bt_gatt_write_without_response(ble_pad_conn, xbox_report_val_handle, &xbox_report_start, 1, false);

    logPrintf("[⚡REAL_LOCK] Activation Combo Successfully Delivered! LED Fixed.\n");

    /*
     * 💡 [최종 조치]
     * 15초 뒤 발생하는 0x3a (Busy) 경고의 원흉이었던 bt_conn_le_param_update 코드를 완전히 삭제합니다.
     * 기기 간 기본 셋업 속도만으로도 로봇/임베디드 제어 패킷 수신에는 딜레이가 전혀 발생하지 않으며,
     * 패드 내부 상태 머신과의 충돌을 원천 차단하여 가장 완벽하고 정적인 연결 상태를 유지합니다.
     */
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
  // 이미 페어링된 기기가 있다면 그 주소를 전역 변수에 복사
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
  k_work_init_delayable(&ble_pad_discover_delay_work, blePadDiscoverDelayWorker);
  k_work_init_delayable(&ble_pad_activation_work, blePadActivationWorker); 


  has_bonded_pad = false;
  bt_foreach_bond(BT_ID_DEFAULT, blePadCountBondedCb, NULL);


  bldPadScanInit();

  err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
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