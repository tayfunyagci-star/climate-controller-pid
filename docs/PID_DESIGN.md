# PID / PI Tasarımı

Kontrol değişkeni T1 (°C), çıkış `pid_output` (0–100 %). Kulübe yavaş, gecikmeli, **tek yönlü** (yalnız ısıtma) bir süreçtir. Soğutma yoktur; çıkış negatif olamaz.

## 1. Algoritma seçimi

| `pid_mode` | Kullanım | Varsayılan |
|---|---|---|
| `PI` | Kulübe ısıtması için önerilen; türev gürültüye duyarlı ve yavaş süreçte az fayda sağlar | **Evet** |
| `PID` | T1 sensörü düşük gürültülü ve süreç ölü zamanı belirginse | |
| `P` | Devreye alma / tanı | |
| `ONOFF` | Yedek: histerezisli iki nokta (`onoff_hysteresis` 0.5 °C); talep 0 veya `onoff_demand` (100 %) | |

**DESIGN DECISION:** Paralel (ISA "ideal" yerine bağımsız kazançlı) form, türev **ölçüm** üzerinden, oransal terimde setpoint ağırlığı `b`.

## 2. Denklemler (ayrık, periyot `Ts`)

```text
e      = SP_eff − PV_f                       (PV_f: filtrelenmiş T1)
e_db   = deadband(e, db)                     (§4)
P      = Kp · (b·SP_eff − PV_f)              b ∈ [0,1], varsayılan 1.0
I[k]   = I[k−1] + Ki · e_db · Ts             (koşullu entegrasyon, §3)
D[k]   = α·D[k−1] − (1−α)·Kd·(PV_f[k] − PV_f[k−1]) / Ts
           α = Tf / (Tf + Ts), Tf = Kd/(Kp·N), N = 8
u_raw  = P + I + D
pid_output = clamp(u_raw, out_min, out_max)  (varsayılan 0…100 %)
```

