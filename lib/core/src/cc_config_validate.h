// Konfigürasyon doğrulayıcı — CONFIGURATION_MODEL §4 (V1–V17).
// Saf fonksiyon: web, MQTT ve boot yüklemesi aynı yolu kullanır. Ret ≠ kıstırma.
#pragma once
#include "cc_config.h"

namespace cc {

enum class ValCode : uint8_t {
  OK,
  INVALID_FORMAT,         // ayrıştırılamadı ("22,5", "on" …)
  INVALID_RANGE,          // alan aralığı dışı (V16 dahil)
  INVALID_STEP,           // adım dışı
  UNKNOWN_FIELD,
  INVALID_RELATION,
  INVALID_SAFETY_MARGIN,
  INVALID_DRIVER_TIMING,
  REQUIRES_T2,
  INVALID_PID,
  WARN_PROFILE_ORDER,     // uyarı
  WARN_PID_KD,            // uyarı
};
const char* name(ValCode);

struct ValIssue {
  uint8_t rule;       // 0 = alan aralığı/biçim, 1..17 = V-kuralı
  ValCode code;
  const char* field;  // ilgili alan anahtarı
};

struct ValidationResult {
  static constexpr int kMax = 24;
  ValIssue errors[kMax];
  ValIssue warnings[8];
  uint8_t nErrors = 0, nWarnings = 0;
  bool ok() const { return nErrors == 0; }
  bool hasRule(uint8_t rule) const;
  bool hasErrorOn(const char* field) const;
  void addError(uint8_t rule, ValCode c, const char* f);
  void addWarning(uint8_t rule, ValCode c, const char* f);
};

// Adayın tamamını doğrular (alan aralıkları + ilişkiler).
ValidationResult validate(const Config& candidate);

// Tek alan yazımı (MQTT /set, web tek alan). `out` yalnız ACCEPTED'da geçerli adaydır;
// `current` hiçbir durumda değişmez. src=MQTT için uzak yazım politikası uygulanır.
struct SetResult {
  CmdResult result;
  ValCode code;
  uint8_t rule;
};
SetResult setField(const Config& current, const char* key, const char* payload, CmdSource src,
                   Config& out);

// Sayı ayrıştırma: yalnız ondalık nokta; boşluk, virgül, üs, sonek reddedilir.
bool parseNumberStrict(const char* s, float& out);

}  // namespace cc
