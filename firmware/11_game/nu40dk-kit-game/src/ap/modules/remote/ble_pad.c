#include "ble_pad.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/reboot.h>


LOG_MODULE_REGISTER(ble_pad, LOG_LEVEL_INF);


#define lock()      k_mutex_lock(&mutex_lock, K_FOREVER);
#define unLock()    k_mutex_unlock(&mutex_lock);


#define MAX_SUBS 6




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

#ifdef _USE_HW_CLI
static void cliCmd(cli_args_t *args);
#endif

MODULE_DEF(ble_pad){
  .name     = "ble_pad",
  .priority = MODULE_PRI_LOW,
  .init     = init};

static K_THREAD_STACK_DEFINE(thread_stack, _HW_DEF_RTOS_THREAD_MEM_BLEPAD);
static struct k_thread thread_data;
static struct bt_conn *ble_pad_conn;

static struct k_work_delayable ble_pad_discover_delay_work;
static struct k_work_delayable ble_pad_activation_work; 
static struct k_work_delayable ble_pad_scan_work;         // 스캔 재시작 (BT 콜백 안에서 delay 하지 않도록)
static struct k_work_delayable ble_pad_battery_work;      // 배터리 주기 읽기

static struct bt_gatt_discover_params  discover_params;
static struct bt_gatt_subscribe_params sub_params_pool[MAX_SUBS];
static struct bt_gatt_read_params      rm_read_params;
static uint8_t                         allocated_subs = 0;

static uint16_t        handle_2a4c            = 0;
static uint16_t        handle_2a4b            = 0;
static uint16_t        xbox_report_val_handle = 0;
static uint8_t           latch_btn[BLE_PAD_LATCH_MAX][2];   // 보고서 13, 14 바이트 (버튼) OR
static uint8_t           latch_dpad[BLE_PAD_LATCH_MAX];
static bool            is_subscribed          = false;
static bt_addr_le_t    bonded_pad_addr;
static bool            has_bonded_pad = false;
static volatile bool   is_connecting_or_connected = false;
static bool            is_connected               = false;
static bool            is_init                    = false;
static uint16_t        rx_cnt                     = 0;
static struct k_timer  keepalive_timer;
static volatile bool   scan_enable                = true;   // 자동 스캔 허용 (런처에서 끌 수 있음)
static volatile bool   is_scanning                = false;
static uint16_t        handle_battery             = 0;      // Battery Level (0x2A19)
static volatile int8_t battery_level              = -1;     // 0~100, 모르면 -1
static volatile int8_t pad_rssi                   = 0;
static char            pad_name[32]               = "";
static struct bt_gatt_read_params battery_read_params;

static xbox_hid_report_t xbox_report;

static K_MUTEX_DEFINE(mutex_lock);


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

// 보고서 초기값: 스틱은 중앙(32768). 0 으로 두면 패드가 스틱 데이터를 보내기 전까지
// "왼쪽 위로 꺾음" 으로 읽혀 메뉴가 계속 움직인다
static void blePadResetReport(xbox_hid_report_t *p_report)
{
  memset(p_report, 0, sizeof(xbox_hid_report_t));
  p_report->left_stick_x  = 32768;
  p_report->left_stick_y  = 32768;
  p_report->right_stick_x = 32768;
  p_report->right_stick_y = 32768;
}

static void blePadScanStartNow(void)
{
  if (!scan_enable || is_connecting_or_connected || ble_pad_conn != NULL)
  {
    return;
  }

  int err = bt_le_scan_start(&ble_pad_scan_param, NULL);
  if (err == 0 || err == -EALREADY)
  {
    is_scanning = true;
    logPrintf("[SCAN] Scanning started. Waiting for Xbox Controller...\n");
  }
  else
  {
    logPrintf("[WARN] Scan start failed (err %d), retry...\n", err);
    k_work_reschedule(&ble_pad_scan_work, K_MSEC(500));
  }
}

static void blePadScanWorker(struct k_work *work)
{
  blePadScanStartNow();
}

