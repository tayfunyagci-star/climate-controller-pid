// Havalandırma mantığı ve ısıtma/havalandırma koordinasyonu — CONTROL_ARCHITECTURE §5, STATE_MACHINE §3.
// Çıktı: VF isteği + inhibit nedeni (InterlockEngine I-6/I-9 girdisi) + ısıtma üst sınırı/engeli.
#pragma once
#include "cc_config.h"

namespace cc {

enum VentSrc : uint8_t {
  VS_TEMP_HIGH = 1u << 0,
  VS_HUMIDITY_HIGH = 1u << 1,
  VS_MANUAL = 1u << 2,
  VS_SCHEDULED = 1u << 3,
  VS_OVERTEMP = 1u << 4,
};

struct VentInput {
  uint32_t dt_ms = 1000;
  OpMode mode = OpMode::AUTO;
  bool controller_enable = true;
  bool service = false;             // SERVICE: otomatik kurallar durur (test ayrı)
  bool ota = false;                 // OTA: ota_vent_state
  float t1 = kNaN;
  Quality t1_q = Quality::MISSING;
  float rh = kNaN;
  Quality rh_q = Quality::MISSING;
  float sp_effective = 21.0f;
  bool heating_active = false;      // ısıtma zinciri PREPURGE/ACTIVE
  bool heat_request = false;        // ısıtma talebi var (koordinasyon öncesi)
  bool antifreeze = false;
  bool overtemp = false;            // Safety OVERTEMP (FORCED)
  bool sensor_fault = false;        // Safety SENSOR/CONFIG/INTERNAL → otomatik kurallar durur
  uint32_t uptime_s = 0;            // periyodik havalandırma için
  bool vf_effective = false;        // önceki tik etkin durumu (rapor için)
  bool program_vent = false;        // yerel program VENTILATE eylemi (SCHEDULED kaynağı)
};

struct VentOutput {
  bool vf_request = false;          // InterlockEngine vf_req
  Reason inhibit = Reason::NONE;    // InterlockEngine vf_inhibit
  uint8_t sources = 0;              // etkin otomatik/manuel istek kaynakları
  bool auto_request = false;
  float heat_cap = kNaN;            // VENT_WINS: heat_demand ≤ cap
  bool heat_inhibit = false;        // vent→ısıtma changeover gecikmesi
  float start_effective = 26.0f;    // ventilation_start_effective
  float stop_effective = 24.0f;
  bool humidity_high = false;       // HUMIDITY_HIGH INFO koşulu (nem yüksek ∧ havalandırma inhibit)
  bool ev_manual_timeout = false;
};

class Ventilation {
 public:
  void setManual(bool on) {
    if (on && !manual_) manual_t_.reset();
    manual_ = on;
  }
  bool manual() const { return manual_; }
  VentOutput step(const Config& c, const VentInput& in);
  const VentOutput& last() const { return last_; }
  // Rapor: V_* durumu (etkin çıkış ve nedenden türetilir)
  static VentState reportState(const VentOutput& v, bool vf_eff, Reason vf_reason_interlock);

 private:
  bool manual_ = false;
  Timer manual_t_;
  bool temp_high_ = false, hum_high_ = false;
  bool last_heating_ = false, last_auto_ = false;
  Timer since_heat_stop_, since_auto_stop_;
  VentOutput last_;
};

}  // namespace cc
