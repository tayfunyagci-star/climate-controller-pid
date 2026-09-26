// Web erişimi ve servis PIN'i (F4) — SECURITY.md §2. NVS "auth": kullanıcı, misafir okuma, oturum süresi,
// parola ve PIN yalnız PBKDF2-HMAC-SHA256 özeti (16 B tuz). Oturumlar RAM'de (yeniden başlatmada düşer).
// Parola tanımsızken cihaz açıktır (kalıcı uyarı); tanımlanınca misafir okuma dışında her uç oturum ister.
#pragma once
#include <cstdint>

namespace auth {

struct Settings {
  char user[33] = "admin";
  bool guest_read = false;
  uint8_t session_h = 8;          // 1–24; "beni hatırla" 14 gün
};

enum class Login : uint8_t { OK, BAD, LOCKED };

void begin();
bool passwordSet();
bool pinSet();
Settings settings();
bool applySettings(const Settings& s, const char** err, const char** field);

Login login(const char* user, const char* pw, uint32_t ip, bool remember, char token_hex[33], uint32_t& ttl_s, uint32_t& lock_s);
bool check(const char* token_hex, uint32_t* expires_in_s);
void logout(const char* token_hex);
// old: mevcut parola (tanımlıysa zorunlu). pw "" = korumayı kaldır. Başarıda bütün oturumlar düşer.
bool setPassword(const char* old, const char* pw, uint32_t ip, const char** err, uint32_t* lock_s);
void clearPassword();             // fiziksel kurtarma (BOOT 10 s)
bool setPin(const char* pin, const char** err);
bool verifyPin(const char* pin, uint32_t ip, uint32_t* lock_s);
void factoryErase();

}  // namespace auth
