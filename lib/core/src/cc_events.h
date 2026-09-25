// Olay günlüğü RAM halkası — ALARM_AND_EVENTS §5. Kalıcı halka StorageTask'tadır (F3).
#pragma once
#include "cc_types.h"

namespace cc {

enum class EvSrc : uint8_t { STATE, SAFETY, CONTROLLER, OUTPUT, ALARM, COMMAND, CONFIG, NET, SYSTEM, SERVICE };
const char* name(EvSrc);

enum class EvCode : uint16_t {
  NONE,
  STATE_CHANGE,        // val = yeni SysState
  HEATING_START,       // val = T1
  POST_COOL_START,
  POST_COOL_END,
  STAGE2_ON,           // val = talep
  STAGE2_OFF,
  LEAD_ROTATED,        // val = yeni lider (0=R1)
  OUTPUT_ON,           // val = çıkış indeksi (HF/VF; R için yalnız dönem sınırları)
  OUTPUT_OFF,
  SAFETY_TRIP,         // val = FailsafeReason
  SAFETY_RESET,
  SAFETY_RESET_REFUSED,
  ALARM_TRANSITION,    // val = AlarmId, msg alanında durum
  COMMAND,             // val = komut değeri
  COMMAND_REJECTED,
  CONFIG_CHANGE,
  SCHEDULE_REQUEST_EXPIRED,  // val = 0 night, 1 away
  BOOST_END,
  MANUAL_TIMEOUT,
  MANUAL_VENT_TIMEOUT,
  ANTIFREEZE_ON,
  ANTIFREEZE_OFF,
  SERVICE_ENTER,
  SERVICE_EXIT,
  SERVICE_TIMEOUT,
  SERVICE_TEST,        // val = çıkış indeksi
  OTA_PREP,
  OTA_TIMEOUT,
  DEVICE_BOOT,
  BOOT_POST_COOL,
  LOCAL_LOCK,
  PROGRAM_START,       // val = program indeksi
  PROGRAM_END,
  PROGRAM_HOLD,
  PROGRAMS_CHANGED,
  COUNT_
};
const char* name(EvCode);

struct Event {
  uint32_t seq = 0;
  uint32_t up_s = 0;
  Severity sev = Severity::INFO;
  EvSrc src = EvSrc::SYSTEM;
  EvCode code = EvCode::NONE;
  CmdSource actor = CmdSource::SYSTEM;
  float val = kNaN;
  uint16_t aux = 0;  // ek kod (ör. alarm durumu, komut sonucu)
};

template <uint16_t N>
class EventRing {
 public:
  void push(Event e) {
    e.seq = ++seq_;
    if (n_ == N) { head_ = (uint16_t)((head_ + 1) % N); --n_; ++overwritten_; }
    buf_[(head_ + n_) % N] = e;
    ++n_;
  }
  uint16_t size() const { return n_; }
  const Event& at(uint16_t i) const { return buf_[(head_ + i) % N]; }  // 0 = en eski
  uint32_t overwritten() const { return overwritten_; }
  uint32_t lastSeq() const { return seq_; }
  void setSeqBase(uint32_t s) { seq_ = s; }
  // Koda göre son olayı ara (test/tanı)
  bool contains(EvCode c) const {
    for (uint16_t i = 0; i < n_; ++i) if (at(i).code == c) return true;
    return false;
  }
  uint16_t count(EvCode c) const {
    uint16_t k = 0;
    for (uint16_t i = 0; i < n_; ++i) if (at(i).code == c) ++k;
    return k;
  }
  void clear() { n_ = 0; head_ = 0; }

 private:
  Event buf_[N];
  uint16_t head_ = 0, n_ = 0;
  uint32_t seq_ = 0, overwritten_ = 0;
};

}  // namespace cc
