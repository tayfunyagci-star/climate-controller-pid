// Kulübe İklim Kontrolörü — ESP32 DevKit V1 (ESP32-WROOM-32, 4 MB flash, PSRAM yok) pin haritası.
// Hedef değişikliği 25.09.2026 (CHANGELOG F2.1). Kaçınılan pinler: strapping 0/2/5/12/15 (0 yalnız BOOT girişi,
// 2 yalnız kart LED'i), 6–11 flash, 1/3 UART0 (seri konsol), 34–39 yalnız giriş, 14 boot'ta PWM üretir.
// Çıkış sırası cc::Out ile aynıdır (R1, R2, HF, VF).
#pragma once
#include <driver/gpio.h>

namespace hw {

// DHT22/AM2302 veri hattı: 4.7 kΩ pull-up → 3.3 V, 100 nF sensör yanında. RMT RX ile okunur.
constexpr gpio_num_t PIN_DHT = GPIO_NUM_4;

// R1/R2: sıfır geçişli SSR, NPN low-side sürücü (SSR girişi 5 V'tan). Aktif-HIGH;
// tabanda 10 kΩ pull-down → reset/boot anında (GPIO yüksek empedans) SSR kapalı.
constexpr gpio_num_t PIN_R1 = GPIO_NUM_25;
constexpr gpio_num_t PIN_R2 = GPIO_NUM_26;
// HF/VF: 5 V optokuplörlü röle modülü, aktif-LOW. JD-VCC jumper'ı sökülü: VCC = 3.3 V (opto),
// JD-VCC = 5 V (bobin); IN hattında 10 kΩ pull-up → 3.3 V → reset/boot anında röle bırakılmış.
constexpr gpio_num_t PIN_HF = GPIO_NUM_32;
constexpr gpio_num_t PIN_VF = GPIO_NUM_33;

// Kart üstü: BOOT butonu (strapping; yalnız boot sonrası giriş) ve mavi LED (GPIO2, aktif-HIGH;
// strapping pini — boot'ta kart üstü dirençle LOW kalır, çıkış yalnız boot sonrası sürülür).
constexpr gpio_num_t PIN_BOOT_BTN = GPIO_NUM_0;
constexpr gpio_num_t PIN_LED = GPIO_NUM_2;

// Yedek (yapılandırılmaz): GPIO13 ileride HEATER_ARM, GPIO21/22 I²C (RTC/SHT), GPIO18 1-Wire (T2).

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

// RMT (IDF 4.4 legacy sürücü; ESP32: 8 kanal, her biri TX/RX). DHT22 RX 2 bellek bloğu → kanal 5 kullanılmaz.
constexpr int RMT_CH_DHT_RX = 4;

}  // namespace hw
