// PowerManager — OUTPUT_AND_INTERLOCKS §2–§4, ADR-002.
// heat_demand (%) → kademe (0/1/2) → R1/R2 duty → zaman-oransal istek. GPIO görmez.
// Kademe 1: lider modüle; kademe 2: lider tam + takipçi modüle. 55/45 % histerezis + min kalış.
#pragma once
#include "cc_config.h"

namespace cc {

struct PowerParams {
  DriverKind driver = DriverKind::SSR_ZC;
  uint32_t window_ms = 20000;
  uint32_t min_on_ms = 1000;     // min darbe (ON kısmı)
  uint32_t min_off_ms = 1000;    // min darbe (OFF kısmı)
  float stage2_on = 55.0f;
  float stage2_off = 45.0f;
  uint32_t dwell_ms = 120000;
  float power_w[2] = {0, 0};     // 0/0 → eşit
  LeadRotation rotation = LeadRotation::DAILY;
  static PowerParams fromConfig(const Config& c);
};

struct PowerInput {
  float demand = 0;              // heat_demand %
  bool enabled = false;          // ısıtma zinciri talebe izin veriyor (aksi hâlde kademe 0)
  bool rotate = false;           // gün devri / 24 sa: lider değişimi talebi
  uint32_t dt_ms = 100;
};

struct PowerOutput {
  uint8_t stage = 0;
  float duty[2] = {0, 0};        // R1, R2 hedef oranı %
  bool req[2] = {false, false};  // R1, R2 anlık istek (zaman-oransal)
  float applied = 0;             // pencere nicemlemesi sonrası uygulanan talep %
  uint8_t lead = 0;              // lider rezistans (0=R1, 1=R2)
  bool ev_stage_up = false, ev_stage_down = false, ev_rotated = false;
};

class PowerManager {
 public:
  explicit PowerManager(const PowerParams& p = PowerParams());
  void setParams(const PowerParams& p);
  PowerOutput step(const PowerInput& in);
  const PowerOutput& last() const { return last_; }
  uint8_t stage() const { return stage_; }
  float stage1Capacity() const;  // lider kapasitesi (0..1)

 private:
  struct Channel {
    uint32_t pos = 0;      // pencere içi konum
    uint32_t on_ms = 0;    // bu pencerede ON süresi (mandallı)
    float duty = 0;
    bool active = false;
  };
  uint32_t quantize(float duty) const;
  void autoLead();

  PowerParams p_;
  uint8_t stage_ = 0;
  Timer stage_t_;
  uint8_t lead_ = 0;
  bool rotate_pending_ = false;
  Channel ch_[2];
  bool prev_req_[2] = {false, false};
  PowerOutput last_;
};

}  // namespace cc
