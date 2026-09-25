// Talep koşullandırma — PID_DESIGN §5, CONTROL_ARCHITECTURE §3.4, §5.2.
// pid_output / manual_heat_demand → heat_demand (%). Min talep (histerezisli), eğim (yalnız artış),
// max_heat_demand ve havalandırma üst sınırı. Güvenlik yönünde (azalış) sınır yoktur.
#pragma once
#include "cc_types.h"

namespace cc {

struct DemandParams {
  float min_demand = 5.0f;      // %
  float min_exit_margin = 2.0f; // 0'dan çıkmak için min + 2 %
  float slew_pct_per_min = 10.0f;
  float max_demand = 100.0f;
};

enum class DemandSource : uint8_t { NONE, PID, MANUAL };

struct DemandInput {
  bool permitted = false;        // ısıtma izni (sistem, mod, safety, sensör)
  DemandSource source = DemandSource::NONE;
  float pid_output = 0;          // AUTO veya antifreeze PID çıktısı
  float manual_demand = 0;       // MANUAL
  float antifreeze_demand = kNaN;// MANUAL'da antifreeze PID talebi (max alınır)
  float cap = kNaN;              // havalandırma üst sınırı (VENT_WINS) — NaN yok
  float dt_s = 2.0f;
};

struct DemandOutput {
  float heat_demand = 0;
  float target = 0;              // eğimden önceki hedef
  bool limited = false;          // talep sınırlandı (max, cap, min, slew)
};

class DemandConditioner {
 public:
  explicit DemandConditioner(const DemandParams& p = DemandParams()) : p_(p) {}
  void setParams(const DemandParams& p) { p_ = p; }
  DemandOutput step(const DemandInput& in);
  void reset() { out_ = 0; active_ = false; }
  // Bumpless başlangıç (ör. MANUAL→AUTO) için mevcut talebi ayarla
  void preset(float v) { out_ = v; active_ = v > 0; }
  float current() const { return out_; }

 private:
  DemandParams p_;
  float out_ = 0;
  bool active_ = false;
};

}  // namespace cc
