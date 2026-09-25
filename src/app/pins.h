// Kulübe İklim Kontrolörü — ESP32-S3-DevKitC-1 (N8R8/N8R2) pin haritası. Onay: 25.09.2026 (CHANGELOG F2).
// Kaçınılan pinler: 0/3/45/46 strapping (0 yalnız BOOT butonu girişi), 19/20 USB, 26–37 flash/PSRAM,
// 43/44 UART0 (seri log/konsol). Çıkış sırası cc::Out ile aynıdır (R1, R2, HF, VF).
#pragma once
#include <driver/gpio.h>

namespace hw {

// DHT22/AM2302 veri hattı: 4.7 kΩ pull-up → 3.3 V, 100 nF sensör yanında. RMT RX ile okunur.
constexpr gpio_num_t PIN_DHT = GPIO_NUM_4;

// R1/R2: sıfır geçişli SSR, NPN/MOSFET low-side sürücü (SSR girişi 5 V'tan). Aktif-HIGH;
// tabanda/kapıda 10 kΩ pull-down → reset/boot anında (GPIO yüksek empedans) SSR kapalı.
constexpr gpio_num_t PIN_R1 = GPIO_NUM_5;
constexpr gpio_num_t PIN_R2 = GPIO_NUM_6;
// HF/VF: 5 V optokuplörlü röle modülü, aktif-LOW. JD-VCC jumper'ı sökülü: VCC = 3.3 V (opto),
// JD-VCC = 5 V (bobin); IN hattında 10 kΩ pull-up → 3.3 V → reset/boot anında röle bırakılmış.
constexpr gpio_num_t PIN_HF = GPIO_NUM_7;
constexpr gpio_num_t PIN_VF = GPIO_NUM_15;

// Kartın BOOT butonu (strapping; yalnız boot sonrası giriş olarak okunur) ve dahili WS2812 LED.
constexpr gpio_num_t PIN_BOOT_BTN = GPIO_NUM_0;
constexpr gpio_num_t PIN_RGB = GPIO_NUM_48;  // DevKitC-1 v1.0; v1.1 kartlarda GPIO38 (F4'te ayar)

// Yedek (yapılandırılmaz): GPIO17 ileride HEATER_ARM, GPIO8/9 I²C (RTC/SHT), GPIO16 1-Wire (T2).

struct OutPin {
  gpio_num_t pin;
  bool active_high;
};
constexpr OutPin kOutPins[4] = {
    {PIN_R1, true},   // cc::R1
    {PIN_R2, true},   // cc::R2
    {PIN_HF, false},  // cc::HF
    {PIN_VF, false},  // cc::VF
};

// RMT kanalları (IDF 4.4 legacy sürücü; ESP32-S3: TX 0–3, RX 4–7). Arduino neopixelWrite kullanılmaz.
constexpr int RMT_CH_LED_TX = 0;
constexpr int RMT_CH_DHT_RX = 4;  // 2 bellek bloğu → kanal 5 kullanılmaz

}  // namespace hw
