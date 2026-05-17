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
            logPrintf(msg " (err %d)", _err); \
            break; \
        } \
    } while(0)

#define MAX_REPORT_SUBS 8



static bool init(void);
static bool blePadInit(void);
static void blePadThread(void const *arg);


static void    blePadScanFilterMatch(struct bt_scan_device_info  *device_info,
                                     struct bt_scan_filter_match *filter_match,
                                     bool                         connectable);
static void    blePadConnected(struct bt_conn *conn, uint8_t err);
static void    blePadDisconnected(struct bt_conn *conn, uint8_t reason);
static void    blePadAuthPasskeyDisplay(struct bt_conn *conn, unsigned int passkey);
static void    blePadAuthCancel(struct bt_conn *conn);
static void    blePadSecurityChanged(struct bt_conn *conn, bt_security_t level, enum bt_security_err err);
static uint8_t blePadNotifyCb(struct bt_conn                  *conn,
                              struct bt_gatt_subscribe_params *params,
                              const void *data, uint16_t length);
static uint8_t blePadDiscoverCb(struct bt_conn                 *conn,
                                const struct bt_gatt_attr      *attr,
                                struct bt_gatt_discover_params *params);
static void    blePadEnableHid(struct bt_conn *conn, uint16_t handle);
static void    blePadPairingComplete(struct bt_conn *conn, bool bonded);
static void    blePadPairingFailed(struct bt_conn *conn, enum bt_security_err reason);

MODULE_DEF(ble_pad) 
{
  .name = "ble_pad",
  .priority = MODULE_PRI_LOW,
  .init = init
};



// K_THREAD_DEFINE(xbox_thread,
//                 _HW_DEF_RTOS_THREAD_MEM_CLI,
//                 sdMgrThread, NULL, NULL, NULL,
//                 _HW_DEF_RTOS_THREAD_PRI_CLI, 0, 0);
static K_THREAD_STACK_DEFINE(thread_stack, _HW_DEF_RTOS_THREAD_MEM_BLEPAD);
static struct k_thread thread_data;

static struct bt_conn                 *ble_pad_conn; // 현재 연결된 기기 정보를 담는 포인터
static struct bt_gatt_discover_params  discover_params;
static struct bt_gatt_subscribe_params subscribe_params[MAX_REPORT_SUBS];
static uint8_t                         subscribe_count = 0;
static struct k_work                   ble_pad_subscribe_work;
static uint16_t                        handle_2a4c = 0;
static struct k_timer                  xbox_keepalive_timer;
static struct k_work_delayable         ble_pad_discover_delay_work; // 💡 추가


BT_SCAN_CB_INIT(ble_pad_scan_cb,
                blePadScanFilterMatch, // filter_match
                NULL,                  // filter_no_match
                NULL,                  // connecting_error
                NULL);                 // connecting

BT_CONN_CB_DEFINE(ble_pad_conn_callbacks) = {
  .connected        = blePadConnected,
  .disconnected     = blePadDisconnected,
  .security_changed = blePadSecurityChanged,
};

static struct bt_conn_auth_cb ble_pad_auth_cb_display = {
  .passkey_display = blePadAuthPasskeyDisplay,
  .cancel          = blePadAuthCancel,
};

static struct bt_conn_auth_info_cb ble_pad_auth_info_cb = {
  .pairing_complete = blePadPairingComplete,
  .pairing_failed   = blePadPairingFailed,
};


bool init(void)
{
  bool ret;

  k_tid_t tid = k_thread_create(&thread_data, thread_stack,
                                  K_THREAD_STACK_SIZEOF(thread_stack),
                                  (k_thread_entry_t)blePadThread,
                                  NULL, NULL, NULL,
                                  _HW_DEF_RTOS_THREAD_PRI_BLEPAD, 0, K_NO_WAIT);

  ret = tid != NULL ? true:false;                                  
  logPrintf("[%s] blePadInit()\n", ret ? "OK":"E_");

  return ret;
}

static bool bldPadScanInit(void)
{
  bool ret = false;

  struct bt_scan_init_param scan_init_obj = {
    .connect_if_match = false,
  };

  bt_scan_init(&scan_init_obj);
  bt_scan_cb_register(&ble_pad_scan_cb);

  do
  {
    CHECK_ERR(bt_scan_filter_add(BT_SCAN_FILTER_TYPE_UUID, BT_UUID_HIDS),
              "[E_] UUID filter add failed");

    CHECK_ERR(bt_scan_filter_enable(BT_SCAN_UUID_FILTER, false),
              "[E_] UUID filter enable failed");

    CHECK_ERR(bt_scan_filter_add(BT_SCAN_FILTER_TYPE_NAME, "Xbox Wireless Controller"),
              "[E_] Name filter add failed");

    CHECK_ERR(bt_scan_filter_enable(BT_SCAN_NAME_FILTER, false),
              "[E_] Name filter enable failed");

    ret = true;
  } while (0);

  return ret;
}