// BT 콜백에서 호출: 지연 후 스캔 재시작 (이전 연결 정리 시간 확보)
static void blePadScanRestartLater(uint32_t ms)
{
  k_work_reschedule(&ble_pad_scan_work, K_MSEC(ms));
}

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
    is_scanning = false;
    is_connecting_or_connected = true; 
    pad_rssi = info->rssi;
    if (strcmp(name, "No Name") != 0) snprintf(pad_name, sizeof(pad_name), "%s", name);
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
      
      blePadScanRestartLater(200); // 이전 상태가 정리될 시간 부여 (콜백 안에서 delay 하지 않음)
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
    blePadScanRestartLater(300);
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
  is_connected               = false;
  is_connecting_or_connected = false;
  is_subscribed              = false;
  battery_level              = -1;
  lock();
  blePadResetReport(&xbox_report);
  unLock();
  handle_battery             = 0;
  k_work_cancel_delayable(&ble_pad_activation_work);
  k_work_cancel_delayable(&ble_pad_battery_work);

  logPrintf("[  ] is_connected : false\n");

  if (ble_pad_conn)
  {
    bt_conn_unref(ble_pad_conn);
    ble_pad_conn = NULL;
  }

  has_bonded_pad = false;
  bt_foreach_bond(BT_ID_DEFAULT, blePadCountBondedCb, NULL);

  // BT 콜백 안에서 delay 하지 않고 지연 작업으로 스캔 재시작
  blePadScanRestartLater(300);
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

  // 배터리 알림
  if (handle_battery != 0 && params->value_handle == handle_battery)
  {
    battery_level = ((const uint8_t *)data)[0];
    return BT_GATT_ITER_CONTINUE;
  }

  // 실제 입력 데이터가 들어오는 유효 handle 및 길이 조건 체크
  if (params->value_handle >= 30 && length >= 14)
  {
    lock();
    memcpy(&xbox_report, data, sizeof(xbox_hid_report_t));
    // 읽는 쪽이 바쁜 동안(EPD 갱신 등) 눌렀다 뗀 버튼을 놓치지 않도록 래치
    const uint8_t *p_raw = (const uint8_t *)&xbox_report;
    for (int i = 0; i < BLE_PAD_LATCH_MAX; i++)
    {
      latch_btn[i][0] |= p_raw[13];
      latch_btn[i][1] |= p_raw[14];
      if (xbox_report.dpad != 0) latch_dpad[i] = xbox_report.dpad;
    }
    unLock();
    rx_cnt++;
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

    // 1. HID Control Point (0x2A4C) 탐색
    if (uuid16 == 0x2A4C)
    {      
      handle_2a4c = chrc->value_handle;
      logPrintf("[OK] Found Real 0x2A4C Handle via Characteristic Scan: %d\n", handle_2a4c);
    }

    // 3. Battery Level (0x2A19)
    if (uuid16 == 0x2A19)
    {
      handle_battery = chrc->value_handle;
      logPrintf("[OK] Found Battery Level Handle: %d\n", handle_battery);
    }

    // 2. HID Report Map (0x2A4B) 탐색
    if (uuid16 == 0x2A4B)
    {
      handle_2a4b = chrc->value_handle;
      logPrintf("[OK] Found Real 0x2A4B Handle via Characteristic Scan: %d\n", handle_2a4b);
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
      k_work_reschedule(&ble_pad_activation_work, K_MSEC(100));
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

static uint8_t blePadBatteryReadCb(struct bt_conn *conn, uint8_t err,
                                   struct bt_gatt_read_params *params,
                                   const void *data, uint16_t length)
{
  if (!err && data && length >= 1)
  {
    battery_level = ((const uint8_t *)data)[0];
  }
  return BT_GATT_ITER_STOP;
}

// 연결 후 배터리를 읽고 60 초마다 다시 읽는다 (알림이 오면 그때도 갱신)
static void blePadBatteryWorker(struct k_work *work)
{
  if (!ble_pad_conn || !is_connected || handle_battery == 0) return;

  battery_read_params.func          = blePadBatteryReadCb;
  battery_read_params.handle_count  = 1;
  battery_read_params.single.handle = handle_battery;
  battery_read_params.single.offset = 0;
  bt_gatt_read(ble_pad_conn, &battery_read_params);

  k_work_reschedule(&ble_pad_battery_work, K_SECONDS(60));
}

static uint8_t blePadReportMapReadCb(struct bt_conn *conn, uint8_t err,
                                     struct bt_gatt_read_params *params,
                                     const void *data, uint16_t length)
{
  if (err)
  {
    logPrintf("[ERR] Report Map Read Failed: %d\n", err);
  }
  else
  {
    logPrintf("[OK] Report Map Read Done (Len: %d). Xbox Pad Handshake Complete!\n", length);

    is_connected = true;
    logPrintf("[  ] is_connected : true\n");
    k_work_reschedule(&ble_pad_battery_work, K_MSEC(500));

    // k_timer_start(&keepalive_timer, K_MSEC(1000), K_MSEC(5000)); 
  }
  return BT_GATT_ITER_STOP;
}

static void blePadActivationWorker(struct k_work *work)
{
  if (!ble_pad_conn) return;

  logPrintf("[SYSTEM] Safe Context Activation Sequence Start...\n");
  delay(50);


  if (handle_2a4b != 0)
  {
    rm_read_params.func          = blePadReportMapReadCb;
    rm_read_params.handle_count  = 1;
    rm_read_params.single.handle = handle_2a4b;
    rm_read_params.single.offset = 0;

    int read_err = bt_gatt_read(ble_pad_conn, &rm_read_params);
    if (read_err)
    {
      logPrintf("[ERR] Failed to start Report Map read: %d\n", read_err);
    }
    delay(100); // Read 요청이 스택에 들어갈 시간을 조금 줌
  }

  if (handle_2a4c != 0)
  {
    logPrintf("[  ] Writing to genuine handle_2a4c: %d\n", handle_2a4c);
    
    uint8_t cmd_suspend = 0x00;
    uint8_t cmd_exit    = 0x01;
    bt_gatt_write_without_response(ble_pad_conn, handle_2a4c, &cmd_suspend, 1, false);
    delay(50);
    bt_gatt_write_without_response(ble_pad_conn, handle_2a4c, &cmd_exit, 1, false);
    delay(50);
  }

  if (xbox_report_val_handle != 0)
  {
    uint8_t xbox_active_cmd[2] = {0x01, 0x00};
    bt_gatt_write_without_response(ble_pad_conn, xbox_report_val_handle, xbox_active_cmd, sizeof(xbox_active_cmd), false);
    delay(50);

    uint8_t xbox_report_start = 0x03;
    bt_gatt_write_without_response(ble_pad_conn, xbox_report_val_handle, &xbox_report_start, 1, false);

    logPrintf("[⚡REAL_LOCK] Activation Combo Successfully Delivered! LED Fixed.\n");        
  }
}

static void blePadDiscoverDelayWorker(struct k_work *work)
{
  if (!ble_pad_conn) return;

  handle_2a4c            = 0;
  handle_battery         = 0;
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

void blePadKeepAliveExpiryFn(struct k_timer *timer_id)
{
  if (ble_pad_conn && handle_2a4c != 0)
  {
    uint8_t keepalive_cmd = 0x01;
    bt_gatt_write_without_response(ble_pad_conn, handle_2a4c, &keepalive_cmd, 1, false);
  }
}

bool blePadIsConnected(void)
{
  return is_connected;
}

ble_pad_state_t blePadGetState(void)
{
  if (!is_init)                    return BLE_PAD_STATE_OFF;
  if (is_connected)                return BLE_PAD_STATE_CONNECTED;
  if (is_connecting_or_connected)  return BLE_PAD_STATE_CONNECTING;
  if (is_scanning)                 return BLE_PAD_STATE_SCANNING;
  return BLE_PAD_STATE_IDLE;
}

const char *blePadGetStateStr(void)
{
  static const char *str[] = {"off", "idle", "scanning", "connecting", "connected"};
  return str[blePadGetState()];
}

// 스캔 허용. 연결이 끊기면 자동으로 다시 스캔한다
void blePadScanStart(void)
{
  scan_enable = true;
  k_work_reschedule(&ble_pad_scan_work, K_NO_WAIT);
}

// 스캔 중지. 연결이 끊겨도 다시 스캔하지 않는다
void blePadScanStop(void)
{
  scan_enable = false;
  k_work_cancel_delayable(&ble_pad_scan_work);
  bt_le_scan_stop();
  is_scanning = false;
}

bool blePadIsScanEnabled(void)
{
  return scan_enable;
}

void blePadDisconnect(void)
{
  if (ble_pad_conn)
  {
    bt_conn_disconnect(ble_pad_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
  }
}

// 모든 페어링 정보 삭제 (연결 중이면 끊는다). 이후 패드를 페어링 모드로 두면 다시 연결된다
void blePadClearBonds(void)
{
  blePadDisconnect();
  bt_unpair(BT_ID_DEFAULT, NULL);
  has_bonded_pad = false;
  pad_name[0]    = 0;
}

bool blePadHasBond(void)
{
  return has_bonded_pad;
}

int8_t blePadGetBattery(void)
{
  return battery_level;
}

int8_t blePadGetRssi(void)
{
  return pad_rssi;
}

const char *blePadGetName(void)
{
  return pad_name;
}

// 현재 보고서 + 마지막 호출 이후 한 번이라도 눌린 버튼 (user 별로 따로 래치)
bool blePadGetInfoLatched(uint8_t user, xbox_hid_report_t *p_report)
{
  bool ret;

  if (user >= BLE_PAD_LATCH_MAX) return blePadGetInfo(p_report);

  lock();
  ret = is_connected;
  if (is_connected)
  {
    memcpy(p_report, &xbox_report, sizeof(xbox_report));
    uint8_t *p_raw = (uint8_t *)p_report;
    p_raw[13] |= latch_btn[user][0];
    p_raw[14] |= latch_btn[user][1];
    if (p_report->dpad == 0) p_report->dpad = latch_dpad[user];
  }
  else
  {
    blePadResetReport(p_report);
  }
  latch_btn[user][0] = 0;
  latch_btn[user][1] = 0;
  latch_dpad[user]   = 0;
  unLock();

  return ret;
}

bool blePadGetInfo(xbox_hid_report_t *p_report)
{
  lock();
  if (is_connected)
    memcpy(p_report, &xbox_report, sizeof(xbox_report));
  else
    blePadResetReport(p_report);
  unLock();

  return is_connected;
}

bool blePadInit(void)
{

  lock();
  blePadResetReport(&xbox_report);
  unLock();

  int err = bt_enable(NULL);
  if (err)
  {
    logPrintf("[E_] BT Enable Failed (err %d)\n", err);
    return false;
  }

  k_timer_init(&keepalive_timer, blePadKeepAliveExpiryFn, NULL);

  if (IS_ENABLED(CONFIG_SETTINGS))
  {
    logPrintf("[SYS] Loading saved Bluetooth settings & identity...\n");
    settings_load();
  }

  bt_conn_auth_info_cb_register(&ble_pad_auth_info_cb);
  bt_le_scan_cb_register(&ble_pad_scan_callbacks);

  k_work_init_delayable(&ble_pad_discover_delay_work, blePadDiscoverDelayWorker);
  k_work_init_delayable(&ble_pad_activation_work, blePadActivationWorker); 
  k_work_init_delayable(&ble_pad_scan_work, blePadScanWorker);
  k_work_init_delayable(&ble_pad_battery_work, blePadBatteryWorker);

  has_bonded_pad = false;
  bt_foreach_bond(BT_ID_DEFAULT, blePadCountBondedCb, NULL);

  delay(100);

  is_init = true;
  blePadScanStartNow();

#ifdef _USE_HW_CLI
  cliAdd("blepad", cliCmd);
#endif

  return is_init;
}

void blePadThread(void const *arg)
{
  moduleIsReady();

  blePadInit();

  while (1)
  {
    delay(10);
  }
}

#ifdef _USE_HW_CLI

static uint16_t blePadReportLog(xbox_hid_report_t *joy)
{
  // D-Pad 문자열 변환 (우측 잔상 방지를 위해 고정 10칸 정렬)
  const char *dpad_str = "RELEASE   ";
  switch (joy->dpad)
  {
    case 1:
      dpad_str = "UP        ";
      break;
    case 2:
      dpad_str = "UP-RIGHT  ";
      break;
    case 3:
      dpad_str = "RIGHT     ";
      break;
    case 4:
      dpad_str = "DOWN-RIGHT";
      break;
    case 5:
      dpad_str = "DOWN      ";
      break;
    case 6:
      dpad_str = "DOWN-LEFT ";
      break;
    case 7:
      dpad_str = "LEFT      ";
      break;
    case 8:
      dpad_str = "UP-LEFT   ";
      break;
  }


    cliPrintf("=====================================================================\n");
    cliPrintf("                 XBOX BLE CONTROLLER REALTIME STATUS                 \n");
    cliPrintf("=====================================================================\n");

    cliPrintf("[STICK] LX:%5d, LY:%5d \n",
              joy->left_stick_x, joy->left_stick_y);

    cliPrintf("[STICK] RX:%5d, RY:%5d \n",
              joy->right_stick_x, joy->right_stick_y);
              
    cliPrintf("[STICK] LT:%5d, RT:%5d\n",
              joy->left_trigger, joy->right_trigger);

    cliPrintf("[D-PAD] %s   \n", dpad_str);

    cliPrintf("[BTNS ] %4s %4s %4s %4s \n",
              joy->btn_a ? "A" : "_",
              joy->btn_b ? "B" : "_",
              joy->btn_x ? "X" : "_",
              joy->btn_y ? "Y" : "_");

    cliPrintf("[TRIG ] %4s %4s %4s %4s \n",
              joy->btn_lb ? "LB" : "__", joy->btn_rb ? "RB" : "__",
              joy->btn_ls ? "LS" : "__", joy->btn_rs ? "RS" : "__");

    cliPrintf("[BTNS ] %4s %4s %4s\n",
              joy->btn_view ? "VIEW" : "____",
              joy->btn_menu ? "MENU" : "____",
              joy->btn_xbox ? "XBOX" : "____");

    return 10;
}

void cliCmd(cli_args_t *args)
{
  bool ret = false;


  if (args->argc == 1 && args->isStr(0, "info"))
  {
    char addr_str[BT_ADDR_LE_STR_LEN] = "-";

    if (has_bonded_pad) bt_addr_le_to_str(&bonded_pad_addr, addr_str, sizeof(addr_str));
    cliPrintf("is_init      : %d\n", is_init);
    cliPrintf("is_connected : %d\n", is_connected);
    cliPrintf("is_connecting_or_connected : %d\n", is_connecting_or_connected);
    cliPrintf("state        : %s\n", blePadGetStateStr());
    cliPrintf("scan enable  : %d\n", scan_enable);
    cliPrintf("bonded       : %d %s\n", has_bonded_pad, addr_str);
    cliPrintf("name         : %s\n", pad_name);
    cliPrintf("rssi         : %d dBm\n", pad_rssi);
    cliPrintf("battery      : %d %%\n", battery_level);
    cliPrintf("rx count     : %d\n", rx_cnt);
    ret = true;
  }

  if (args->argc == 2 && args->isStr(0, "scan"))
  {
    if (args->isStr(1, "on"))  blePadScanStart();
    if (args->isStr(1, "off")) blePadScanStop();
    cliPrintf("scan %s\n", scan_enable ? "on" : "off");
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "disc"))
  {
    blePadDisconnect();
    cliPrintf("disconnect\n");
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "clear"))
  {
    blePadClearBonds();
    cliPrintf("all bonds cleared. put the pad in pairing mode to connect again\n");
    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "reset"))
  {
    sys_reboot(SYS_REBOOT_COLD);

    ret = true;
  }

  if (args->argc == 1 && args->isStr(0, "show"))
  {
    uint16_t line = 0;
    xbox_hid_report_t hid_report;

    cliShowCursor(false);
    while(cliKeepLoop())
    {
      lock();
      hid_report = xbox_report;
      unLock();

      cliPrintf("is_connected : %d, battery %d %%\n", is_connected, battery_level);
      
      line = blePadReportLog(&hid_report);
      line++;

      delay(100);
      cliMoveUp(line);
    }
    cliMoveDown(line);
    cliShowCursor(true);
    ret = true;
  }

  if (ret != true)
  {
    cliPrintf("blepad info\n");
    cliPrintf("blepad scan on|off\n");
    cliPrintf("blepad disc\n");
    cliPrintf("blepad clear\n");
    cliPrintf("blepad reset\n");
    cliPrintf("blepad show\n");
  }
}
#endif