// Gömülü web sunucusu (port 80) — yalnız NetTask çağırır (WebServer nesnesinin tek sahibi).
// UI varlıkları, AP kurulum akışı, canlı veri, komut, ayarlar (bölüm bölüm), olaylar/alarmlar/programlar, trend,
// oturum/parola (F4), servis modu ve PIN, sayaç sıfırlama, web OTA (hazırlık + ham gövde), fabrika ayarı.
// Erişim: parola tanımsızsa açık; tanımlıysa misafir okuma dışında oturum (WEB_SCADA_UI §13, SECURITY §2).
#pragma once

namespace web {
void begin();    // rotaları kaydeder (ağ yığını gerektirmez)
void start();    // dinleme soketini açar (ilk WiFi.mode çağrısından sonra)
void handle();   // NetTask döngüsünden
const char* uiBuild();   // gömülü UI revizyonu (ui_generated.h yalnız web.cpp'de derlenir)
}
