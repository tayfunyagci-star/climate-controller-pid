exec(open('gen_head.py').read())
# =================================================================== SAYFA 1 — SELV mantık kartı
a = S()
W1, H1 = 1220, 1180
a.rect(10, 10, W1 - 20, H1 - 20, "frame")
a.text(30, 40, "SAYFA 1 · GÜÇ ve MANTIK KARTI (SELV: +5 V / +3.3 V)", "sheet")

# --- Güç: U1 → D1 → +5V
a.rect(60, 130, 130, 120, "box"); a.text(125, 118, "U1", "ref", "middle")
a.text(125, 180, "AC/DC 5 V", "pinl", "middle"); a.text(125, 196, "IRM-10-5", "lbl", "middle"); a.text(125, 212, "10 W · 2 A", "lbl", "middle")
a.line(30, 160, 60, 160, "L"); a.line(30, 220, 60, 220, "N"); a.text(34, 152, "L", "lbl"); a.text(34, 212, "N", "lbl")
a.text(30, 272, "← Sayfa 2: F1 sonrası", "lbl")
a.add('<circle class="pin" cx="190" cy="160" r="3"/>'); a.text(184, 164, "+V", "pinl", "end")
a.add('<circle class="pin" cx="190" cy="220" r="3"/>'); a.text(184, 224, "−V", "pinl", "end")
a.diode_h(190, 160, "D1", "SS34")
a.line(250, 160, 300, 160); a.dot(300, 160); a.line(300, 160, 380, 160); a.dot(340, 160)
a.flag(380, 160, "+5V")
a.cap_v(300, 160, "C1", "470 µF 16 V", pol=True, side="l"); a.gnd(300, 210)
a.cap_v(340, 160, "C2", "100 nF", side="r"); a.gnd(340, 210)
a.line(190, 220, 230, 220); a.gnd(230, 220)
a.text(60, 300, "D1: USB bağlıyken kartın 5V hattından", "note")
a.text(60, 314, "PSU'ya geri beslemeyi engeller.", "note")

# --- U2 ESP32 DevKit V1
ux, uy, uw, uh = 470, 130, 190, 680
a.rect(ux, uy, uw, uh, "box")
a.text(ux + uw / 2, uy - 12, "U2 ESP32 DevKit V1 (ESP32-WROOM-32, 4 MB)", "ref", "middle")
lp = {"VIN": 190, "GND": 250, "3V3": 330}
Y27 = 790
lp["GPIO27"] = Y27
for n, y in lp.items():
    a.add(f'<circle class="pin" cx="{ux}" cy="{y}" r="3"/>'); a.text(ux + 8, y + 4, n, "pinl")
Y4, Y25, Y26, Y32, Y33, Y13 = 190, 390, 540, 650, 710, 770
for n, y in (("GPIO4", Y4), ("GPIO25", Y25), ("GPIO26", Y26), ("GPIO32", Y32), ("GPIO33", Y33), ("GPIO13", Y13)):
    a.add(f'<circle class="pin" cx="{ux+uw}" cy="{y}" r="3"/>'); a.text(ux + uw - 8, y + 4, n, "pinl", "end")
a.text(ux + 14, 430, "Kart üstünde:", "lbl")
a.text(ux + 14, 446, "BOOT buton → GPIO0", "lbl"); a.text(ux + 14, 462, "Mavi LED → GPIO2", "lbl")
a.text(ux + 14, 478, "USB-UART CP2102/CH340", "lbl"); a.text(ux + 14, 494, "→ GPIO1/3", "lbl")
a.text(ux + 14, 510, "3.3 V LDO AMS1117 (kart)", "lbl")
a.text(ux + 14, 580, "Kullanılmaz: strap 0/2/5/12/15,", "lbl"); a.text(ux + 14, 596, "flash 6–11, 14, 34–39 (giriş)", "lbl")
a.poly([(ux, 190), (420, 190), (420, 160)]); a.flag(420, 160, "+5V")
a.line(ux, 250, 440, 250); a.gnd(440, 250)
a.poly([(ux, 330), (420, 330), (420, 310)]); a.flag(420, 310, "+3V3")
a.text(300, 360, "3V3: DHT22 + röle modülü", "note"); a.text(300, 374, "opto tarafı (< 30 mA)", "note")

