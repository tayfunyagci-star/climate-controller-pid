// Reset nedeni ve RTC belleğinde (RTC_NOINIT) yazılım resetlerinden sağ çıkan bayraklar.
// Güç kesintisinde silinir; kalıcı karşılıkları F3'te (StorageTask) eklenir.
#pragma once
#include <cstdint>
#include "core_api.h"

namespace app {

struct BootState {
  const char* reset_reason = "?";
  bool fault_reset = false;
  bool power_on = false;
};

// Reset nedenini okur, RTC bayraklarını doğrular/günceller ve çekirdeğe verilecek BootInfo'yu üretir.
cc::BootInfo readBoot(BootState& bs);
// OutputTask her çevrimde: rezistans açık ya da post-cool sürüyor → boot'ta post-cool gerekir (D-20)
void rtcSetHeaterWasOn(bool v);
// SafetyTask: kilitli safety bitleri (D-16, yazılım resetlerine karşı)
void rtcSetSafetyLatched(uint8_t mask);
// Sağlıklı çalışma süresi pencereyi aşınca hatalı boot sayacı sıfırlanır (restart fırtınası penceresi)
void rtcClearFaultBoots();
uint8_t rtcFaultBoots();
// Acil yazılım reseti öncesi: sonraki boot hatalı sayılır ve post-cool yapılır
void rtcMarkSwFault();

}  // namespace app
