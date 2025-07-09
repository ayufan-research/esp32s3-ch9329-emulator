#include <Arduino.h>
// #include "USB.h"
// #include "USBHIDKeyboard.h"
// #include "USBHIDMouse.h"

// USBHIDKeyboard Keyboard;
// USBHIDMouse Mouse;

#include "usb.h"
#include "class/hid/hid_device.h"
#include "hid_abs_mouse.h"

enum CmdEvent {
  GET_INFO = 0x01,
  SEND_KB_GENERAL_DATA = 0x02,
  SEND_KB_MEDIA_DATA = 0x03,
  SEND_MS_ABS_DATA = 0x04,
  SEND_MS_REL_DATA = 0x05,
  SEND_MY_HID_DATA = 0x06,
  READ_MY_HID_DATA = 0x87,
  GET_PARA_CFG = 0x08,
  SET_PARA_CFG = 0x09,
  GET_USB_STRING = 0x0a,
  SET_USB_STRING = 0x0b,
  SET_DEFAULT_CFG = 0x0c,
  RESET = 0x0f
};

enum MouseMode {
  MOUSE_RELATIVE = 1,
  MOUSE_ABSOLUTE = 2
};

static constexpr uint8_t HEAD1 = 0x57;
static constexpr uint8_t HEAD2 = 0xAB;

uint8_t buf[265];
uint8_t bufIndex = 0;

enum PacketState {
  MORE,
  DROP,
  ACCEPT
};

void handleGetInfoCmd(const uint8_t*, uint8_t) {
  uint8_t version[] = { 0x01, 0x10 };
  Serial.write(version, sizeof(version));
}

void handleKeyboardGeneralData(const uint8_t *d, uint8_t len) {
  if (len < 8) return;

  tud_hid_keyboard_report(HID_REPORT_ID_KEYBOARD, d[0], (uint8_t*)d+2);
}

#define clamp(d, min, max) \
  ((d) < (min) ? (min) : (d) > (max) ? (max) : (d))

#define range_scale(d, a, b) \
  clamp((int)(d) * ((b)+1) / ((a)+1), 0, (b))

#define ABSMOUSEIN_MAX 4095

void handleMouseAbsData(const uint8_t *d, uint8_t len) {
  if (len < 7) return;
  if (d[0] != MOUSE_ABSOLUTE) return;

  uint8_t buttons = d[1];
  int16_t x = range_scale(d[2] | (d[3] << 8), ABSMOUSEIN_MAX, ABSMOUSE_MAX);
  int16_t y = range_scale(d[4] | (d[5] << 8), ABSMOUSEIN_MAX, ABSMOUSE_MAX);
  int8_t wheel = (int8_t)d[6];
  tud_hid_abs_mouse_report(HID_REPORT_ID_MOUSE_ABS,
      buttons, x, y, wheel, 0);
}

void handleMouseRelData(const uint8_t *d, uint8_t len) {
  if (len < 5) return;
  if (d[0] != MOUSE_RELATIVE) return;

  uint8_t buttons = d[1];
  int8_t dx = (int8_t)d[2];
  int8_t dy = (int8_t)d[3];
  int8_t wheel = d[4];
  tud_hid_mouse_report(HID_REPORT_ID_MOUSE_REL, buttons, dx, dy, wheel, 0);
}

void handleResetCmd(const uint8_t*, uint8_t) {
  delay(100); // Allow time for cleanup
  ESP.restart(); // Restart the ESP32
}

int processPacket(const uint8_t *p, uint8_t len) {
  if (len < 1) return MORE; // need more bytes
  if (p[0] != HEAD1) return DROP; // drop, invalid head
  if (len < 2) return MORE; // need more bytes
  if (p[1] != HEAD2) return DROP; // drop, invalid head
  if (len < 6) return MORE; // need more bytes

  uint8_t dataLen = p[4];
  uint8_t expectedLen = 5 + dataLen + 1; // header(2) + addr(1) + cmd(1) + len(1) + data + sum(1)
  if (len < expectedLen) return MORE; // incomplete packet

  // checksum
  uint16_t s = 0;
  for (uint8_t i = 0; i < expectedLen - 1; i++) s += p[i];
  s &= 0xFF;
  if (((uint8_t)s) != p[expectedLen - 1]) return true; // checksum fail, discard and reset buffer

  uint8_t cmd = p[3];
  const uint8_t *data = p + 5;

  switch (cmd) {
    case GET_INFO:
      handleGetInfoCmd(data, dataLen);
      break;
    case SEND_KB_GENERAL_DATA:
      handleKeyboardGeneralData(data, dataLen);
      break;
    case SEND_MS_REL_DATA:
      handleMouseRelData(data, dataLen);
      break;
    case SEND_MS_ABS_DATA:
      handleMouseAbsData(data, dataLen);
      break;
    case RESET:
      handleResetCmd(data, dataLen);
      break;

    // Extend with other commands as needed
    default:
      return DROP;
  }

  return ACCEPT; // packet processed
}

static void usb_device_task(void *param)
{
    while (1) {
        usb_task(100);
        delay(1);
    }
}

void setup() {
  Serial.begin(57600);
  while (!Serial);
  
  if (usb_init()) {
    Serial.println("USB started.");
    xTaskCreateUniversal(usb_device_task, "usbd", 8192, NULL, configMAX_PRIORITIES - 1, NULL, 1);
  } else {
    Serial.println("USB failed.");
  }
  bufIndex = 0;
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    buf[bufIndex++] = c;

    switch (processPacket(buf, bufIndex)) {
      case MORE:
        break;

      case DROP:
        bufIndex = 0;
        break;

      case ACCEPT:
        bufIndex = 0;
        break;
    }
  }
}
