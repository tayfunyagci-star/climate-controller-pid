# CC-SCH-01 devre şeması üreticisi

`python3 gen_s1.py && python3 gen_s2.py && python3 build.py` → `docs/hardware/CC-SCH-01.html` (tek başına açılır) ve `artifact_body.html`.
Semboller IEC tarzıdır; koordinatlar 10 px ızgaradadır. Donanım değişince önce `src/app/pins.h` ve CHANGELOG, sonra bu şema güncellenir.
