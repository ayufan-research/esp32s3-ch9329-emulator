#include <esp32-hal.h>
#include <esp32-hal-tinyusb.h>
#include <tusb.h>
#include <esp_private/usb_phy.h>
#include <soc/efuse_reg.h>
#include <device/usbd_pvt.h>
#include <device/usbd.h>

#include "usb.h"
#include "class/hid/hid_device.h"
#include "hid_abs_mouse.h"

static tusb_desc_device_t const desc_device =
{
  .bLength            = sizeof(tusb_desc_device_t),
  .bDescriptorType    = TUSB_DESC_DEVICE,
  .bcdUSB             = 0x0200,
  .bDeviceClass       = TUSB_CLASS_MISC,
  .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
  .bDeviceProtocol    = MISC_PROTOCOL_IAD,
  .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

  .idVendor           = 0x16d0,
  .idProduct          = 0x10a9,
  .bcdDevice          = 0x0110,

  .iManufacturer      = STRID_Manufacturer,
  .iProduct           = STRID_Product,
  .iSerialNumber      = STRID_SerialNumber,

  .bNumConfigurations = 0x01
};

const uint8_t *tud_descriptor_device_cb(void)
{
  return (const uint8_t *)&desc_device;
}

static const uint8_t desc_hid_report[] = {
  TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_REPORT_ID_KEYBOARD)),
  TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(HID_REPORT_ID_MOUSE_REL)),
  TUD_HID_REPORT_DESC_ABSMOUSE(HID_REPORT_ID(HID_REPORT_ID_MOUSE_ABS)),
};

static uint8_t desc_hid_report_length = sizeof(desc_hid_report);

const uint8_t *tud_hid_descriptor_report_cb(uint8_t instance)
{
  return desc_hid_report;
}

static tusb_desc_configuration_t *desc_configuration;

static uint8_t tud_descriptor_num_interfaces()
{
  if (!desc_configuration)
    return 0;
  return desc_configuration->bNumInterfaces;
}

static bool tud_add_descriptor(const uint8_t *desc, uint32_t size, uint32_t itfs)
{
  if (!desc_configuration)
    desc_configuration = (tusb_desc_configuration_t*)new uint8_t[TUD_CONFIG_DESC_LEN] {
      TUD_CONFIG_DESCRIPTOR(1, 0, STRID_ConfigName, TUD_CONFIG_DESC_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100)
    };

  if (!desc_configuration)
    return false;
  
  uint16_t total_length = tu_le16toh(desc_configuration->wTotalLength);
  desc_configuration = (tusb_desc_configuration_t *)realloc(desc_configuration, total_length + size);
  if (!desc_configuration)
    return false;

  uint8_t *dest = (uint8_t*)desc_configuration + total_length;
  memcpy(dest, desc, size);
  desc_configuration->wTotalLength = tu_htole16(total_length + size);
  desc_configuration->bNumInterfaces += itfs;
  return true;
}

const uint8_t *tud_descriptor_configuration_cb(uint8_t index)
{
  return (const uint8_t*)desc_configuration;
}

char desc_serial[16];

static const char *desc_string[STRID_MAX] = {
  [STRID_Lang] = "\x09\x04",
  [STRID_Manufacturer] = "ESP32 S3",
  [STRID_Product] = "CH9329",
  [STRID_SerialNumber] = desc_serial,
  [STRID_ConfigName] = "TinyUSB Device",
  [STRID_HidName] = "HID Device",
  [STRID_GudName] = "GUD Device",
  [STRID_CDCName] = "CDC Device",
};

const uint16_t *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
  static uint16_t _desc_str[127];
  uint8_t count = 1;

  if (index >= STRID_MAX) {
    return NULL;
  } else if (index == 0) {
    memcpy(&_desc_str[count++], desc_string[0], 2);
  } else {
    const char *str = desc_string[index];
    while (*str && count < sizeof(_desc_str)/sizeof(_desc_str[0])) {
      _desc_str[count++] = *str++;
    }
  }

  // first byte is len, second byte is string type
  _desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2*count);
  return _desc_str;
}

static usb_phy_handle_t phy_hdl;

// Configure USB PHY
static usb_phy_config_t phy_conf = {
  .controller = USB_PHY_CTRL_OTG,
  .target = USB_PHY_TARGET_INT,
  .otg_mode = USB_OTG_MODE_DEVICE,
};

bool usb_init()
{
  uint8_t mac[8];
  if (esp_efuse_mac_get_default(mac) == ESP_OK) {
    sprintf(desc_serial, "%02X%02X%02X%02X%02X%02X",
      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  }

  esp_err_t err = usb_new_phy(&phy_conf, &phy_hdl);
  if (err != ESP_OK) {
    log_e("failed: %s", esp_err_to_name(err));
    return false;
  }

  if (desc_hid_report_length) {
    const uint8_t hid_desc[] = {
      TUD_HID_DESCRIPTOR(
        tud_descriptor_num_interfaces(), STRID_HidName, HID_ITF_PROTOCOL_NONE,
        desc_hid_report_length, ENDPOINT_IN_HID, CFG_TUD_HID_EP_BUFSIZE, 10)
    };
    tud_add_descriptor(hid_desc, sizeof(hid_desc), 1);
  }

  return tud_init(0);
}

void usb_task(uint32_t timeout_ms)
{
  tud_task_ext(timeout_ms, false);
}
