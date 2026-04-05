
#include "usb.h"
#include "cli.h"


#include <zephyr/device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/bos.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(usbd);


#define USB_VID           0x0483
#define USB_PID           0x5207
#define USBD_MANUFACTURER "BARAM"
#define USBD_PRODUCT      "NU40DK-KIT"


#if CLI_USE(HW_USB)
static void cliCmd(cli_args_t *args);
#endif


/* By default, do not register the USB DFU class DFU mode instance. */
static const char *const blocklist[] = {
  "dfu_dfu",
  NULL,
};

bool             is_init  = false;
static UsbMode_t usb_mode = USB_CDC_MODE;
static UsbType_t usb_type = USB_CON_CDC;
bool             is_connected = false;
bool             is_open      = false;


USBD_DEVICE_DEFINE(usbd,
		   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
		   USB_VID, USB_PID);


USBD_DESC_LANG_DEFINE(usbd_lang);
USBD_DESC_MANUFACTURER_DEFINE(usbd_mfr, USBD_MANUFACTURER);
USBD_DESC_PRODUCT_DEFINE(usbd_product, USBD_PRODUCT);
IF_ENABLED(CONFIG_HWINFO, (USBD_DESC_SERIAL_NUMBER_DEFINE(usbd_sn)));

USBD_DESC_CONFIG_DEFINE(fs_cfg_desc, "FS Configuration");
USBD_DESC_CONFIG_DEFINE(hs_cfg_desc, "HS Configuration");

static const uint8_t attributes = USB_SCD_SELF_POWERED |
                                  USB_SCD_REMOTE_WAKEUP;

/* Full speed configuration */
USBD_CONFIGURATION_DEFINE(usbd_fs_config,
			  attributes,
			  250, &fs_cfg_desc);

/* High speed configuration */
USBD_CONFIGURATION_DEFINE(usbd_hs_config,
			  attributes,
			  250, &hs_cfg_desc);


static void usbd_fix_code_triple(struct usbd_context *uds_ctx,
				   const enum usbd_speed speed)
{
  /* Always use class code information from Interface Descriptors */
  if (IS_ENABLED(CONFIG_USBD_CDC_ACM_CLASS) ||
      IS_ENABLED(CONFIG_USBD_CDC_ECM_CLASS) ||
      IS_ENABLED(CONFIG_USBD_CDC_NCM_CLASS) ||
      IS_ENABLED(CONFIG_USBD_MIDI2_CLASS) ||
      IS_ENABLED(CONFIG_USBD_AUDIO2_CLASS))
  {
    /*
     * Class with multiple interfaces have an Interface
     * Association Descriptor available, use an appropriate triple
     * to indicate it.
     */
    usbd_device_set_code_triple(uds_ctx, speed,
                                USB_BCC_MISCELLANEOUS, 0x02, 0x01);
  }
  else
  {
    usbd_device_set_code_triple(uds_ctx, speed, 0, 0, 0);
  }
}

struct usbd_context *usbd_setup_device(usbd_msg_cb_t msg_cb)
{
  int err;

  err = usbd_add_descriptor(&usbd, &usbd_lang);
  if (err)
  {
    LOG_ERR("Failed to initialize language descriptor (%d)", err);
    return NULL;
  }

  err = usbd_add_descriptor(&usbd, &usbd_mfr);
  if (err)
  {
    LOG_ERR("Failed to initialize manufacturer descriptor (%d)", err);
    return NULL;
  }

  err = usbd_add_descriptor(&usbd, &usbd_product);
  if (err)
  {
    LOG_ERR("Failed to initialize product descriptor (%d)", err);
    return NULL;
  }

  IF_ENABLED(CONFIG_HWINFO, (
                            err = usbd_add_descriptor(&usbd, &usbd_sn);))
  if (err)
  {
    LOG_ERR("Failed to initialize SN descriptor (%d)", err);
    return NULL;
  }

  if (USBD_SUPPORTS_HIGH_SPEED &&
      usbd_caps_speed(&usbd) == USBD_SPEED_HS)
  {
    err = usbd_add_configuration(&usbd, USBD_SPEED_HS,
                                 &usbd_hs_config);
    if (err)
    {
      LOG_ERR("Failed to add High-Speed configuration");
      return NULL;
    }

    err = usbd_register_all_classes(&usbd, USBD_SPEED_HS, 1,
                                    blocklist);
    if (err)
    {
      LOG_ERR("Failed to add register classes");
      return NULL;
    }

    usbd_fix_code_triple(&usbd, USBD_SPEED_HS);
  }