# --- DHT22 (GPIO4)
X = ux + uw
a.line(X, Y4, 730, Y4); a.dot(730, Y4)
a.res_v(730, 100, "R1", "4.7 kΩ", side="r"); a.flag(730, 100, "+3V3")
a.res_h(730, Y4, "R2", "100 Ω", above=False)
a.line(790, Y4, 880, Y4)
a.conn(880, 155, ["1 VCC", "2 DATA", "3 NC", "4 GND"], "J1", w=70)
a.poly([(880, 170), (860, 170), (860, 145)]); a.flag(860, 145, "+3V3")
a.poly([(880, 230), (860, 230), (860, 245)]); a.gnd(860, 245)
for y in (170, 190, 230):
    a.line(950, y, 1040, y, "cable")
a.text(995, 162, "kablo ≤ 5 m", "lbl", "middle")
a.rect(1040, 150, 80, 100, "box"); a.text(1080, 142, "B1 DHT22", "ref", "middle")
for y, n in ((170, "VCC"), (190, "DATA"), (230, "GND")):
    a.add(f'<circle class="pin" cx="1040" cy="{y}" r="3"/>'); a.text(1048, y + 4, n, "pinl")
a.dot(1025, 170); a.poly([(1025, 170), (1025, 125), (1170, 125), (1170, 150)])
a.cap_v(1170, 150, "C5", "100 nF", side="l")
a.poly([(1170, 200), (1170, 270), (1025, 270), (1025, 230)]); a.dot(1025, 230)
a.text(1040, 112, "C5 sensör pinlerinde", "note")

# --- SSR sürücüleri (GPIO25, GPIO26)
def ssr_driver(y, n, rb, rpd, q, j):
    a.line(X, y, 740, y); a.res_h(740, y, rb, "1 kΩ")
    a.line(800, y, 818, y); a.dot(818, y)
    a.res_v(818, y, rpd, "10 kΩ", side="l"); a.gnd(818, y + 60)
    a.line(818, y, 838, y)
    a.npn(860, y, q, "BC337-40")
    a.gnd(872, y + 30)
    a.poly([(872, y - 30), (872, y - 50), (960, y - 50)])
    a.conn(960, y - 85, ["+ (A1)", "− (A2)"], j, w=84)
    a.poly([(960, y - 70), (930, y - 70), (930, y - 95)]); a.flag(930, y - 95, "+5V")
    a.text(1060, y - 64, f"→ SSR{n} giriş", "lbl"); a.text(1060, y - 50, "(Sayfa 2)", "lbl")
ssr_driver(Y25, 1, "R3", "R4", "Q1", "J2 SSR1")
ssr_driver(Y26, 2, "R5", "R6", "Q2", "J3 SSR2")

# --- Röle modülü (GPIO32, GPIO33)
a.line(X, Y32, 760, Y32); a.dot(760, Y32)
a.res_v(760, Y32 - 60, "R7", "10 kΩ", side="l"); a.flag(760, Y32 - 60, "+3V3")
a.line(760, Y32, 940, Y32)
a.line(X, Y33, 860, Y33); a.dot(860, Y33)
a.res_v(860, Y33, "R8", "10 kΩ", side="l"); a.flag(860, Y33 + 60, "+3V3", up=False)
a.line(860, Y33, 940, Y33)
mx, my = 940, 610
a.rect(mx, my, 160, 160, "box")
for n, y in (("VCC", 630), ("IN1", Y32), ("IN2", Y33), ("GND", 750)):
    a.add(f'<circle class="pin" cx="{mx}" cy="{y}" r="3"/>'); a.text(mx + 8, y + 4, n, "pinl")
