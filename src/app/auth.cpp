#include "auth.h"
#include <Arduino.h>
#include <Preferences.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#include <cstring>
#include "cc_auth.h"

namespace auth {

namespace {

// ESP32 (240 MHz, SHA donanım hızlandırma): 2048 iterasyon ≈ 100–200 ms; hedef ≤ 300 ms (SECURITY §2).
// İterasyon kayıtla saklanır: sonradan artırılırsa eski özetler kendi değeriyle doğrulanır.
constexpr uint32_t kIterPw = 2048, kIterPin = 1024;

SemaphoreHandle_t g_mtx = nullptr;
Settings g_set;
bool g_pw = false, g_pin = false;
uint8_t g_salt[16], g_hash[32], g_pin_salt[16], g_pin_hash[32];
uint32_t g_iter = kIterPw, g_pin_iter = kIterPin;
cc::SessionTable g_sessions;
cc::LoginLimiter g_limit;

struct Lock {
  Lock() { if (g_mtx) xSemaphoreTake(g_mtx, portMAX_DELAY); }
  ~Lock() { if (g_mtx) xSemaphoreGive(g_mtx); }
};

uint32_t nowS() { return millis() / 1000u; }

bool pbkdf2(const char* pw, const uint8_t salt[16], uint32_t iter, uint8_t out[32]) {
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  bool ok = mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1) == 0 &&
            mbedtls_pkcs5_pbkdf2_hmac(&ctx, (const unsigned char*)pw, strlen(pw), salt, 16, iter, 32, out) == 0;
  mbedtls_md_free(&ctx);
  return ok;
}

bool verifyPw(const char* pw) {
  uint8_t h[32];
  return g_pw && pbkdf2(pw ? pw : "", g_salt, g_iter, h) && cc::ctEqual(h, g_hash, 32);
}

bool store() {
  Preferences p;
  if (!p.begin("auth", false)) return false;
  bool ok = p.putString("user", g_set.user) > 0 && p.putBool("guest", g_set.guest_read) == 1 &&
            p.putUChar("sess", g_set.session_h) == 1;
  if (g_pw) ok = ok && p.putBytes("salt", g_salt, 16) == 16 && p.putBytes("hash", g_hash, 32) == 32 && p.putUInt("iter", g_iter) == 4;
  else { p.remove("salt"); p.remove("hash"); }
  if (g_pin) ok = ok && p.putBytes("psalt", g_pin_salt, 16) == 16 && p.putBytes("phash", g_pin_hash, 32) == 32 && p.putUInt("piter", g_pin_iter) == 4;
  else { p.remove("psalt"); p.remove("phash"); }
  p.end();
  return ok;
}

}  // namespace

void begin() {
  g_mtx = xSemaphoreCreateMutex();
  Preferences p;
  if (!p.begin("auth", true)) return;
  if (p.isKey("user")) p.getString("user", g_set.user, sizeof g_set.user);
  g_set.guest_read = p.getBool("guest", false);
  g_set.session_h = p.getUChar("sess", 8);
  if (g_set.session_h < 1 || g_set.session_h > 24) g_set.session_h = 8;
  g_pw = p.getBytesLength("hash") == 32 && p.getBytesLength("salt") == 16;
  if (g_pw) { p.getBytes("salt", g_salt, 16); p.getBytes("hash", g_hash, 32); g_iter = p.getUInt("iter", kIterPw); }
  g_pin = p.getBytesLength("phash") == 32 && p.getBytesLength("psalt") == 16;
  if (g_pin) { p.getBytes("psalt", g_pin_salt, 16); p.getBytes("phash", g_pin_hash, 32); g_pin_iter = p.getUInt("piter", kIterPin); }
  p.end();
}

bool passwordSet() { Lock l; return g_pw; }
bool pinSet() { Lock l; return g_pin; }
Settings settings() { Lock l; return g_set; }

bool applySettings(const Settings& s, const char** err, const char** field) {
  const size_t n = strlen(s.user);
  if (n < 1 || n > 32) { *err = "Web kullanıcı adı 1–32 karakter olmalı."; *field = "user"; return false; }
  if (s.session_h < 1 || s.session_h > 24) { *err = "Oturum süresi 1–24 saat olmalı."; *field = "session_hours"; return false; }
  Lock l;
  const Settings old = g_set;
  g_set = s;
  if (!store()) { g_set = old; *err = "Erişim ayarları yazılamadı; NVS hatası."; *field = nullptr; return false; }
  return true;
}

