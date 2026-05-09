#include "ddc_ci.h"
#include "hid_link.h"
#include <Arduino.h>

bool ddcSetVCP(TwoWire &bus, uint8_t vcpCode, uint8_t value) {
  uint8_t payload[] = { 0x51, 0x84, 0x03, vcpCode, 0x00, value };
  uint8_t checksum = 0x6E;
  for (uint8_t i = 0; i < sizeof(payload); i++) checksum ^= payload[i];

  bus.beginTransmission(DDC_ADDR);
  bus.write(payload, sizeof(payload));
  bus.write(checksum);
  return (bus.endTransmission() == 0);
}

bool ddcSetVCPWithRetry(TwoWire &bus, uint8_t vcpCode, uint8_t value, uint8_t retries) {
  for (uint8_t i = 0; i < retries; i++) {
    if (ddcSetVCP(bus, vcpCode, value)) return true;
    if (i < retries - 1) {
      Log.printf("retry %d... ", i + 1);
      delay(DDC_RETRY_DELAY_MS);
    }
  }
  return false;
}

bool ddcGetVCP(TwoWire &bus, uint8_t vcpCode, uint8_t *valueHi, uint8_t *valueLo) {
  uint8_t payload[] = { 0x51, 0x82, 0x01, vcpCode };
  uint8_t checksum = 0x6E;
  for (uint8_t i = 0; i < sizeof(payload); i++) checksum ^= payload[i];

  bus.beginTransmission(DDC_ADDR);
  bus.write(payload, sizeof(payload));
  bus.write(checksum);
  if (bus.endTransmission() != 0) return false;

  delay(DDC_GET_DELAY_MS);

  uint8_t resp[11];
  if (bus.requestFrom((uint8_t)DDC_ADDR, (uint8_t)11) != 11) return false;
  for (uint8_t i = 0; i < 11; i++) resp[i] = bus.read();

  uint8_t chk = 0x50;
  for (uint8_t i = 0; i < 10; i++) chk ^= resp[i];
  if (chk != resp[10]) return false;
  if (resp[3] != 0x00) return false;
  if (resp[4] != vcpCode) return false;

  *valueHi = resp[8];
  *valueLo = resp[9];
  return true;
}

SwitchResult ddcSwitchMonitors(TwoWire &busA, TwoWire &busB, bool input1) {
  SwitchResult r;

  uint8_t u38 = input1 ? U38_INPUT1 : U38_INPUT2;
  uint8_t u24 = input1 ? U24_INPUT1 : U24_INPUT2;

  Log.printf("  U3821DW -> 0x%02X ... ", u38);
  r.a_ok = ddcSetVCPWithRetry(busA, VCP_INPUT_SRC, u38, 2);
  Log.println(r.a_ok ? "OK" : "FAIL");

  delay(DDC_RETRY_DELAY_MS);

  Log.printf("  U2419H  -> 0x%02X ... ", u24);
  r.b_ok = ddcSetVCPWithRetry(busB, VCP_INPUT_SRC, u24, 2);
  Log.println(r.b_ok ? "OK" : "FAIL");

  return r;
}

int ddcReadCurrentInput(TwoWire &busA) {
  Log.println("Reading current input from U3821DW...");
  uint8_t valHi = 0, valLo = 0;

  if (!ddcGetVCP(busA, VCP_INPUT_SRC, &valHi, &valLo)) {
    Log.println("  No response");
    return 0;
  }

  Log.printf("  VCP 0x60 = 0x%02X\r\n", valLo);
  if (valLo == U38_INPUT1) { Log.println("  Input 1 (DP)"); return 1; }
  if (valLo == U38_INPUT2) { Log.println("  Input 2 (USB-C)"); return 2; }
  Log.printf("  Unknown: 0x%02X\r\n", valLo);
  return 0;
}

bool ddcIsMonitorAwake(TwoWire &bus) {
  uint8_t valHi = 0, valLo = 0;
  if (!ddcGetVCP(bus, VCP_POWER_MODE, &valHi, &valLo)) return false;
  return (valLo == 0x01);
}

const char* inputNameU38(bool input1) {
  return input1 ? "DisplayPort" : "USB-C";
}

const char* inputNameU24(bool input1) {
  return input1 ? "HDMI 1" : "DisplayPort";
}

void i2cScan(TwoWire &bus, const char* name) {
  Log.printf("Scanning %s...\r\n", name);
  bool found = false;
  for (uint8_t addr = 1; addr < 127; addr++) {
    bus.beginTransmission(addr);
    if (bus.endTransmission() == 0) {
      Log.printf("  Found 0x%02X\r\n", addr);
      found = true;
    }
  }
  if (!found) Log.println("  No devices found");
}