static bool blePadParseNameCb(struct bt_data *data, void *user_data)
{
  char *name = user_data;

  switch (data->type)
  {
    case BT_DATA_NAME_COMPLETE:
    case BT_DATA_NAME_SHORTENED:
      // 찾은 이름을 user_data(name 변수)에 복사
      int len = MIN(data->data_len, 31); // 31바이트 제한
      memcpy(name, data->data, len);
      name[len] = '\0';
      return false;                      // 이름을 찾았으므로 순회 중단
    default:
      return true;                       // 계속 탐색
  }
}

static void blePadScanFilterMatch(struct bt_scan_device_info  *device_info,
                              struct bt_scan_filter_match *filter_match,
                              bool                         connectable)
{
  char addr[BT_ADDR_LE_STR_LEN];
  char name[32] = "No Name"; // 이름을 담을 버퍼


  bt_addr_le_to_str(device_info->recv_info->addr, addr, sizeof(addr));
  bt_data_parse(device_info->adv_data, blePadParseNameCb, name);


  logPrintf("[  ] Device: %s (%s) rssi %d\n", name, addr, device_info->recv_info->rssi);

  if (strstr(name, "Xbox") != NULL && device_info->recv_info->rssi > -50)
  {
    logPrintf("[..] Attempting to connect: %s (%s)\n", name, addr);

    int err;

    err = bt_scan_stop();
    if (err && err != -EALREADY)
    {
      logPrintf("[E_] Scan stop failed (err %d)\n", err);
      return;
    }

    // 스캔을 중지하고 연결 시도 (Scan 모듈의 자동 연결 기능을 쓸 수도 있음)
    err = bt_conn_le_create(device_info->recv_info->addr,
                                BT_CONN_LE_CREATE_CONN,
                                BT_LE_CONN_PARAM_DEFAULT,
                                &ble_pad_conn);
    if (err)
    {
      logPrintf("[E_] Connection attempt failed (err %d)\n", err);
      ble_pad_conn = NULL;      

      bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);      
    }
    else
    {
      logPrintf("[OK] Connection request sent\n");
    }
  }
}

static void blePadMtuExchangeCb(struct bt_conn *conn, uint8_t err,
                            struct bt_gatt_exchange_params *params)
{
  if (err)
  {
    logPrintf("[  ] MTU exchange failed (err %u)\n", err);
  }
  else
  {
    logPrintf("[  ] MTU exchange successful\n");

    // 💡 MTU가 안전하게 셋업된 후 보안 승격을 요청합니다.
    int sec_err = bt_conn_set_security(conn, BT_SECURITY_L2);
    if (sec_err)
    {
      logPrintf("[E_] Failed to set security (err %d)\n", sec_err);
    }
  }
}

// 연결 관련 이벤트 콜백
static void blePadConnected(struct bt_conn *conn, uint8_t err)
{
  if (err)
  {
    logPrintf("[E_] Connection failed (err %u)\n", err);
    return;
  }

  logPrintf("[OK] Connected to Xbox Controller!\n");
  ble_pad_conn = bt_conn_ref(conn); 


  static struct bt_gatt_exchange_params mtu_params;
  mtu_params.func = blePadMtuExchangeCb; 
  bt_gatt_exchange_mtu(conn, &mtu_params);

  // 연결 간격을 7.5ms ~ 15ms로 좁혀서 통신 속도를 올립니다.
  struct bt_le_conn_param *param = BT_LE_CONN_PARAM(6, 12, 0, 400);
  bt_conn_le_param_update(conn, param);

  // int sec_err = bt_conn_set_security(conn, BT_SECURITY_L2);
  // if (sec_err)
  // {
  //   logPrintf("[E_] Failed to set security (err %d)\n", sec_err);
  // }
  // else
  // {
  //   logPrintf("[..] Security upgrade requested\n");
  // }
}

static void blePadDisconnected(struct bt_conn *conn, uint8_t reason)
{
  logPrintf("[--] Disconnected (reason %u)\n", reason);
  if (ble_pad_conn)
  {
    bt_conn_unref(ble_pad_conn); // 참조 해제
    ble_pad_conn = NULL;
  }
}

