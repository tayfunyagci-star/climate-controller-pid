# ADR-007 — Kalıcılık ayrımı ve flash aşınması

Durum: DESIGN DECISION / önerilen · 24.09.2026

## Karar
- Konfigürasyon: tek şemalı belge, primary/backup/temp, nesil kontrollü atomik yazım; operasyonel değerler 5 s ertelenmiş, azami 60 s flush.
- Sayaçlar (çalışma saati, anahtarlama, boot): ayrı dosya, 15 dk'da bir (değişiklik varsa) + kontrollü reboot/OTA öncesi.
- Olaylar: RAM halkası 200 + kalıcı halka 64 (WARNING+), olay başına hız sınırı.
- Trend halkaları: yalnız RAM.
- Kilitli alarmlar ve onay durumu: alarm geçişinde kalıcı.

## Gerekçe
Farklı yazım frekansları birbirini etkilemez; ≤ 100 yazma/gün; güç kesintisinde ayar kaybı yok, sayaçta ≤ 15 dk kayıp.

## Sonuçlar
Kesinti anındaki son olay ve son 15 dk sayaç artışı kaybolabilir; kayıp aralığı `seq` boşluğuyla görünür.
