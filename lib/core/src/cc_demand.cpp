#include "cc_demand.h"

namespace cc {

DemandOutput DemandConditioner::step(const DemandInput& in) {
  DemandOutput o;
  float raw = 0;
  if (in.permitted) {
    if (in.source == DemandSource::PID) {
      raw = in.pid_output;
    } else if (in.source == DemandSource::MANUAL) {
      raw = in.manual_demand;
      if (isValid(in.antifreeze_demand) && in.antifreeze_demand > raw) raw = in.antifreeze_demand;
    }
  }
  float target = clampf(raw, 0.0f, 100.0f);
  if (target > p_.max_demand) { target = p_.max_demand; o.limited = true; }
  if (isValid(in.cap) && target > in.cap) { target = in.cap < 0 ? 0 : in.cap; o.limited = true; }

  // Minimum talep + histerezis
  if (active_) active_ = target >= p_.min_demand && target > 0;
  else active_ = target > 0 && target >= p_.min_demand + p_.min_exit_margin;
  if (!active_) {
    if (target > 0) o.limited = true;
    target = 0;
  }
  o.target = target;

  // Eğim sınırı: yalnız artış; azalış anında (güvenlik yönü)
  if (target <= out_) {
    out_ = target;
  } else {
    const float step = p_.slew_pct_per_min * (in.dt_s / 60.0f);
    float next = out_ + step;
    if (next >= target) next = target;
    else o.limited = true;
    out_ = next;
  }
  o.heat_demand = out_;
  return o;
}

}  // namespace cc
