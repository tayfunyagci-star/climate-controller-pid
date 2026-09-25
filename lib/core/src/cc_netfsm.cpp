#include "cc_netfsm.h"

namespace cc {

void NetFsm::beginAttempt(const NetInput& in, uint32_t now, NetActions& a) {
  if (!in.configured) {
    if (!ap_) { a.start_ap = true; ap_ = true; a.ev = NetEvent::AP_STARTED; }
    a.stop_sta = true;
    phase_ = NetPhase::AP_ONLY;
    t0_ = now;
    return;
  }
  if (in.static_enabled && !static_failed_ && !in.static_valid) {
    static_failed_ = true;       // geçersiz statik ayar denenmez; doğrudan DHCP
    a.ev = NetEvent::STATIC_TO_DHCP;
  }
  a.begin_sta = true;
  a.use_static = in.static_enabled && !static_failed_;
  if (a.ev == NetEvent::NONE) a.ev = NetEvent::CONNECTING;
  phase_ = NetPhase::CONNECTING;
  t0_ = now;
}

NetActions NetFsm::step(const NetInput& in, uint32_t now) {
  NetActions a;
  if (changed_) {
    changed_ = false;
    if (phase_ == NetPhase::ONLINE) a.stop_services = true;
    static_failed_ = false;
    beginAttempt(in, now, a);
    if (a.ev == NetEvent::CONNECTING) a.ev = NetEvent::CONFIG_CHANGED;
    return a;
  }
  if (in.sta_connected && phase_ != NetPhase::AP_ONLY) {
    if (phase_ != NetPhase::ONLINE) {
      phase_ = NetPhase::ONLINE;
      a.start_services = true;
      a.ev = (in.static_enabled && static_failed_) ? NetEvent::CONNECTED_DHCP_FALLBACK : NetEvent::CONNECTED;
    }
    if (ap_) {
      a.stop_ap = true;
      ap_ = false;
      if (a.ev == NetEvent::NONE) a.ev = NetEvent::AP_STOPPED;
    }
    return a;
  }
  switch (phase_) {
    case NetPhase::ONLINE:   // bağlantı koptu
      a.stop_services = true;
      a.ev = NetEvent::DISCONNECTED;
      phase_ = NetPhase::WAITING;
      t0_ = now;
      break;
    case NetPhase::CONNECTING:
      if ((uint32_t)(now - t0_) >= p_.connect_timeout_ms) {
        if (in.static_enabled && !static_failed_) {
          static_failed_ = true;
          beginAttempt(in, now, a);
          a.ev = NetEvent::STATIC_TO_DHCP;
        } else {
          a.stop_sta = true;
          if (!ap_) { a.start_ap = true; ap_ = true; }
          a.ev = NetEvent::TIMEOUT_TO_AP;
          phase_ = NetPhase::WAITING;
          t0_ = now;
        }
      }
      break;
    case NetPhase::WAITING:
      if ((uint32_t)(now - t0_) >= (ap_ ? p_.retry_ap_ms : p_.retry_sta_ms)) beginAttempt(in, now, a);
      break;
    case NetPhase::AP_ONLY:
      if (in.configured) beginAttempt(in, now, a);   // kimlik başka yoldan geldi (requestReconnect'siz)
      break;
  }
  return a;
}

uint32_t NetFsm::retryInMs(uint32_t now) const {
  if (phase_ != NetPhase::WAITING) return 0;
  const uint32_t per = ap_ ? p_.retry_ap_ms : p_.retry_sta_ms, el = now - t0_;
  return el >= per ? 0 : per - el;
}

bool parseIpv4(const char* s, uint32_t& out) {
  if (!s || !*s) return false;
  uint32_t v = 0;
  for (int part = 0; part < 4; ++part) {
    if (*s < '0' || *s > '9') return false;
    uint32_t n = 0;
    int digits = 0;
    while (*s >= '0' && *s <= '9') {
      n = n * 10 + (uint32_t)(*s - '0');
      if (++digits > 3 || n > 255) return false;
      ++s;
    }
    v = (v << 8) | n;
    if (part < 3) { if (*s != '.') return false; ++s; }
  }
  if (*s) return false;
  out = v;
  return true;
}

bool validStaticIpv4(uint32_t ip, uint32_t mask, uint32_t gw, const char** err) {
  const char* e = nullptr;
  const uint32_t inv = ~mask;
  if (mask == 0 || mask == 0xFFFFFFFFu || (inv & (inv + 1)) != 0) e = "Alt ağ maskesi geçersiz (bitişik olmalı).";
  else if ((ip & mask) != (gw & mask)) e = "IP ile ağ geçidi aynı alt ağda değil.";
  else if (ip == gw) e = "IP adresi ağ geçidiyle aynı olamaz.";
  else if ((ip & inv) == 0) e = "IP, ağın kendi adresi olamaz.";
  else if ((ip & inv) == inv) e = "IP, yayın (broadcast) adresi olamaz.";
  if (err) *err = e;
  return e == nullptr;
}

}  // namespace cc
