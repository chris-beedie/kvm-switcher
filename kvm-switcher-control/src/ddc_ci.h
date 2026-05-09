#pragma once
#include <Wire.h>

// --- Protocol ---
#define DDC_ADDR            0x37
#define VCP_INPUT_SRC       0x60
#define VCP_POWER_MODE      0xD6
#define DDC_GET_DELAY_MS    40
#define DDC_RETRY_DELAY_MS  100

// --- Dell U3821DW (Bus A) ---
#define U38_INPUT1  0x0F   // DisplayPort  (Laptop 1)
#define U38_INPUT2  0x1B   // USB-C        (Laptop 2)

// --- Dell U2419H (Bus B) ---
#define U24_INPUT1  0x11   // HDMI 1       (Laptop 1)
#define U24_INPUT2  0x0F   // DisplayPort  (Laptop 2)

// Per-monitor switch result
struct SwitchResult {
  bool a_ok;
  bool b_ok;
};

bool ddcSetVCP(TwoWire &bus, uint8_t vcpCode, uint8_t value);
bool ddcSetVCPWithRetry(TwoWire &bus, uint8_t vcpCode, uint8_t value, uint8_t retries);
bool ddcGetVCP(TwoWire &bus, uint8_t vcpCode, uint8_t *valueHi, uint8_t *valueLo);

SwitchResult ddcSwitchMonitors(TwoWire &busA, TwoWire &busB, bool input1);
int ddcReadCurrentInput(TwoWire &busA);
bool ddcIsMonitorAwake(TwoWire &bus);

// Input name helpers
const char* inputNameU38(bool input1);
const char* inputNameU24(bool input1);

void i2cScan(TwoWire &bus, const char* name);
