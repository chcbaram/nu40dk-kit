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

static uint16_t        handle_2a4c            = 0;
static uint16_t        xbox_report_val_handle = 0;
static bool            is_subscribed          = false;
static bt_addr_le_t    bonded_pad_addr;
static bool            has_bonded_pad = false;
static struct bt_conn *ble_pad_conn;
static volatile bool   is_connecting_or_connected = false;


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

  // 💡 [안전장치 1] 이미 연결 시도 중이거나 연결된 상태라면 스캔 데이터는 무조건 버립니다.
  if (is_connecting_or_connected || ble_pad_conn != NULL)
  {
    return;
  }

  // 💡 [안전장치 2] Zephyr 내부에 이 주소로 된 객체가 조금이라ve도 남아있다면 즉시 차단
  struct bt_conn *already_conn = bt_conn_lookup_addr_le(BT_ID_DEFAULT, info->addr);
  if (already_conn)
  {
    bt_conn_unref(already_conn); // 찌꺼기 참조 해제
    return;
  }

  bool is_target = false;

  // 1. 본딩된 패드 자동 연결 판단
  if (has_bonded_pad)
  {
    if (bt_addr_le_eq(info->addr, &bonded_pad_addr))
    {
      bt_addr_le_to_str(info->addr, addr_str, sizeof(addr_str));
      logPrintf("[SYS] Auto-Reconnect: Physical Address Match [%s]\n", addr_str);
      is_target = true;
    }
  }

  // 2. 페어링 모드 등 이름 기반 판단
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

  // 최종 연결 진입
  if (is_target && info->rssi > -75)
  {
    // 중복 진입 방지를 위해 플래시 속도로 스캔 중지 및 플래그 락(Lock)
    bt_le_scan_stop();
    is_connecting_or_connected = true; 
    logPrintf("[..] Stopping scan and connecting to target...\n");

    static struct bt_le_conn_param init_fast_param = {
      .interval_min = 12,
      .interval_max = 12,
      .latency      = 0,
      .timeout      = 200,
    };

    int err = bt_conn_le_create(info->addr, BT_CONN_LE_CREATE_CONN, &init_fast_param, &ble_pad_conn);
    if (err)
    {
      logPrintf("[E_] Conn failed (err %d)\n", err);
      
      // 완전히 실패한 것이므로 락 해제 및 초기화
      is_connecting_or_connected = false;
      if (ble_pad_conn) {
        bt_conn_unref(ble_pad_conn);
        ble_pad_conn = NULL;
      }
      
      k_msleep(200); // 이전 상태가 정리될 충분한 시간 부여
      bt_le_scan_start(&ble_pad_scan_param, NULL);
    }
  }
}

static void blePadConnected(struct bt_conn *conn, uint8_t err)
{
  if (err)
  {
    logPrintf("[E_] Connection establishment failed (err %u)\n", err);

    // 💡 연결 생성에 실패했으므로 안전하게 락을 풀고 구조체를 정리합니다.
    is_connecting_or_connected = false;
    if (ble_pad_conn)
    {
      bt_conn_unref(ble_pad_conn);
      ble_pad_conn = NULL;
    }

    // 완전히 해제될 시간을 충분히 벌어준 뒤 스캔 재시작 (-22 에러 원천 차단)
    k_msleep(300);
    bt_le_scan_start(&ble_pad_scan_param, NULL);
    return;
  }

  logPrintf("[OK] Connected! Initiating Security Link...\n");

  // 이미 생성 시점에 레퍼런스가 증가하므로 중복 reference 방지를 위해
  // 기존의 ble_pad_conn = bt_conn_ref(conn); 대신 상태 확인 후 할당 유지
  is_connecting_or_connected = true;
  is_subscribed              = false;

  int sec_err = bt_conn_set_security(conn, BT_SECURITY_L2);
  if (sec_err)
  {
    logPrintf("[E_] Security Request Failed (err %d)\n", sec_err);
  }
}

static void blePadDisconnected(struct bt_conn *conn, uint8_t reason)
{
  logPrintf("[--] Disconnected! Reason code: %u\n", reason);

  // 💡 완전히 연결이 끊어졌으므로 모든 락을 해제합니다.
  is_connecting_or_connected = false;
  is_subscribed              = false;
  k_work_cancel_delayable(&ble_pad_activation_work);

  if (ble_pad_conn)
  {
    bt_conn_unref(ble_pad_conn);
    ble_pad_conn = NULL;
  }

  has_bonded_pad = false;
  bt_foreach_bond(BT_ID_DEFAULT, blePadCountBondedCb, NULL);

  // 💡 delay(200) 대신 시스템을 정지시키지 않는 k_msleep 사용 필수
  k_msleep(300);

  int err = bt_le_scan_start(&ble_pad_scan_param, NULL);
  if (err && err != -EALREADY)
  {
    logPrintf("[WARN] Scan start failed (err %d), retrying...\n", err);
  }
  else
  {
    logPrintf("[SCAN] Scanning restarted. Waiting for Xbox Controller power-on...\n");
  }
}

