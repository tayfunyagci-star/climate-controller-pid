// Durum LED şeridi uygulaması: çekirdek + ağ durumundan LED durumlarını seçer (cc_ledstrip), WS2812B'ye
// yazar, parlaklık/renk ayarlarını NVS "led" alanında saklar. Web ve konsol aynı apply() yolunu kullanır.
// LED işi proses bütçesini tüketmez: çekirdek kilidi kısa zaman aşımıyla denenir, alınamazsa son girdi kalır.
#pragma once
#include <cstdint>
#include "core_api.h"

namespace leds {

void begin();                                    // NVS yükle + sürücüyü başlat (loop görevinden)
void service(uint32_t now_ms);                   // loop: kendi 50 ms temposuyla çalışır
cc::LedConfig config();
void states(uint8_t out[cc::kLedCount]);         // son seçilen durumlar (UI canlı gösterimi)
bool driverOk();
// Aday bütünüyle doğrulanır; hata = hiçbir şey değişmez
bool apply(const cc::LedConfig& c, const char** err);

}  // namespace leds