Login login(const char* user, const char* pw, uint32_t ip, bool remember, char token_hex[33], uint32_t& ttl_s, uint32_t& lock_s) {
  const uint32_t now = nowS();
  Lock l;
  lock_s = g_limit.lockedFor(ip, now);
  if (lock_s) return Login::LOCKED;
  const bool user_ok = user && !strcmp(user, g_set.user);
  const bool pw_ok = verifyPw(pw);                 // kullanıcı yanlış olsa da PBKDF2 çalışır (zamanlama)
  if (!g_pw || !user_ok || !pw_ok) {
    g_limit.fail(ip, now);
    lock_s = g_limit.lockedFor(ip, now);
    return lock_s ? Login::LOCKED : Login::BAD;
  }
  g_limit.success(ip);
  uint8_t t[cc::kTokenLen];
  esp_fill_random(t, sizeof t);                     // donanım RNG (Wi-Fi/BT açıkken gerçek rastgele)
  ttl_s = remember ? 14u * 86400u : (uint32_t)g_set.session_h * 3600u;
  g_sessions.add(t, now, ttl_s);
  cc::toHex(t, sizeof t, token_hex);
  return Login::OK;
}

bool check(const char* token_hex, uint32_t* expires_in_s) {
  uint8_t t[cc::kTokenLen];
  if (!cc::fromHex(token_hex, t, sizeof t)) return false;
  Lock l;
  const uint32_t e = g_sessions.expiresIn(t, nowS());
  if (expires_in_s) *expires_in_s = e;
  return e > 0;
}

void logout(const char* token_hex) {
  uint8_t t[cc::kTokenLen];
  if (!cc::fromHex(token_hex, t, sizeof t)) return;
  Lock l;
  g_sessions.remove(t);
}

bool setPassword(const char* old, const char* pw, uint32_t ip, const char** err, uint32_t* lock_s) {
  const uint32_t now = nowS();
  const size_t n = pw ? strlen(pw) : 0;
  if (n && (n < 8 || n > 128)) { *err = "Parola 8–128 karakter olmalı."; return false; }
  Lock l;
  if (g_pw) {
    *lock_s = g_limit.lockedFor(ip, now);
    if (*lock_s) { *err = "Çok fazla hatalı deneme; biraz sonra yeniden deneyin."; return false; }
    if (!verifyPw(old)) {
      g_limit.fail(ip, now);
      *err = "Mevcut parola yanlış.";
      return false;
    }
    g_limit.success(ip);
  }
  const bool old_pw = g_pw;
  uint8_t os[16], oh[32];
  memcpy(os, g_salt, 16);
  memcpy(oh, g_hash, 32);
  if (n) {
    esp_fill_random(g_salt, 16);
    const uint32_t t0 = millis();
    if (!pbkdf2(pw, g_salt, kIterPw, g_hash)) { *err = "Parola özeti hesaplanamadı."; return false; }
    Serial.printf("[AUTH] PBKDF2 %u iterasyon: %u ms\n", (unsigned)kIterPw, (unsigned)(millis() - t0));
    g_iter = kIterPw;
    g_pw = true;
  } else {
    g_pw = false;
  }
  if (!store()) {
    g_pw = old_pw; memcpy(g_salt, os, 16); memcpy(g_hash, oh, 32);
    *err = "Parola yazılamadı; NVS hatası.";
    return false;
  }
  g_sessions.clear();                               // parola değişiminde bütün oturumlar düşer
  return true;
}

void clearPassword() {
  Lock l;
  g_pw = false;
  store();
  g_sessions.clear();
}

bool setPin(const char* pin, const char** err) {
  const size_t n = pin ? strlen(pin) : 0;
  if (n < 4 || n > 8) { *err = "Servis PIN'i 4–8 rakam olmalı."; return false; }
  for (size_t i = 0; i < n; ++i) if (pin[i] < '0' || pin[i] > '9') { *err = "Servis PIN'i yalnız rakamdan oluşur."; return false; }
  Lock l;
  esp_fill_random(g_pin_salt, 16);
  if (!pbkdf2(pin, g_pin_salt, kIterPin, g_pin_hash)) { *err = "PIN özeti hesaplanamadı."; return false; }
  g_pin_iter = kIterPin;
  g_pin = true;
  if (!store()) { *err = "PIN yazılamadı; NVS hatası."; return false; }
  return true;
}

bool verifyPin(const char* pin, uint32_t ip, uint32_t* lock_s) {
  const uint32_t now = nowS();
  Lock l;
  *lock_s = g_limit.lockedFor(ip, now);
  if (*lock_s || !g_pin) return false;
  uint8_t h[32];
  const bool ok = pbkdf2(pin ? pin : "", g_pin_salt, g_pin_iter, h) && cc::ctEqual(h, g_pin_hash, 32);
  if (ok) g_limit.success(ip); else g_limit.fail(ip, now);
  return ok;
}

void factoryErase() {
  Lock l;
  Preferences p;
  if (p.begin("auth", false)) { p.clear(); p.end(); }
  g_set = Settings();
  g_pw = g_pin = false;
  g_sessions.clear();
}

}  // namespace auth
