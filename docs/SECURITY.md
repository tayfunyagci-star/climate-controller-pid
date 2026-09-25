# Güvenlik (Siber)

**ASSUMPTION:** Cihaz güvenilir bir yerel LAN'da, internete doğrudan açılmadan çalışır. Kritik tesis SCADA ürünü değildir; enterprise karmaşıklığı (PKI, SSO, HSM) eklenmez. Buna karşın cihaz **şebeke gücünde ısıtıcı** sürdüğü için uzaktan kötüye kullanım fiziksel sonuç doğurabilir; tasarım bu nedenle uzaktan yetkileri dar tutar.

## 1. Tehdit özeti

| Tehdit | Etki | Karşı önlem |
|---|---|---|
| LAN'daki yetkisiz kullanıcı web'e erişir | Setpoint/mod değişimi | Parola + oturum; misafir yalnız okuma (varsayılan kapalı) |
| CSRF (tarayıcıdaki başka sayfa) | Yetkisiz komut | `SameSite=Strict` + `X-SCADA: 1` + Origin kontrolü |
| Broker'a erişen herhangi istemci | `/set` komutları | Broker kimlik doğrulama + ACL (§3), `remote_config_enabled=false`, servis kanalı kapalı |
| Programs/Kurallar yanlış yapılandırma | Uzun ısıtma | Safety limitleri uzaktan yazılamaz; R1/R2 doğrudan kumandası yok |
| OTA ile kötü firmware | Tam kontrol | OTA parolası önerilir; parolasız durumda web'de kalıcı uyarı (D-17, F2.5). Her yüklemede güvenli duruş (OTA_PREP); metadata/uyumluluk kontrolü; imza FUTURE |
| Sır sızıntısı (yedek, tanı, GET) | Kimlik ele geçirme | Sırlar hiçbir çıkışta yok |
| Fiziksel erişim | Flash okuma | v1 kapsam dışı; FUTURE: flash encryption + secure boot |

## 2. Web

| Konu | Karar |
|---|---|
| Taşıma | HTTP (yerel). **DESIGN DECISION:** v1'de cihaz üzerinde HTTPS yok — sertifika yönetimi, RAM (~40 KB/oturum mbedTLS) ve tarayıcı güven uyarısı maliyeti; UI'da "Bağlantı şifrelenmiyor" bilgi notu. FUTURE: kendinden imzalı sertifika seçeneği veya Suite üzerinden ters vekil |
| Kimlik | Kullanıcı adı + parola; PBKDF2-HMAC-SHA256, 16 B tuz, iterasyon ESP32-S3'te ≤ 300 ms olacak şekilde ölçülerek seçilir; sabit zamanlı karşılaştırma |
| Roller | `viewer` (misafir/okuma), `operator` (operasyonel komut), `admin` (ayar, güvenlik, servis, OTA). v1'de tek admin + isteğe bağlı operatör parolası |
| Oturum | 128 bit rastgele token (donanım RNG), HttpOnly + SameSite=Strict çerez, 8 sa / beni hatırla 14 gün, en çok 4 oturum, parola değişiminde tüm oturumlar düşer |
| Deneme sınırı | 5 hatalı / 5 dk → 5 dk bekleme (IP başına ve global) |
| İlk kurulum | Parola tanımsızsa kalıcı uyarı; **DESIGN DECISION:** ilk kurulum sihirbazı parola belirlemeyi ister (atlanabilir ama uyarı kalır) |
| Kurtarma | Kurtarma sorusu (skill) + fiziksel buton 10 s → yalnız web parolasını temizleyip AP kurulumu (ayarlar ve güvenlik limitleri korunur) |
| Başlıklar | CSP (skill §2), `X-Frame-Options: DENY`, `nosniff`, `Referrer-Policy: same-origin` |
| Riskli uçlar | reboot, factory, OTA, servis, güvenlik limitleri: POST + admin + onay; servis ek PIN |

## 3. MQTT

| Konu | Karar |
|---|---|
| Kimlik | Broker kullanıcı adı/parola (cihaza özel); anonim broker'da `remote_config_enabled` açılamaz (UI engeller + uyarı) |
| ACL önerisi | Cihaz: `mqttsuite/climate/<slug>/#` yaz/oku, `homeassistant/+/<slug>_+/config` yaz, `homeassistant/status` oku. Suite: `mqttsuite/climate/+/+/set` yaz; diğer istemciler yalnız okuma |
| TLS | **OPEN ISSUE:** esp-mqtt TLS destekler (CA sabitleme); v1 yapılandırma seçeneği olarak planlanır, varsayılan kapalı |
| Servis kanalı | Varsayılan kapalı; açıksa `service_token` + `cid` tekrar penceresi + allowlist (alarm_reset, diag_snapshot, reboot) |
| Komut güvenliği | Retained komut uygulanmaz; tüm değerler cihazda doğrulanır; kıstırma yok |
| Veri gizliliği | Payload'larda SSID, parola, token yok; IP yalnız diag'da |

## 4. Sır saklama

- Sırlar (`wifi_password`, `mqtt_password`, `service_token`) NVS'de ayrı ad alanında; web parolası, servis PIN'i ve OTA parolası yalnız özet olarak.
- **OPEN ISSUE:** NVS şifreleme / flash encryption v1'de kapalı (fiziksel erişim tehdit modelinde değil); açılırsa geri dönüşsüz eFuse işlemi olduğu belgelenmelidir.
- GET yanıtlarında sır yerine `*Set` bayrakları; boş alan = koru; açık kaldırma eylemi (skill §8.6).
- Yedek dosyası sır içermez; geri yüklemede sırlar mevcut değerleriyle korunur.

## 5. Debug ve servis kısıtları

- Üretim build'inde debug uçları (`/api/debug/*`, test kancaları: görev dondurma, sensör enjeksiyonu) **derlenmez** (build bayrağı).
- Seri konsol komutları yalnız okuma ve kurtarma (Wi-Fi sıfırlama) sunar.
- Servis modu: admin + PIN + zaman aşımı + her işlem olay günlüğünde (`actor`, sonuç).

## 6. Güncelleme güvenliği

OTA parolası kimlik doğrulamadır; firmware bütünlüğü için SHA-256 metadata kontrolü, rollback ve uyumluluk (hw_rev, config şeması). **FUTURE ENHANCEMENT:** Secure Boot v2 + imzalı OTA.
