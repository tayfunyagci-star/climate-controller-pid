# ADR-003 — Requested / Effective / Reason modeli

Durum: DESIGN DECISION / önerilen · 24.09.2026

## Problem
Interlock ve safety, kullanıcı isteğinden farklı çıkış durumu üretir (ör. HF OFF isteği, fan ON). MQTT Studio komutu 8 s içinde state'te istenen değeri görmezse "doğrulanmadı" işaretler; etkin durumu switch state'i olarak yayınlamak her interlock'u "komut başarısız" gibi gösterir, istek durumunu yayınlamamak ise kullanıcıya ne istediğini göstermez.

## Seçenekler
1. Switch state = etkin durum.
2. Switch state = istek; etkin durum ayrı `binary_sensor`; neden ayrı `sensor`.
3. Tek alanda birleşik metin ("OFF→ON").

## Karar
Seçenek 2. Genel kalıp: `<cikis>_manual` (switch, istek) · `<cikis>_active` (binary, etkin) · `<cikis>_reason` (sensor, kapalı sözlük). Setpoint/profil/talep için aynı ayrım (`temperature_setpoint`/`setpoint_effective`/`setpoint_source`). Komut sonucu `B/ack`'te `ACCEPTED` / `OVERRIDDEN` / `REJECTED_*`.

## Gerekçe
Suite doğrulaması isteğin kabulünü doğru ölçer; etkin durum ve neden SCADA UI'da açıkça gösterilir; düz JSON ve mevcut `value_template` kısıtıyla uyumlu.

## Sonuçlar
Entity sayısı artar. UI'nın istek ≠ etkin durumunu her ekranda göstermesi zorunlu. Reason sözlüğü sürümlü ve kapalıdır.
