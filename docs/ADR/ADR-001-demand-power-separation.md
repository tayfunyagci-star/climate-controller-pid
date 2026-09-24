# ADR-001 — PID çıktısı ile güç çıkışlarının ayrılması

Durum: DESIGN DECISION / önerilen · 24.09.2026

## Problem
PID'in doğrudan röle/SSR sürmesi algoritmayı sürücü tipine, rezistans sayısına ve güvenlik kurallarına bağlar; 3. kademe, farklı güç veya röle→SSR değişimi kontrol kodunu değiştirir.

## Seçenekler
1. PID → doğrudan GPIO (termostat tarzı).
2. PID → `heat_demand %` → PowerManager → Interlock/Safety → OutputManager.
3. PID çıkışını her rezistans için ayrı PID'lere bölmek.

## Karar
Seçenek 2. PID yalnız `pid_output`; koşullandırma sonrası `heat_demand`; PowerManager saf fonksiyon; çıkışların tek sahibi OutputManager.

## Gerekçe
Sorumluluk ayrımı, host'ta native test, sürücü/kademe değişiminde PID'in etkilenmemesi, anti-windup için "uygulanan talep" geri beslemesinin tek noktadan verilmesi.

## Sonuçlar
Uygulanan talep ≠ PID çıktısı olabilir; bu fark raporlanır (`pid_output` vs `heat_demand`, `r*_duty`) ve integratöre geri hesaplamayla bildirilir. Ek bir katman ve arayüz sözleşmesi bakımı gerekir.

Referans: [CONTROL_ARCHITECTURE](../CONTROL_ARCHITECTURE.md), [OUTPUT_AND_INTERLOCKS](../OUTPUT_AND_INTERLOCKS.md).
