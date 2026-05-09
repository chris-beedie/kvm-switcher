/*********************************************************************
 hid_link — UART link to the Xiao RP2350 (kvm-switcher-hid)

 Carries:
   ESP32 -> RP2350: filtered keyboard reports, consumer-control reports,
                    debug log lines (since USB-CDC is unavailable in host mode).
   RP2350 -> ESP32: USB-to-PC mount status, 1Hz heartbeat.

 Frame: 0xAB | type | len | payload[len] | xor(type, len, payload)
*********************************************************************/

#include "hid_link.h"
#include <HardwareSerial.h>
#include <stdarg.h>

// Use UART1 — UART0 is left free for upload/flash with the default pinout.
static HardwareSerial LinkUart(1);

static constexpr uint8_t  FRAME_START   = 0xAB;
static constexpr uint16_t MAX_PAYLOAD   = 220;
static constexpr uint32_t HEARTBEAT_TIMEOUT_MS = 3000;

// ── RX state machine ─────────────────────────────────────────────────────────
enum RxState : uint8_t { RX_IDLE, RX_TYPE, RX_LEN, RX_PAYLOAD, RX_CHECKSUM };
static RxState        rx_state = RX_IDLE;
static uint8_t        rx_type, rx_len, rx_xor;
static uint16_t       rx_idx;
static uint8_t        rx_buf[MAX_PAYLOAD];

// ── Link state ───────────────────────────────────────────────────────────────
static LinkUsbStatus  usb_status     = {false, false};
static uint32_t       last_heartbeat = 0;
static bool           ever_seen_hb   = false;

// ── Helpers ──────────────────────────────────────────────────────────────────
static void writeFrame(uint8_t type, const uint8_t* payload, uint8_t len) {
    if (len > MAX_PAYLOAD) return;
    uint8_t xorv = type ^ len;
    for (uint8_t i = 0; i < len; i++) xorv ^= payload[i];

    uint8_t header[3] = { FRAME_START, type, len };
    LinkUart.write(header, 3);
    if (len) LinkUart.write(payload, len);
    LinkUart.write(&xorv, 1);
}

static void handleFrame(uint8_t type, const uint8_t* payload, uint8_t len) {
    switch (type) {
    case LinkMsg::USB_STATUS:
        if (len >= 1) {
            usb_status.mounted   = (payload[0] & 0x01) != 0;
            usb_status.suspended = (payload[0] & 0x02) != 0;
        }
        break;
    case LinkMsg::HEARTBEAT:
        last_heartbeat = millis();
        ever_seen_hb   = true;
        break;
    default:
        break;  // unknown type — ignore
    }
}

static void rxFeed(uint8_t b) {
    switch (rx_state) {
    case RX_IDLE:
        if (b == FRAME_START) rx_state = RX_TYPE;
        break;
    case RX_TYPE:
        rx_type  = b;
        rx_xor   = b;
        rx_state = RX_LEN;
        break;
    case RX_LEN:
        rx_len   = b;
        rx_xor  ^= b;
        rx_idx   = 0;
        if (rx_len > MAX_PAYLOAD) { rx_state = RX_IDLE; break; }  // bogus length
        rx_state = (rx_len == 0) ? RX_CHECKSUM : RX_PAYLOAD;
        break;
    case RX_PAYLOAD:
        rx_buf[rx_idx++] = b;
        rx_xor ^= b;
        if (rx_idx >= rx_len) rx_state = RX_CHECKSUM;
        break;
    case RX_CHECKSUM:
        if (b == rx_xor) handleFrame(rx_type, rx_buf, rx_len);
        rx_state = RX_IDLE;
        break;
    }
}

// ── Public API ───────────────────────────────────────────────────────────────
void hidLinkBegin(uint8_t tx_pin, uint8_t rx_pin, uint32_t baud) {
    LinkUart.begin(baud, SERIAL_8N1, rx_pin, tx_pin);
}

void hidLinkLoop() {
    while (LinkUart.available()) rxFeed((uint8_t)LinkUart.read());
}

bool hidLinkSendKeyboard(const uint8_t report[8]) {
    writeFrame(LinkMsg::KEYBOARD, report, 8);
    return true;
}

bool hidLinkSendConsumer(uint16_t usage_code) {
    uint8_t pl[2] = { (uint8_t)(usage_code & 0xFF), (uint8_t)(usage_code >> 8) };
    writeFrame(LinkMsg::CONSUMER, pl, 2);
    return true;
}

void hidLinkLog(const char* msg) {
    if (!msg) return;
    size_t n = strlen(msg);
    if (n > MAX_PAYLOAD) n = MAX_PAYLOAD;
    writeFrame(LinkMsg::LOG, (const uint8_t*)msg, (uint8_t)n);
}

void hidLinkLogf(const char* fmt, ...) {
    char buf[MAX_PAYLOAD + 1];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n <= 0) return;
    if (n > (int)MAX_PAYLOAD) n = MAX_PAYLOAD;
    writeFrame(LinkMsg::LOG, (const uint8_t*)buf, (uint8_t)n);
}

// ── Print sink: fan out to Serial (UART0 hardware) and the link ──────────────
LinkPrint Log;

size_t LinkPrint::write(uint8_t c) {
    Serial.write(c);
    if (c == '\r') return 1;            // ignore CR; we split on LF
    if (c == '\n' || len_ >= sizeof(buf_) - 1) {
        if (len_ > 0) {
            buf_[len_] = 0;
            writeFrame(LinkMsg::LOG, (const uint8_t*)buf_, (uint8_t)len_);
            len_ = 0;
        }
    } else {
        buf_[len_++] = (char)c;
    }
    return 1;
}

size_t LinkPrint::write(const uint8_t* buf, size_t n) {
    for (size_t i = 0; i < n; i++) write(buf[i]);
    return n;
}

void LinkPrint::flushLine() {
    if (len_ == 0) return;
    buf_[len_] = 0;
    writeFrame(LinkMsg::LOG, (const uint8_t*)buf_, (uint8_t)len_);
    len_ = 0;
}

LinkUsbStatus hidLinkUsbStatus() { return usb_status; }

bool hidLinkIsUp() {
    if (!ever_seen_hb) return false;
    return (millis() - last_heartbeat) < HEARTBEAT_TIMEOUT_MS;
}

uint32_t hidLinkMsSinceHeartbeat() {
    if (!ever_seen_hb) return UINT32_MAX;
    return millis() - last_heartbeat;
}
