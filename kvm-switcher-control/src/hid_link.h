#pragma once
#include <Arduino.h>

// ── Wire protocol shared with the RP2350 (kvm-switcher-hid) ─────────────────
// Frame layout on the UART:
//   [0xAB][type][len][payload 0..len-1][xor checksum of type|len|payload]
// Baud: 1 Mbps, 8N1.
//
// Message types
namespace LinkMsg {
    constexpr uint8_t KEYBOARD       = 0x01;  // 8-byte boot keyboard report
    constexpr uint8_t CONSUMER       = 0x02;  // 2-byte consumer-control report (LE)
    constexpr uint8_t LOG            = 0x10;  // ESP32 -> RP2350 debug log line
    constexpr uint8_t USB_STATUS     = 0x20;  // RP2350 -> ESP32 USB-to-PC state
    constexpr uint8_t HEARTBEAT      = 0x21;  // RP2350 -> ESP32 1Hz keepalive
}

// USB-to-PC status payload (1 byte, MSB→LSB):
//   bit0: mounted   (host enumerated us)
//   bit1: suspended (USB suspend)
struct LinkUsbStatus {
    bool mounted;
    bool suspended;
};

// Initialise the UART link to the RP2350.
// Call once from setup(). After this, hidLinkLog() may be called instead of
// Serial.print* to send debug lines through the link.
void hidLinkBegin(uint8_t tx_pin, uint8_t rx_pin, uint32_t baud = 1000000);

// Pump receive buffer + heartbeat tracking. Call every loop().
void hidLinkLoop();

// Send an 8-byte boot keyboard report (modifier, reserved, k1..k6).
// Returns true if the frame was queued for transmission.
bool hidLinkSendKeyboard(const uint8_t report[8]);

// Send a 2-byte consumer-control report (little-endian usage code).
bool hidLinkSendConsumer(uint16_t usage_code);

// Send a UTF-8 log line. Truncated to 200 bytes. No trailing newline needed.
void hidLinkLog(const char* msg);
void hidLinkLogf(const char* fmt, ...);

// Print-compatible log sink that fans out to UART0 (Serial) and the UART link.
// Use as a drop-in for Serial: e.g. Log.printf("foo %d\n", x).
// Lines are buffered until '\n' is written, then sent as one LOG frame.
class LinkPrint : public Print {
public:
    size_t write(uint8_t c)  override;
    size_t write(const uint8_t* buf, size_t n) override;
    void   flushLine();
private:
    char   buf_[220];
    size_t len_ = 0;
};
extern LinkPrint Log;

// Latest USB-to-PC status reported by the RP2350.
LinkUsbStatus hidLinkUsbStatus();

// True if a heartbeat has been received within the last 3 seconds.
bool hidLinkIsUp();

// Milliseconds since the last received heartbeat, or UINT32_MAX if none yet.
uint32_t hidLinkMsSinceHeartbeat();