a.poly([(mx, 630), (905, 630), (905, 612)]); a.flag(905, 612, "+3V3")
a.poly([(mx, 750), (915, 750), (915, 760)]); a.gnd(915, 760)
a.add(f'<circle class="pin" cx="{mx+160}" cy="630" r="3"/>'); a.text(mx + 152, 634, "JD-VCC", "pinl", "end")
a.poly([(mx + 160, 630), (1140, 630), (1140, 612)]); a.flag(1140, 612, "+5V")
a.text(mx + 152, 670, "K1 COM/NO", "pinl", "end"); a.text(mx + 152, 690, "K2 COM/NO", "pinl", "end")
a.text(mx + 152, 716, "opto izoleli", "lbl", "end")
a.text(mx + 168, 684, "→ Sayfa 2", "lbl")
a.text(mx + 80, 790, "U3 2 kanal röle modülü 5 V", "ref", "middle")
a.text(mx + 80, 808, "JD-VCC jumper'ı SÖKÜLÜ", "warnt", "middle")

# GPIO13 ARM yedek
a.line(X, Y13, 700, Y13); a.line(694, Y13 - 6, 706, Y13 + 6, "c"); a.line(694, Y13 + 6, 706, Y13 - 6, "c")
a.text(714, Y13 + 4, "NC (ARM yedek)", "lbl")

# modül dekuplajı (U3 VCC pinine yakın)
for cx, ref, val, side in ((700, "C3", "10 µF", "r"), (790, "C4", "100 nF", "r")):
    a.flag(cx, 820, "+3V3"); a.cap_v(cx, 820, ref, val, side=side); a.gnd(cx, 870)
a.text(850, 850, "C3/C4: U3 VCC pinine yakın", "note")

# --- WS2812B durum şeridi (GPIO27, F2.4): U4 seviye çevirici → J4 → kablo → R13 → LS1
YD = 1050
a.poly([(ux, Y27), (450, Y27), (450, YD), (480, YD)])
# U4 74AHCT1G125 (SOT-23-5): 1 OE, 2 A, 3 GND, 4 Y, 5 VCC
a.rect(480, 1010, 120, 80, "box"); a.add('<polygon class="c" points="525,1035 525,1065 555,1050"/>')
for x, y in ((480, YD), (600, YD), (540, 1010), (510, 1090), (570, 1090)):
    a.add(f'<circle class="pin" cx="{x}" cy="{y}" r="3"/>')
a.text(488, YD + 4, "2 A", "pinl"); a.text(592, YD + 4, "4 Y", "pinl", "end"); a.text(540, 1026, "5 VCC", "pinl", "middle")
a.text(510, 1082, "3 GND", "pinl", "middle"); a.text(570, 1082, "1 OE", "pinl", "middle"); a.line(570, 1071, 584, 1071, "ovl")
a.text(540, 1138, "U4 74AHCT1G125", "ref", "middle")
a.line(510, 1090, 510, 1100); a.gnd(510, 1100); a.line(570, 1090, 570, 1100); a.gnd(570, 1100)
a.line(540, 1010, 540, 940); a.flag(540, 940, "+5V"); a.dot(540, 960)
a.line(540, 960, 680, 960); a.dot(630, 960)
a.cap_v(630, 960, "C10", "100 nF", side="l"); a.gnd(630, 1010)
a.line(600, YD, 700, YD)
a.conn(700, YD - 35, ["1 +5V", "2 DIN", "3 GND"], "J4", w=70)
a.poly([(680, 960), (680, 1030), (700, 1030)])
a.poly([(700, 1070), (680, 1070), (680, 1080)]); a.gnd(680, 1080)
for y in (1030, YD, 1070):
    a.line(770, y, 820, y, "cable")
