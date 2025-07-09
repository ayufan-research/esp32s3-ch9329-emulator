#pragma once

#include <stdint.h>

enum HID_ReportID
{
  HID_REPORT_ID_KEYBOARD = 1,
  HID_REPORT_ID_MOUSE_REL = 2,
  HID_REPORT_ID_MOUSE_ABS = 3,
};

enum USB_StringID
{
  STRID_Lang,
  STRID_Manufacturer,
  STRID_Product,
  STRID_SerialNumber,
  STRID_ConfigName,
  STRID_HidName,
  STRID_GudName,
  STRID_CDCName,
  STRID_MAX,
};

enum USB_EndpointIn
{
  ENDPOINT_IN_FIRST = 0x80,
  ENDPOINT_IN_HID = 0x81,
  ENDPOINT_IN_CDC = 0x84,
  ENDPOINT_IN_CDC_NOTIF = 0x85,
  ENDPOINT_IN_MAX,
};

enum USB_EndpointOut
{
  ENDPOINT_OUT_FIRST = 0x0,
  ENDPOINT_OUT_GUD = 0x1,
  ENDPOINT_OUT_CDC = 0x3,
  ENDPOINT_OUT_MAX,
};


bool usb_init();
void usb_task(uint32_t timeout_ms);
