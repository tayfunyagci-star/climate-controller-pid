// Gömülü web sunucusu (port 80) — yalnız NetTask çağırır (WebServer nesnesinin tek sahibi).
// F2.2 kapsamı: UI varlıkları (include/ui_generated.h), AP kurulum akışı (captive portal, /scan,
// /api/settings Wi-Fi + Ağ bölümü, /api/reset-wifi, /api/reboot), canlı veri (/api/data), komut (/api/cmd),
// olaylar, alarmlar, programlar (okuma). Kalan uçlar (oturum, parola, trend, servis, OTA web yüklemesi) F4'tedir.
#pragma once

namespace web {
void begin();    // rotaları kaydeder (ağ yığını gerektirmez)
void start();    // dinleme soketini açar (ilk WiFi.mode çağrısından sonra)
void handle();   // NetTask döngüsünden
const char* uiBuild();   // gömülü UI revizyonu (ui_generated.h yalnız web.cpp'de derlenir)
}