# şerit ucu: R13, C8, C9 şeridin giriş pedlerinde
a.poly([(820, 1030), (830, 1030), (830, 1000), (940, 1000)]); a.dot(920, 1000)
a.line(820, YD, 840, YD); a.res_h(840, YD, "R13", "330 Ω"); a.line(900, YD, 940, YD)
a.poly([(820, 1070), (830, 1070), (830, 1100), (940, 1100)]); a.dot(920, 1100)
a.rect(940, 980, 80, 140, "box"); a.text(980, 950, "LS1", "ref", "middle"); a.text(980, 966, "WS2812B × 6", "lbl", "middle")
for y, n in ((1000, "+5V"), (YD, "DIN"), (1100, "GND")):
    a.add(f'<circle class="pin" cx="940" cy="{y}" r="3"/>'); a.text(948, y + 4, n, "pinl")
for i in range(6):
    y = 990 + 22 * i
    a.rect(990, y, 10, 10, "pin"); a.text(1006, y + 9, str(i + 1), "lbl")
    if i < 5: a.line(995, y + 10, 995, y + 22)
a.poly([(920, 1000), (920, 930), (1140, 930)]); a.dot(1060, 930)
a.poly([(920, 1100), (920, 1140), (1140, 1140)]); a.dot(1060, 1140)
for x, ref, val, pol in ((1060, "C9", "100 nF", False), (1140, "C8", "470 µF", True)):
    a.line(x, 930, x, 1010); a.cap_v(x, 1010, ref, val, pol=pol, side="r"); a.line(x, 1060, x, 1140)
a.text(920, 916, "R13, C8, C9 şerit girişinde", "note")
a.text(40, 940, "WS2812B durum şeridi (F2.4)", "ref")
for i, t in enumerate(("U4 önerilir: 3.3 V veri seviyesi WS2812B'nin",
                       "VIH = 0.7 · VDD sınırında. U4 takılmazsa A–Y köprülenir.",
                       "74AHCT125N (DIP-14): 1OE = 1, 1A = 2, 1Y = 3, GND = 7,",
                       "VCC = 14; boş kapılarda A → GND, OE → +5V.",
                       "",
                       "LED1 durum · LED2 ağ · LED3 MQTT",
                       "LED4 mDNS · LED5 ısıtma · LED6 fan",
                       "LS1 ≤ 360 mA (beyaz %100), %20 parlaklıkta ≈ 72 mA")):
    if t: a.text(40, 964 + 14 * i, t, "note")

# HIL-1 ek
a.rect(40, 590, 390, 230, "hilbox")
a.text(56, 614, "HIL-1 (şebekesiz ilk test): J2/J3 yerine", "ref")
a.flag(110, 650, "+5V"); a.res_v(110, 650, "R9/R10", "1 kΩ", side="r")
a.line(110, 710, 110, 720)
a.add('<polygon class="fillc" points="101,720 119,720 110,736"/>'); a.line(101, 736, 119, 736, "c")
a.line(118, 724, 128, 716, "c"); a.line(122, 730, 132, 722, "c")
a.line(110, 736, 110, 760); a.text(140, 734, "LED1/LED2", "lbl")
a.text(110, 778, "→ Q1/Q2 kollektörü", "lbl", "middle")
a.text(220, 650, "SSR çıkışına şebeke", "note"); a.text(220, 664, "bağlanmaz; röle kontak", "note"); a.text(220, 678, "tarafı boş.", "note")
a.text(220, 706, "LED yanık = R komutlu", "note"); a.text(220, 720, "(fiziksel geri bildirim", "note"); a.text(220, 734, "değildir).", "note")

sheet1 = a.svg(W1, H1, "Sayfa 1: ESP32 DevKit V1 kartı, 5 V besleme, DHT22 sensörü, iki SSR sürücü transistörü, 2 kanallı röle modülü ve WS2812B durum şeridi bağlantıları", "s1")
open('sheet1.svg', 'w').write(sheet1)