static void blePadDiscoverDelayWorker(struct k_work *work)
{
  if (!ble_pad_conn) return;

  logPrintf("[WORK] Security stabilized. Starting safe GATT Discovery...\n");

  subscribe_count = 0;
  memset(&discover_params, 0, sizeof(discover_params));

  discover_params.uuid         = NULL;
  discover_params.func         = blePadDiscoverCb;
  discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
  discover_params.end_handle   = BT_ATT_LAST_ATTRIBUTE_HANDLE;
  discover_params.type         = BT_GATT_DISCOVER_PRIMARY;

  int ret = bt_gatt_discover(ble_pad_conn, &discover_params);
  if (ret)
  {
    logPrintf("[E_] Safe GATT Discovery failed (err %d)\n", ret);
  }
}

// 보안 상태가 변경되었을 때 호출되는 콜백
static void blePadSecurityChanged(struct bt_conn *conn, bt_security_t level, enum bt_security_err err)
{
  if (err)
  {
    logPrintf("[E_] Security failed: level %u, err %d\n", level, err);
  }
  else
  {
    logPrintf("[OK] Security changed: level %u\n", level);

    // Xbox 컨트롤러는 보통 Level 2(Encryption) 또는 Level 3(MITM) 이상이어야 합니다.
    if (level >= BT_SECURITY_L2)
    {
      logPrintf("[OK] Starting GATT Discovery...\n");
      if (IS_ENABLED(CONFIG_SETTINGS))
      {
        settings_save();
        logPrintf("[BT] Security settings saved to Flash.\n");
      }

      subscribe_count = 0;
      k_work_reschedule(&ble_pad_discover_delay_work, K_MSEC(300));
    }
  }
}

static void blePadAuthPasskeyDisplay(struct bt_conn *conn, unsigned int passkey)
{
  logPrintf("[..] Passkey for %p: %06u\n", conn, passkey);
}

static void blePadAuthCancel(struct bt_conn *conn)
{
  logPrintf("[E_] Pairing cancelled\n");
}

// 버튼 데이터를 받았을 때 호출될 콜백 (가장 중요한 함수)
static uint8_t blePadNotifyCb(struct bt_conn                  *conn,
                              struct bt_gatt_subscribe_params *params,
                              const void *data, uint16_t length)
{
  if (!data)
  {
    // params->value_handle가 0이 되는 것을 방지하기 위해 로그만 찍고 리턴 확인
    logPrintf("[..] Unsubscribed (Handle: %u)\n", params->value_handle);
    return BT_GATT_ITER_STOP;
  }

  // 여기서 Xbox 컨트롤러의 버튼 데이터가 헥사값으로 찍힙니다!
  logPrintf("[DATA] %u Received length: %u bytes\n", params->value_handle, length);
  // 간단하게 첫 4바이트만 출력해보기
  uint8_t *raw = (uint8_t *)data;
  logPrintf("Data: %02X %02X %02X %02X\n", raw[0], raw[1], raw[2], raw[3]);

  ledToggle(1);

  return BT_GATT_ITER_CONTINUE;
}

static uint8_t blePadDiscoverCb(struct bt_conn                 *conn,
                                const struct bt_gatt_attr      *attr,
                                struct bt_gatt_discover_params *params)
{
  // 1. HID 서비스 내의 모든 특성/속성 탐색이 완료되었을 때
  if (!attr)
  {
    logPrintf("[OK] GATT Discovery completed. Found %u potential reports.\n", subscribe_count);

    if (subscribe_count > 0)
    {
      // 탐색이 완전히 끝난 안전한 시점에 워커를 실행하여 구독 진행
      k_work_submit(&ble_pad_subscribe_work);
    }
    return BT_GATT_ITER_STOP;
  }

  // 2. Primary Service를 발견했을 때 -> 하위 특성 전체 탐색 시작
  if (params->type == BT_GATT_DISCOVER_PRIMARY)
  {
    logPrintf("[OK] Found Primary Service. Starting flat characteristic discovery...\n");
    params->uuid         = NULL; // 모든 특성을 다 확인하기 위해 NULL 설정
    params->start_handle = attr->handle + 1;
    params->type         = BT_GATT_DISCOVER_CHARACTERISTIC;

    // 이 호출은 루프의 시작점 역할을 하므로 안전합니다.
    return BT_GATT_ITER_CONTINUE; 
  }