static void blePadSecurityChanged(struct bt_conn *conn, bt_security_t level, enum bt_security_err err)
{
  if (err)
  {
    logPrintf("[E_] Security Level Change Failed (err %d) -> Evicting Bad Bond...\n", err);

    // 💡 [핵심] 암호화 실패(err 9 등) 시, 저장된 본딩 정보가 깨진 것이므로 과감히 지웁니다.
    if (ble_pad_conn)
    {
      // 현재 연결된 타겟의 주소를 가져와 본딩 해제
      struct bt_conn_info info;
      if (bt_conn_get_info(ble_pad_conn, &info) == 0)
      {
        // 본딩 데이터 완전히 플래시에서 삭제
        bt_unpair(BT_ID_DEFAULT, info.le.dst);
        logPrintf("[SYS] Flushed corrupted bond data for this device. Please re-pair.\n");
      }

      // 고장 난 연결은 끊어버리고 재시작 유도
      bt_conn_disconnect(ble_pad_conn, BT_HCI_ERR_AUTH_FAIL);
    }
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

// 💡 전방 선언 (2단계 디스크립터 탐색을 위함)
static uint8_t blePadDiscoverDescriptorCb(struct bt_conn *conn, const struct bt_gatt_attr *attr, struct bt_gatt_discover_params *params);

// 💡 [수정] 1단계: 특성(Characteristic) 탐색 콜백
static uint8_t blePadDiscoverCharacteristicCb(struct bt_conn *conn, const struct bt_gatt_attr *attr, struct bt_gatt_discover_params *params)
{
  if (!attr)
  {
    logPrintf("[SYS] Phase 1: Characteristic Discovery Finished. Starting Phase 2: Descriptor Scan...\n");

    // 2단계: CCCD 디스크립터 스캔 시작
    memset(&discover_params, 0, sizeof(struct bt_gatt_discover_params));
    discover_params.uuid         = NULL;
    discover_params.func         = blePadDiscoverDescriptorCb; // 디스크립터 전용 콜백으로 체인 연결
    discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    discover_params.end_handle   = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    discover_params.type         = BT_GATT_DISCOVER_DESCRIPTOR; // 디스크립터 모드 전환

    int err = bt_gatt_discover(conn, &discover_params);
    if (err) {
      logPrintf("[E_] Phase 2 Discovery failed (err %d)\n", err);
    }
    return BT_GATT_ITER_STOP;
  }

  // 특성(Characteristic) 값 구조체 파싱
  const struct bt_gatt_chrc *chrc = (const struct bt_gatt_chrc *)attr->user_data;
  if (chrc && chrc->uuid->type == BT_UUID_TYPE_16)
  {
    uint16_t uuid16 = BT_UUID_16(chrc->uuid)->val;
    if (uuid16 == 0x2A4C)
    {
      // 💡 chrc->value_handle이 0x2A4C 특성에 무선 데이터를 직접 쏠 수 있는 정확한 Value Handle 하드웨어 주소입니다.
      handle_2a4c = chrc->value_handle;
      logPrintf("[OK] Found Real 0x2A4C Handle via Characteristic Scan: %d\n", handle_2a4c);
    }
  }

  return BT_GATT_ITER_CONTINUE;
}

// 💡 [수정] 2단계: 디스크립터(Descriptor) 탐색 콜백
static uint8_t blePadDiscoverDescriptorCb(struct bt_conn *conn, const struct bt_gatt_attr *attr, struct bt_gatt_discover_params *params)
{
  if (!attr)
  {
    logPrintf("[OK] All GATT Discovery Sequences Finished.\n");

    if (xbox_report_val_handle != 0)
    {
      k_work_reschedule(&ble_pad_activation_work, K_MSEC(1000));
    }
    return BT_GATT_ITER_STOP;
  }

  if (attr->uuid->type == BT_UUID_TYPE_16)
  {
    uint16_t uuid16 = BT_UUID_16(attr->uuid)->val;

    if (uuid16 == BT_UUID_GATT_CCC_VAL)
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
    logPrintf("[  ] Writing to genuine handle_2a4c: %d\n", handle_2a4c);
    
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

  // 💡 [수정] 최초 구동 시 특성(Characteristic) 탐색 모드로 시작합니다.
  memset(&discover_params, 0, sizeof(struct bt_gatt_discover_params));
  discover_params.uuid         = NULL;
  discover_params.func         = blePadDiscoverCharacteristicCb; // 특성 전용 콜백 설정
  discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
  discover_params.end_handle   = BT_ATT_LAST_ATTRIBUTE_HANDLE;
  discover_params.type         = BT_GATT_DISCOVER_CHARACTERISTIC; // 💡 특성 스캔 명시

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

  delay(100);

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