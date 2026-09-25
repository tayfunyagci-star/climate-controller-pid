// Seri konsol (UART0, 115200): durum, olay günlüğü, yerel servis komutları, Wi-Fi/NTP kurulumu (F2).
// loop() görevinde çalışır (öncelik 1); çekirdeğe yalnız kısa kilitle erişir, yazdırma kilit dışındadır.
#pragma once
#include "boot_state.h"

namespace app {
void consoleBegin(const BootState& bs);
void consoleService();
}
