#include "cc_auth.h"
#include <cstring>

namespace cc {

bool ctEqual(const uint8_t* a, const uint8_t* b, size_t n) {
  uint8_t d = 0;
  for (size_t i = 0; i < n; ++i) d |= (uint8_t)(a[i] ^ b[i]);
  return d == 0;
}

void toHex(const uint8_t* in, size_t n, char* out) {
  static const char k[] = "0123456789abcdef";
  for (size_t i = 0; i < n; ++i) { out[2 * i] = k[in[i] >> 4]; out[2 * i + 1] = k[in[i] & 15]; }
  out[2 * n] = 0;
}

static int nib(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool fromHex(const char* s, uint8_t* out, size_t n) {
  if (!s || strlen(s) != 2 * n) return false;
  for (size_t i = 0; i < n; ++i) {
    const int a = nib(s[2 * i]), b = nib(s[2 * i + 1]);
    if (a < 0 || b < 0) return false;
    out[i] = (uint8_t)(a << 4 | b);
  }
  return true;
}

// ---------------------------------------------------------------- oturumlar
int SessionTable::find(const uint8_t token[kTokenLen], uint32_t now) const {
  int hit = -1;
  for (int i = 0; i < kMaxSessions; ++i)   // erken çıkış yok: süre, eşleşmeye göre değişmez
    if (s_[i].used && (int32_t)(s_[i].expires - now) > 0 && ctEqual(s_[i].token, token, kTokenLen)) hit = i;
  return hit;
}

int SessionTable::add(const uint8_t token[kTokenLen], uint32_t now, uint32_t ttl_s) {
  int slot = -1;
  for (int i = 0; i < kMaxSessions && slot < 0; ++i)
    if (!s_[i].used || (int32_t)(s_[i].expires - now) <= 0) slot = i;
  if (slot < 0) {   // dolu: en eski oturum düşer
    slot = 0;
    for (int i = 1; i < kMaxSessions; ++i) if ((int32_t)(s_[i].created - s_[slot].created) < 0) slot = i;
  }
  Slot& s = s_[slot];
  s.used = true;
  memcpy(s.token, token, kTokenLen);
  s.created = now;
  s.expires = now + ttl_s;
  return slot;
}

bool SessionTable::valid(const uint8_t token[kTokenLen], uint32_t now) const { return find(token, now) >= 0; }

uint32_t SessionTable::expiresIn(const uint8_t token[kTokenLen], uint32_t now) const {
  const int i = find(token, now);
  return i < 0 ? 0 : s_[i].expires - now;
}

bool SessionTable::remove(const uint8_t token[kTokenLen]) {
  for (Slot& s : s_)
    if (s.used && ctEqual(s.token, token, kTokenLen)) { s = Slot(); return true; }
  return false;
}

void SessionTable::clear() { for (Slot& s : s_) s = Slot(); }

uint8_t SessionTable::count(uint32_t now) const {
  uint8_t n = 0;
  for (const Slot& s : s_) if (s.used && (int32_t)(s.expires - now) > 0) ++n;
  return n;
}

// ---------------------------------------------------------------- deneme sınırı
uint32_t LoginLimiter::lockOf(const Entry& e, uint32_t now) {
  return (e.used && (int32_t)(e.lock_until - now) > 0) ? e.lock_until - now : 0;
}

void LoginLimiter::bump(Entry& e, uint32_t now) {
  if (!e.used || now - e.first > kWindowS) { e.used = true; e.first = now; e.fails = 0; }
  if (++e.fails >= kMaxFails) { e.lock_until = now + kLockS; e.fails = 0; e.first = now; }
}

uint32_t LoginLimiter::lockedFor(uint32_t ip, uint32_t now) const {
  uint32_t l = lockOf(global_, now);
  for (const Entry& e : ip_) if (e.used && e.ip == ip) { const uint32_t x = lockOf(e, now); if (x > l) l = x; }
  return l;
}

void LoginLimiter::fail(uint32_t ip, uint32_t now) {
  Entry* slot = nullptr;
  for (Entry& e : ip_) if (e.used && e.ip == ip) slot = &e;
  if (!slot) {
    for (Entry& e : ip_) if (!e.used || (now - e.first > kWindowS && !lockOf(e, now))) { slot = &e; break; }
    if (!slot) slot = &ip_[0];
    *slot = Entry();
    slot->ip = ip;
  }
  bump(*slot, now);
  // Genel sınır: IP değiştirerek deneme (dağıtık) da yavaşlatılır — genel eşik IP eşiğinin 4 katı
  if (!global_.used || now - global_.first > kWindowS) { global_.used = true; global_.first = now; global_.fails = 0; }
  if (++global_.fails >= kMaxFails * 4) { global_.lock_until = now + kLockS; global_.fails = 0; global_.first = now; }
}

void LoginLimiter::success(uint32_t ip) {
  for (Entry& e : ip_) if (e.used && e.ip == ip && !e.lock_until) e = Entry();
  for (Entry& e : ip_) if (e.used && e.ip == ip) e.fails = 0;
}

}  // namespace cc
