// Çekirdek başlıklarının platform kodundan tek giriş noktası. Arduino/IDF başlıkları bazı adları makro
// olarak tanımlar; çekirdek aynı adları numaralandırıcı olarak kullanır (PidMode::PI, Saturation::HIGH/LOW,
// Quality::DISABLED, …). Makrolar çekirdek başlıkları ayrıştırılırken geçici olarak kaldırılır, sonra geri
// yüklenir. Liste, Arduino-ESP32 2.0.17 makro kümesi ile çekirdek tanımlayıcılarının kesişimidir
// (g++ -dM -E ile çıkarıldı; CHANGELOG F2). Çekirdek kaynakları (lib/core) Arduino.h içermez; sarmalayıcı yalnız src/ içindir.
#pragma once
#pragma push_macro("PI")
#pragma push_macro("HIGH")
#pragma push_macro("LOW")
#pragma push_macro("DISABLED")
#pragma push_macro("OUTPUT")
#undef PI
#undef HIGH
#undef LOW
#undef DISABLED
#undef OUTPUT
#include "cc_core.h"
#include "cc_dht.h"
#include "cc_ledstrip.h"
#include "cc_mqtt_map.h"
#pragma pop_macro("OUTPUT")
#pragma pop_macro("DISABLED")
#pragma pop_macro("LOW")
#pragma pop_macro("HIGH")
#pragma pop_macro("PI")