Birimler (UI ve MQTT'de):

| Parametre | Birim | Başlangıç | Sınır | Adım |
|---|---|---|---|---|
| `pid_kp` | %/°C | 20.0 | 0.5 – 200 | 0.5 |
| `pid_ki` | %/(°C·dk) | 1.0 (Ti ≈ 20 dk) | 0 – 20 | 0.05 |
| `pid_kd` | %·dk/°C | 0.0 | 0 – 60 | 0.5 |
| `pid_setpoint_weight` (b) | — | 1.0 | 0 – 1 | 0.1 |
| `pid_deadband` | °C | 0.1 | 0 – 1.0 | 0.05 |
| `control_interval_s` (Ts) | s | 2 | 1 – 30 | 1 |

**ASSUMPTION:** Başlangıç katsayıları tipik 10–20 m³ kulübe ve 2 × 1–2 kW rezistans içindir; saha adımı cevabıyla ayarlanmalıdır (§10). Nihai değer değildir.

## 3. Anti-windup ve doyum yönetimi

**DESIGN DECISION:** İki mekanizma birlikte:

1. **Koşullu entegrasyon (clamping):** `u_raw` üst sınırda ve `e > 0` ise ya da alt sınırda ve `e < 0` ise integratör güncellenmez.
2. **Geri hesaplama (back-calculation):** Aşağı akıştaki gerçek uygulanan talep (`heat_demand_applied`, interlock/safety sonrası) PID çıktısından farklıysa integratör `Kt·(applied − u_raw)·Ts` ile düzeltilir; `Kt = Ki/Kp` (varsayılan).

Aşağı akış kısıtları integratörü **bilgilendirir**:

| Kısıt | PID'e bildirilen | Davranış |
|---|---|---|
| Safety kilidi / FAILSAFE | `tracking = true` | İntegratör dondurulur, çıkış 0 izlenir; dönüşte bumpless |
| POST_COOL / fan prestart gecikmesi | `applied = 0` (geçici) | Koşullu entegrasyon durdurur (5 dk'dan kısa kesintilerde integratör korunur) |
| Güç sınırı (`max_heat_demand`) | `out_max` düşer | Clamping |
| MANUAL | `tracking = true` | Integratör manuel talebi izler (§6) |
| Vent koordinasyonu (`VENT_WINS`) | `out_max = vent_heat_cap` | Clamping |

`anti_windup_active` (binary, tanı) ve `pid_saturation` (`NONE`/`HIGH`/`LOW`) raporlanır.

## 4. Deadband

`|e| < db` ise `e_db = 0` (integratör sabit, P terimi etkilenmez). Kulübede termik gürültü ve sensör kuantizasyonu (SHT4x 0.01 °C, DS18B20 0.0625 °C) kaynaklı integratör kaymasını önler. Deadband P'ye uygulanmaz; aksi hâlde setpoint çevresinde basamak oluşur.

## 5. Çıkış koşullandırma (PID sonrası, PowerManager öncesi)

| Mekanizma | Başlangıç | Kural |
|---|---|---|
| Çıkış sınırı | 0–100 % | `max_heat_demand` ile üst sınır düşürülebilir (güç sözleşmesi, jeneratör) |
| Minimum talep | 5 % | `pid_output < min_heat_demand` ise talep 0 (kısa darbe yok); histerezis: 0'dan çıkmak için ≥ `min_heat_demand + 2 %` |
| Çıkış eğim sınırı (slew) | 10 %/dk artış, sınırsız azalış | Ani tam güç yükselişini ve kademe sıçramasını yumuşatır; güvenlik yönünde (azalış) sınır yok |
| MANUAL talep | 0–100 % | PID atlanır, koşullandırma yine uygulanır |

Sonuç `heat_demand` olarak yayınlanır; `pid_output` ham PID çıktısıdır. İkisi arasındaki fark UI'da "talep sınırlandı" nedeniyle gösterilir.

## 6. Bumpless transfer

| Olay | Yöntem |
|---|---|
| MANUAL → AUTO | `I = heat_demand_current − P − D` (sınırlara kıstırılmış) |
| AUTO → MANUAL | `manual_heat_demand = heat_demand_current` (kullanıcı değer göndermediyse) |
| Kp/Ki/Kd değişimi | `I` yeniden ölçeklenir: çıkış anında sabit kalacak şekilde `I_new = u_prev − P_new − D_new` |
| `pid_mode` değişimi | Aynı ilke; ONOFF → PI geçişinde `I = son ONOFF talebi` |
| Setpoint değişimi | Setpoint rampası (§7) + setpoint ağırlığı b < 1 ile P sıçraması sınırlanabilir |
| FAILSAFE/kilit dönüşü | `I` dondurulmuş değerden devam etmez; `I = 0` ve slew sınırıyla yükselir (**DESIGN DECISION:** arıza sonrası temkinli başlangıç) |

## 7. Setpoint rampası

- `setpoint_ramp_c_per_min` varsayılan 0.2 °C/dk (0 = kapalı). Yalnız **yükselen** setpoint'e uygulanır; düşüş anında geçerlidir (enerji ve güvenlik).
- BOOST ve antifreeze girişinde rampa uygulanmaz (**DESIGN DECISION**: kullanıcı/koruma anında hedefi istiyor).
- Rampa başlangıcı mevcut T1 değil, önceki etkin setpoint'tir; T1 ≫ SP ise rampa beklemeden etkin SP'ye geçilir.

## 8. Sensör yumuşatma ve türev filtresi

| Katman | Başlangıç | Not |
|---|---|---|
| Ham örnek doğrulama | aralık + hız kontrolü | [SENSOR_ARCHITECTURE §4](SENSOR_ARCHITECTURE.md) |
| Medyan-3 | açık | Tekil sıçramaları atar |
| EMA (`sensor_filter_tau_s`) | 10 s | PV_f; kontrol bu değeri kullanır |
| Türev filtresi | N = 8 | Yalnız PID modunda |

Filtre gecikmesi kontrol döngüsüne eklenir; `sensor_filter_tau_s ≤ control ölü zamanı / 3` önerilir.

## 9. Doğrulama kriterleri (implementasyon aşaması için)

| Test | Beklenen |
|---|---|
| Doyumda uzun süre (100 %, 60 dk) sonra SP'ye ulaşma | Aşım ≤ 0.5 °C (anti-windup) |
| MANUAL 40 % → AUTO | Talep sıçraması ≤ 2 % |
| Kp 20 → 30 anlık değişim | Çıkış sıçraması ≤ 1 % |
| Deadband içinde 1 sa gürültülü PV | İntegratör kayması ≤ 1 % |
| SP 18 → 22 | Etkin SP 0.2 °C/dk yükselir |
| Talep 3 % | Çıkış 0; 7 %'ye çıkınca etkin |
| Safety kilidi 10 dk sonra açılır | İntegratör 0'dan, slew ≤ 10 %/dk |
| Ts değişimi 2 → 5 s | Ki birimi dakika tabanlı olduğundan davranış korunur |

## 10. Saha ayarı prosedürü (öneri)

1. `pid_mode=P`, rezistanslar ve fan doğrulandıktan sonra MANUAL 50 % adım cevabı kaydedilir (trend ekranı + historian).
2. Ölü zaman (L), zaman sabiti (T) ve kazanç (K, °C/%) grafikte okunur.
3. Başlangıç PI (SIMC benzeri): `Kp = T / (K·(L + τc))`, `Ti = min(T, 4(L + τc))`, `τc = L`.
4. AUTO'da SP basamağı ile doğrulanır; aşım > 0.5 °C ise Kp %20 düşürülür.
5. **FUTURE ENHANCEMENT:** Röle geri beslemeli otomatik ayar (Åström–Hägglund) — servis modunda, güvenlik sınırları altında.

## 11. PID durum raporu

| Alan | Tür | Anlam |
|---|---|---|
| `pid_output` | % | Ham PID çıktısı |
| `pid_error` | °C | `SP_eff − PV_f` |
| `pid_p`, `pid_i`, `pid_d` | % | Terim katkıları (tanı, `diag` veya PID sayfası) |
| `pid_saturation` | enum | `NONE`, `HIGH`, `LOW` |
| `anti_windup_active` | bool | Entegrasyon durduruldu mu |
| `pid_tracking` | bool | MANUAL/FAILSAFE izleme |
