// Çıkış HAL'i — GPIO'nun tek sahibi OutputTask'tır (SYSTEM_ARCHITECTURE §4). Tek istisna:
// SafetyTask'ın acil yolu (emergencyHeatersOff), OutputTask takıldığında rezistans hatlarını pasife çeker.
#pragma once
#include <cstdint>

namespace hal {

// Boot'ta ilk iş: önce pasif seviye yazılır, sonra pin çıkış yapılır (yanlış seviye darbesi yok).
void outputsEarlyInit();
// Mantıksal istekleri (cc::Out sırası) polariteye göre yazar. Her çağrıda dört pin de yeniden yazılır.
void outputsWrite(const bool on[4]);
// Son yazılan mantıksal durum (tanı)
const bool* outputsLast();
// Acil: R1/R2 pasif. ISR/kilit gerektirmez; OutputTask'ı beklemez.
void emergencyHeatersOff();

}  // namespace hal