  // 3. Characteristic들을 발견하면서 필터링 수행
  if (params->type == BT_GATT_DISCOVER_CHARACTERISTIC)
  {
    struct bt_gatt_chrc *chrc   = (struct bt_gatt_chrc *)attr->user_data;
    uint16_t             uuid16 = (chrc->uuid->type == BT_UUID_TYPE_16) ? BT_UUID_16(chrc->uuid)->val : 0;

    logPrintf("[..] Chrc Handle: %u, Value Handle: %u, UUID: %04X\n", attr->handle, chrc->value_handle, uuid16);

    // HID Control Point 핸들 저장
    if (uuid16 == 0x2A4C)
    {
      handle_2a4c = chrc->value_handle;
      logPrintf("[OK] Cached HID Control Point Handle: %u\n", handle_2a4c);
    }

    // Report 특성(2A4D)이면서 Notify 권한이 있는 경우 저장
    // if (uuid16 == 0x2A4D || chrc->uuid->type == BT_UUID_TYPE_128)
    if (chrc->uuid->type == BT_UUID_TYPE_128)
    {
      if (subscribe_count < MAX_REPORT_SUBS)
      {
        memset(&subscribe_params[subscribe_count], 0, sizeof(struct bt_gatt_subscribe_params));
        subscribe_params[subscribe_count].notify       = blePadNotifyCb;
        subscribe_params[subscribe_count].value        = BT_GATT_CHRC_NOTIFY;
        subscribe_params[subscribe_count].value_handle = chrc->value_handle;
        
        // 💡 [핵심] Xbox 컨트롤러는 구조상 구조적 편차가 적어 value_handle + 1이 CCCD인 경우가 지배적입니다.
        // 스택 충돌을 방지하기 위해 정적 오프셋 방식을 취하되, 탐색 완료 후 워커에서 안전하게 구독합니다.
        subscribe_params[subscribe_count].ccc_handle   = chrc->value_handle + 1;

        logPrintf("[OK] Registered Report [%d] -> Value: %u, Est_CCCD: %u\n", 
                  subscribe_count, chrc->value_handle, subscribe_params[subscribe_count].ccc_handle);
        subscribe_count++;
      }
    }
  }

  return BT_GATT_ITER_CONTINUE; // 끊김 없이 다음 특성을 계속 탐색
}

static uint8_t blePadDummyReadCb(struct bt_conn *conn, uint8_t err,
                                 struct bt_gatt_read_params *params,
                                 const void *data, uint16_t length)
{
  // 데이터 검증 없이 그냥 통과 처리하여 세션 유지용 신호로만 사용
  return BT_GATT_ITER_STOP;
}

static void blePadSubscribeWorker(struct k_work *work)
{
  if (!ble_pad_conn) return;

  logPrintf("[WORK] Starting subscriptions for %u reports...\n", subscribe_count);
 
  for (int i = 0; i < subscribe_count; i++)
  {
    // 이미 DiscoverCb에서 세팅된 subscribe_params[i]를 사용하여 구독 시도
    int err = bt_gatt_subscribe(ble_pad_conn, &subscribe_params[i]);
    if (err)
    {
      logPrintf("[E_] Sub failed (Handle: %u, err: %d)\n",
                subscribe_params[i].value_handle, err);
    }
    else
    {
      logPrintf("[OK] Subscribed (Handle: %u)\n", subscribe_params[i].value_handle);
    }    
  }

  // 모든 리포트 구독 후 Xbox 컨트롤러를 깨우기 위해 2A4C에 신호 전송
  // if (handle_2a4c != 0)
  // {
  //   blePadEnableHid(ble_pad_conn, handle_2a4c);    
  // }
  // else
  // {
  //   logPrintf("[W_] HID Control Point (2A4C) not found. Skip enabling.\n");
  // }

  if (handle_2a4c != 0)
  {
    uint8_t cmd_suspend = 0x00;
    uint8_t cmd_exit    = 0x01;

    // A. 먼저 Suspend(0x00)를 명시적으로 전달
    bt_gatt_write_without_response(ble_pad_conn, handle_2a4c, &cmd_suspend, 1, false);
    logPrintf("[..] Sent Suspend(00) to HID Control Point\n");

    // B. 커맨드가 유입될 시간을 주기 위해 커널 딜레이 (OS 스케줄러 양보)
    k_msleep(30);

    // C. 그 다음 Exit Suspend(0x01)를 전송하여 상태 머신을 강제로 활성화
    bt_gatt_write_without_response(ble_pad_conn, handle_2a4c, &cmd_exit, 1, false);
    logPrintf("[OK] Sent Exit Suspend(01) to HID Control Point\n");

    k_msleep(100);
  }
  else
  {
    logPrintf("[W_] HID Control Point (2A4C) not found. Skip enabling.\n");
  }

  // 💡 [추가] Xbox 패드 기기 인증용 킵얼라이브 더미 리드 트리거
  static struct bt_gatt_read_params read_params;
  read_params.handle_count  = 1;
  read_params.single.handle = 17; // UUID 180A의 Value Handle 위치
  read_params.single.offset = 0;
  read_params.func          = blePadDummyReadCb;

  int read_err = bt_gatt_read(ble_pad_conn, &read_params);
  if (read_err)
  {
    logPrintf("[E_] Dummy Read failed (err %d)\n", read_err);
  }

  k_timer_start(&xbox_keepalive_timer, K_MSEC(500), K_MSEC(500)); 
}

