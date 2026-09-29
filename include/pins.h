#pragma once

// Waveshare ESP32-S3-Touch-LCD-1.47 pin map.
// Source: ESP32-S3-Touch-LCD-1.47 schematic. Where the vendor demos disagree
// with the schematic, the schematic wins (PLAN.md §3.4).

namespace pins {

// LCD: JD9853 over 4-wire SPI on SPI2 (FSPI). No MISO.
constexpr int LCD_SCLK = 38;
constexpr int LCD_MOSI = 39;
constexpr int LCD_CS = 21;
constexpr int LCD_DC = 45;
constexpr int LCD_RST = 40;  // The vendor hello-world passes 47, which is TP_RST.
constexpr int LCD_BL = 46;   // NPN transistor, active high.

// Touch: AXS5106L on I2C at 0x63. The bus is also broken out on header P1.10/P1.12.
constexpr int TP_SDA = 42;
constexpr int TP_SCL = 41;
constexpr int TP_RST = 47;  // Vendor IDF BSP has RST/INT swapped; schematic says RST=47, INT=48.
constexpr int TP_INT = 48;

// PN532 V3 on header P1, SPI3 (HSPI). Interface switches: I0=L, I1=H.
constexpr int NFC_SCK = 4;   // P1.17
constexpr int NFC_MISO = 5;  // P1.19
constexpr int NFC_MOSI = 6;  // P1.21
constexpr int NFC_SS = 7;    // P1.22
constexpr int NFC_IRQ = 8;   // P1.20, optional

// PN532 HSU (UART) fallback, reusing the same wires. Interface switches: I0=L, I1=L.
constexpr int NFC_HSU_TX = 4;  // ESP TX -> PN532 RX
constexpr int NFC_HSU_RX = 5;  // ESP RX <- PN532 TX

constexpr int BOOT_BUTTON = 0;  // Active low. Held at reset = download mode.
constexpr int BAT_ADC = 12;     // 200k/100k divider; unused on USB power.

}  // namespace pins
