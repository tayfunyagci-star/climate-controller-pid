// InterlockEngine + OutputGuard — OUTPUT_AND_INTERLOCKS §5–§6, SAFETY_DESIGN L2.
// requested → effective + reason. Kurallar öncelik sırasıyla (I-1…I-9); I-10/I-11 komut
// katmanında (ClimateCore) uygulanır. Değişmez: (R1 ∨ R2) ⇒ HF; prestart; her kapanışta post-cool.
#pragma once
#include "cc_config.h"

namespace cc {

struct InterlockParams {
  uint32_t prestart_ms = 3000;
  uint32_t heater_min_on_ms = 1000, heater_min_off_ms = 1000;
  uint32_t hf_min_on_ms = kHeaterFanMinOnS * 1000u, hf_min_off_ms = kHeaterFanMinOffS * 1000u;
  uint32_t vf_min_on_ms = 60000, vf_min_off_ms = 60000;
  PostCoolMode pc_mode = PostCoolMode::TIME;
  uint32_t pc_ms = 60000, pc_min_ms = 20000, pc_max_ms = 600000;
  float pc_safe_temp = 40.0f;
  static InterlockParams fromConfig(const Config& c);
};

struct InterlockInput {
  uint32_t dt_ms = 100;
  bool heat_chain = false;              // ısıtma zinciri ısı istiyor (talep > 0 ∧ izin, ya da servis R testi)
  bool r_req[2] = {false, false};       // PowerManager veya servis testi
  bool hf_req = false;                  // heater_fan_manual (veya servis HF testi)
  bool vf_req = false;                  // havalandırma mantığının isteği
  bool heater_lockout = false;          // I-1: Safety kilidi / FAILSAFE / OTA
  bool arm = false;                     // HEATER_ARM
  Reason lockout_reason = Reason::SAFETY_LOCKOUT;
  bool vf_force_on = false;             // I-4: OVERTEMP (T1)
  bool hf_force_on = false;             // S2: T2 aşırı sıcaklık → HF ON
  bool antifreeze = false;              // I-5
  Reason vf_inhibit = Reason::NONE;     // I-6 / I-9 ve mod kaynaklı kapatma
  float t2 = kNaN;
  Quality t2_q = Quality::DISABLED;
};

struct InterlockOutput {
  bool eff[OUT_COUNT] = {false, false, false, false};
  Reason reason[OUT_COUNT] = {Reason::NONE, Reason::NONE, Reason::NONE, Reason::NONE};
  bool post_cool = false;
  bool boot_post_cool = false;
  bool prepurge = false;
  bool post_cool_timeout = false;       // POST_COOL_TIMEOUT uyarısı (fan açık kalır)
  uint32_t post_cool_elapsed_ms = 0;
  uint32_t post_cool_remaining_ms = 0;  // TIME bileşeni için
  uint32_t hf_on_ms = 0;
  bool ev_post_cool_start = false, ev_post_cool_end = false;
  bool ev_on[OUT_COUNT] = {false, false, false, false};
  bool ev_off[OUT_COUNT] = {false, false, false, false};
};

class InterlockEngine {
 public:
  // Boot: çıkışlar bilinmeyen süredir kapalı sayılır (min OFF sağlanmış); tekrar-başlatma döngüsünü
  // RESTART_STORM sınırlar.
  explicit InterlockEngine(const InterlockParams& p = InterlockParams()) : p_(p) {
    for (auto& t : off_t_) t.saturate();
  }
  void setParams(const InterlockParams& p) { p_ = p; }
  // Boot'ta heater_was_on ∧ reset nedeni WDT/PANIC/BROWNOUT → Heater Fan post-cool (D-20)
  void startBootPostCool();
  InterlockOutput step(const InterlockInput& in);
  const InterlockOutput& last() const { return last_; }
  uint32_t onMs(uint8_t o) const { return on_t_[o].ms; }
  uint32_t offMs(uint8_t o) const { return off_t_[o].ms; }
  bool heaterWasOn() const { return was_heating_ || eff_[R1] || eff_[R2]; }

 private:
  bool postCoolDone(const InterlockInput& in) const;

  InterlockParams p_;
  bool eff_[OUT_COUNT] = {false, false, false, false};
  Timer on_t_[OUT_COUNT], off_t_[OUT_COUNT];
  bool was_heating_ = false;
  bool post_cool_ = false;
  bool boot_pc_ = false;
  Timer pc_t_;
  InterlockOutput last_;
};

// OutputManager'daki ikinci kontrol (savunma derinliği) ve sıralı uygulama:
// açılışta önce HF, sonra R; kapanışta önce R, sonra HF. Aynı tikte R açılışı ve HF kapanışı olmaz.
class OutputGuard {
 public:
  struct Result {
    bool out[OUT_COUNT];
    bool violation;  // istenen durum değişmezi ihlal ediyordu (INTERNAL_FAULT girdisi)
  };
  Result apply(const bool desired[OUT_COUNT], uint32_t dt_ms);
  const bool* applied() const { return applied_; }
  uint32_t switchCount(uint8_t o) const { return switches_[o]; }
  uint64_t onTimeMs(uint8_t o) const { return on_ms_[o]; }
  void setCounters(uint8_t o, uint32_t sw, uint64_t on_ms) { switches_[o] = sw; on_ms_[o] = on_ms; }

 private:
  bool applied_[OUT_COUNT] = {false, false, false, false};
  uint32_t switches_[OUT_COUNT] = {0, 0, 0, 0};
  uint64_t on_ms_[OUT_COUNT] = {0, 0, 0, 0};
};

}  // namespace cc