static void blePadEnableHid(struct bt_conn *conn, uint16_t handle)
{
  if (!conn || handle == 0) return;

  // 0x00: Suspend 해제 (Exit Suspend)
  // Xbox 컨트롤러에 따라 0x00 혹은 0x01이 필요할 수 있습니다.
  uint8_t enable_data = 0x00;

  int err = bt_gatt_write_without_response(conn, handle, &enable_data, sizeof(enable_data), false);
  if (err)
  {
    logPrintf("[E_] Failed to send HID Enable (err %d)\n", err);
  }
  else
  {
    logPrintf("[OK] Sent Enable(%02X) to HID Control Point (Handle %u)\n", enable_data, handle);
  }

  enable_data = 0x01;

  delay(10);
  err = bt_gatt_write_without_response(conn, handle, &enable_data, sizeof(enable_data), false);
  if (err)
  {
    logPrintf("[E_] Failed to send HID Enable (err %d)\n", err);
  }
  else
  {
    logPrintf("[OK] Sent Enable(%02X) to HID Control Point (Handle %u)\n", enable_data, handle);
  }

}

static void blePadPairingComplete(struct bt_conn *conn, bool bonded)
{
  logPrintf("[BT] Pairing %s\n", bonded ? "Success (Bonded)" : "Failed");

  if (bonded)
  {
    // 💡 페어링이 완벽히 검증된 "가장 안전한 이 시점"에 GATT 탐색을 시작합니다!
    subscribe_count = 0;
    memset(&discover_params, 0, sizeof(discover_params));

    discover_params.uuid         = NULL; // 전수 조사
    discover_params.func         = blePadDiscoverCb;
    discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    discover_params.end_handle   = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    discover_params.type         = BT_GATT_DISCOVER_PRIMARY;

    int ret = bt_gatt_discover(conn, &discover_params);
    if (ret)
    {
      logPrintf("[E_] Discovery post-bonding failed (err %d)\n", ret);
    }
    else
    {
      logPrintf("[OK] Safe GATT Discovery started after successful bonding!\n");
    }
  }
}

static void blePadPairingFailed(struct bt_conn *conn, enum bt_security_err reason)
{
  logPrintf("[BT] Security Error: %d (Auth Failed)\n", reason);
}

void xbox_keepalive_expiry_fn(struct k_timer *timer_id)
{
  if (ble_pad_conn && handle_2a4c != 0)
  {
    uint8_t keepalive_cmd = 0x01;
    bt_gatt_write_without_response(ble_pad_conn, handle_2a4c, &keepalive_cmd, 1, false);
  }
}

bool blePadInit(void)
{
  bool ret = false;  
  int err;

  logPrintf("[  ] blePadInit()\n");

  k_timer_init(&xbox_keepalive_timer, xbox_keepalive_expiry_fn, NULL);

  do 
  {
    err = bt_enable(NULL);
    if (err)
    {
      logPrintf("[E_] Bluetooth init failed (err %d)\n", err);
      break;
    }

    // bt_unpair(BT_ID_DEFAULT, BT_ADDR_LE_ANY);
    // logPrintf("All bonding information cleared!\n");

    if (IS_ENABLED(CONFIG_SETTINGS))
    {
      settings_load();
    }

    bt_conn_auth_cb_register(&ble_pad_auth_cb_display);
    bt_conn_auth_info_cb_register(&ble_pad_auth_info_cb);

    delay(100);

    ret = bldPadScanInit();
    if (ret)
    {
      err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
      if (err)
      {
        logPrintf("[E_] Scanning failed to start (err %d)\n", err);
        ret = false;
      }
    }
  } while(0);

  k_work_init(&ble_pad_subscribe_work, blePadSubscribeWorker);
  k_work_init_delayable(&ble_pad_discover_delay_work, blePadDiscoverDelayWorker);

  logPrintf("[%s] blePadInit()\n", ret ? "OK":"E_");
  return ret;
}

void blePadThread(void const *arg)
{
  bool init_ret = true;


  moduleIsReady();

  logPrintf("[%s] Thread Started : BLE_PAD\n", init_ret ? "OK":"E_" );

  blePadInit();

  while(1)
  {
    delay(10);
  }
}
