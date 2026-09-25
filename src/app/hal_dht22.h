// DHT22/AM2302 sürücüsü — RMT RX ile darbe yakalama, çözüm lib/core/cc_dht (native testli).
// Yalnız SensorTask çağırır. read() ≈ 5–20 ms sürer; bekleme RTOS uykusudur (meşgul döngü/delay() yok).
#pragma once
#include "core_api.h"

namespace hal {

class Dht22 {
 public:
  bool begin();                 // RMT kanalı + pin (open-drain, pull-up)
  cc::DhtFrame read();          // başlat → yakala → çöz
  uint32_t lastPulseCount() const { return pulses_; }
 private:
  bool ok_ = false;
  uint32_t pulses_ = 0;
};

}  // namespace hal