  err = usbd_add_configuration(&usbd, USBD_SPEED_FS,
                               &usbd_fs_config);
  if (err)
  {
    LOG_ERR("Failed to add Full-Speed configuration");
    return NULL;
  }  

  err = usbd_register_all_classes(&usbd, USBD_SPEED_FS, 1, blocklist);
  if (err)
  {
    LOG_ERR("Failed to add register classes");
    return NULL;
  }

  usbd_fix_code_triple(&usbd, USBD_SPEED_FS);
  usbd_self_powered(&usbd, attributes & USB_SCD_SELF_POWERED);

  if (msg_cb != NULL)
  {    
    err = usbd_msg_register_cb(&usbd, msg_cb);
    if (err)
    {
      LOG_ERR("Failed to register message callback");
      return NULL;
    }  
  }

  return &usbd;
}

struct usbd_context *usbd_init_device(usbd_msg_cb_t msg_cb)
{
  int err;

  if (usbd_setup_device(msg_cb) == NULL)
  {
    return NULL;
  }

  err = usbd_init(&usbd);
  if (err)
  {
    LOG_ERR("Failed to initialize device support");
    return NULL;
  }

  return &usbd;
}

static void msg_cb(struct usbd_context *const   usbd_ctx,
                   const struct usbd_msg *const msg)
{
  // LOG_INF("USBD message: %s", usbd_msg_type_string(msg->type));

  if (msg->type == USBD_MSG_CONFIGURATION)
  {
    LOG_INF("\tConfiguration value %d", msg->status);

    is_connected = true;
  }

  if (usbd_can_detect_vbus(usbd_ctx))
  {
    if (msg->type == USBD_MSG_VBUS_READY)
    {
      if (usbd_enable(usbd_ctx))
      {
        LOG_ERR("Failed to enable device support");
      }
    }

    if (msg->type == USBD_MSG_VBUS_REMOVED)
    {
      if (usbd_disable(usbd_ctx))
      {
        LOG_ERR("Failed to disable device support");
      }

      is_connected = false;
    }
  }

  if (msg->type == USBD_MSG_CDC_ACM_LINE_CODING)
  {
    uint32_t baudrate;
    int      ret;

    ret = uart_line_ctrl_get(msg->dev, UART_LINE_CTRL_BAUD_RATE, &baudrate);
    if (ret == 0)
    {
      // LOG_INF("Baudrate %u", baudrate);
    }
		uint32_t dtr = 0U;

		uart_line_ctrl_get(msg->dev, UART_LINE_CTRL_DTR, &dtr);
		if (dtr)
    {
			is_open = true;
		}    
    else
    {
      is_open = false;
    }
  }
}
UsbMode_t usbGetMode(void)
{
  return usb_mode;
}

UsbType_t usbGetType(void)
{
  return (UsbType_t)usb_type;
}

bool usbIsOpen(void)
{
  return is_connected & is_open;
}

bool usbIsConnect(void)
{
  return is_connected;
}

bool usbInit(void)
{
  struct usbd_context *p_usbd;


  p_usbd = usbd_init_device(msg_cb);
  if (p_usbd == NULL)
  {
    LOG_ERR("usbd_init_device()");
    return false;
  }

  if (!usbd_can_detect_vbus(p_usbd))
  {
    int ret;

    ret = usbd_enable(p_usbd);
    if (ret)
    {
      LOG_ERR("usbd_enable()");
      return false;
    }
  }

  is_init = true;
  
#if CLI_USE(HW_USB)
  cliAdd("usb", cliCmd);
#endif  
  return true;
}

#if CLI_USE(HW_USB)
void cliCmd(cli_args_t *args)
{
  bool ret = false;

  if (args->argc == 1 && args->isStr(0, "info") == true)
  {
    while(cliKeepLoop())
    {
      cliPrintf("USB Mode    : %d\n", usbGetMode());
      cliPrintf("USB Type    : %d\n", usbGetType());
      cliPrintf("USB Connect : %d\n", usbIsConnect());
      cliPrintf("USB Open    : %d\n", usbIsOpen());
      cliPrintf("\x1B[%dA", 4);
      delay(100);
    }
    cliPrintf("\x1B[%dB", 4);

    ret = true;
  }

  if (ret == false)
  {
    cliPrintf("usb info\n");
  }
}
#endif
