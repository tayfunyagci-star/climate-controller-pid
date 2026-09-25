// Rol bazlı ölçüm doğrulama, filtre ve kalite — SENSOR_ARCHITECTURE §4, PID_DESIGN §8.
// Sürücüden bağımsızdır: SensorTask ham örneği + sürücü durumunu verir.
#pragma once
#include "cc_types.h"

namespace cc {

enum class DrvStatus : uint8_t { OK, MISSING, TIMEOUT, CRC_ERROR, BUS_ERROR, NOT_READY };

struct RoleParams {
  float phys_min = -40, phys_max = 85;     // dışında BAD
  float plaus_min = -30, plaus_max = 60;   // dışında UNCERTAIN
  float max_rate_per_s = 2.0f;             // aşarsa UNCERTAIN (medyan-3 eler)
  bool ds18b20_sentinels = false;          // 85.0 / −127 → BAD
  float offset = 0;
  uint32_t interval_ms = 1000;
  uint32_t stale_ms = 10000;
  float tau_s = 10;                        // EMA; 0 = kapalı
  uint32_t stuck_ms = 1800000;             // aynı ham değer + ısıtma aktif → UNCERTAIN
  bool humidity = false;                   // RH özel durumları (≥99.5 % > 30 dk)
  static RoleParams t1(float offset, uint32_t interval_ms, uint32_t stale_ms, float tau_s, uint32_t stuck_ms);
  static RoleParams rh1(float offset, uint32_t interval_ms, uint32_t stale_ms, float tau_s, uint32_t stuck_ms);
  static RoleParams t2(float offset, uint32_t interval_ms, uint32_t stale_ms);
};

struct RoleReading {
  Quality quality = Quality::MISSING;
  float value = kNaN;        // kontrol değeri (EMA, düzeltilmiş); kullanılamazsa NaN
  float safety = kNaN;       // Safety değeri (medyan-3, EMA'sız)
  float raw = kNaN;          // son ham (düzeltilmiş) örnek
  uint32_t age_ms = 0;       // son geçerli örnek yaşı
  bool intermittent = false; // 10 dk hata oranı > 20 %
  bool stuck = false;
  bool condensation = false;
  float error_rate_10m = 0;  // %
  uint32_t errors = 0, crc_errors = 0, timeouts = 0;
};

class RoleFilter {
 public:
  explicit RoleFilter(const RoleParams& p = RoleParams()) : p_(p) {}
  void setParams(const RoleParams& p) { p_ = p; }
  void setDisabled(bool d) { disabled_ = d; }
  void setMissing(bool m) { missing_ = m; }  // begin() başarısız
  // Yeni örnek (dt_ms: son çağrıdan beri geçen süre). Örnek yoksa tick() çağrılır.
  const RoleReading& sample(DrvStatus st, float raw, uint32_t dt_ms, bool heating_active = false);
  const RoleReading& tick(uint32_t dt_ms);
  const RoleReading& reading() const { return r_; }

 private:
  void accountError(DrvStatus st);
  void pushBucket(bool error);
  void finish();

  RoleParams p_;
  bool disabled_ = false, missing_ = false;
  float med_[3] = {kNaN, kNaN, kNaN};
  uint8_t medN_ = 0, medI_ = 0;
  float ema_ = kNaN;
  float last_raw_ = kNaN, last_accepted_ = kNaN;
  bool last_uncertain_ = false;
  uint8_t consecutive_err_ = 0;
  bool bad_ = false;
  Timer age_, since_sample_, stuck_t_, cond_t_;
  bool ever_good_ = false;
  // 10 dk hata oranı: 10 × 1 dk kova
  uint16_t b_total_[10] = {0}, b_err_[10] = {0};
  uint8_t b_i_ = 0;
  Timer b_t_;
  RoleReading r_;
};

}  // namespace cc
