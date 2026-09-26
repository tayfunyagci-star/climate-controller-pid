// Web erişim mantığı (F4) — SECURITY.md §2. Saf: kriptografi (PBKDF2, RNG) çağıran tarafta.
//   SessionTable: 128 bit belirteç, en çok 4 oturum (dolunca en eski düşer), süre sonu, parola değişiminde temizlik
//   LoginLimiter: IP başına ve genel 5 hatalı / 5 dk → 5 dk bekleme
#pragma once
#include <cstddef>
#include <cstdint>

namespace cc {

constexpr size_t kTokenLen = 16;
constexpr uint8_t kMaxSessions = 4;

bool ctEqual(const uint8_t* a, const uint8_t* b, size_t n);   // sabit zamanlı
void toHex(const uint8_t* in, size_t n, char* out);           // out: 2n+1
bool fromHex(const char* s, uint8_t* out, size_t n);          // tam 2n onaltılık karakter

class SessionTable {
 public:
  // now/ttl: saniye (monoton). Dönüş: yuva indeksi
  int add(const uint8_t token[kTokenLen], uint32_t now, uint32_t ttl_s);
  bool valid(const uint8_t token[kTokenLen], uint32_t now) const;
  uint32_t expiresIn(const uint8_t token[kTokenLen], uint32_t now) const;   // 0 = geçersiz
  bool remove(const uint8_t token[kTokenLen]);
  void clear();
  uint8_t count(uint32_t now) const;

 private:
  struct Slot {
    bool used = false;
    uint8_t token[kTokenLen] = {};
    uint32_t created = 0, expires = 0;
  };
  int find(const uint8_t token[kTokenLen], uint32_t now) const;
  Slot s_[kMaxSessions];
};

class LoginLimiter {
 public:
  static constexpr uint8_t kMaxFails = 5;
  static constexpr uint32_t kWindowS = 300, kLockS = 300;
  uint32_t lockedFor(uint32_t ip, uint32_t now) const;   // 0 = deneme serbest
  void fail(uint32_t ip, uint32_t now);
  void success(uint32_t ip);

 private:
  struct Entry {
    uint32_t ip = 0, first = 0, lock_until = 0;
    uint8_t fails = 0;
    bool used = false;
  };
  static uint32_t lockOf(const Entry& e, uint32_t now);
  static void bump(Entry& e, uint32_t now);
  Entry ip_[8];
  Entry global_;
};

}  // namespace cc
