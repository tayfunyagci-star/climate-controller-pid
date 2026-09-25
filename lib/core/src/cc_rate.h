// Sabit periyotlu örnek halkası: değişim (Δ) ve doğrusal regresyon eğimi.
#pragma once
#include "cc_types.h"

namespace cc {

template <uint16_t N>
class RateWindow {
 public:
  explicit RateWindow(uint32_t period_ms = 10000) : period_(period_ms) {}
  void setPeriod(uint32_t p) { period_ = p; clear(); }
  void clear() { n_ = 0; head_ = 0; acc_ = 0; }
  // Her çağrıda geçen süre ve anlık değer; periyot dolunca örnek eklenir. Geçersiz değer halkayı temizler.
  bool push(float v, uint32_t dt_ms) {
    if (!isValid(v)) { clear(); return false; }
    acc_ += dt_ms;
    if (n_ == 0) { add(v); acc_ = 0; return true; }
    if (acc_ < period_) return false;
    acc_ -= period_;
    if (acc_ > period_) acc_ = 0;
    add(v);
    return true;
  }
  uint16_t size() const { return n_; }
  bool full() const { return n_ == N; }
  float oldest() const { return n_ ? buf_[(head_ + N - n_) % N] : kNaN; }
  float newest() const { return n_ ? buf_[(head_ + N - 1) % N] : kNaN; }
  float at(uint16_t i) const { return buf_[(head_ + N - n_ + i) % N]; }  // 0 = en eski
  // Pencere boyunca değişim (en yeni − en eski)
  float delta() const { return n_ >= 2 ? newest() - oldest() : kNaN; }
  float spanMinutes() const { return n_ >= 2 ? (float)(n_ - 1) * period_ / 60000.0f : 0.0f; }
  // En küçük kareler eğimi, birim/saat
  float slopePerHour() const {
    if (n_ < 3) return kNaN;
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (uint16_t i = 0; i < n_; ++i) {
      const double x = (double)i * period_ / 3600000.0, y = at(i);
      sx += x; sy += y; sxx += x * x; sxy += x * y;
    }
    const double den = n_ * sxx - sx * sx;
    return den > 0 ? (float)((n_ * sxy - sx * sy) / den) : kNaN;
  }

 private:
  void add(float v) {
    buf_[head_] = v;
    head_ = (uint16_t)((head_ + 1) % N);
    if (n_ < N) ++n_;
  }
  uint32_t period_;
  float buf_[N] = {};
  uint16_t n_ = 0, head_ = 0;
  uint32_t acc_ = 0;
};

}  // namespace cc
