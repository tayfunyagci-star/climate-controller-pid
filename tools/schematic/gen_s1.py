exec(open('gen_head.py').read())
# =================================================================== SAYFA 1 — SELV mantık kartı
a = S()
W1, H1 = 1220, 910
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
for n, y in lp.items():
    a.add(f'<circle class="pin" cx="{ux}" cy="{y}" r="3"/>'); a.text(ux + 8, y + 4, n, "pinl")
Y4, Y5, Y6, Y7, Y15, Y17 = 190, 390, 540, 650, 710, 770
for n, y in (("GPIO4", Y4), ("GPIO25", Y5), ("GPIO26", Y6), ("GPIO32", Y7), ("GPIO33", Y15), ("GPIO13", Y17)):
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

# --- SSR sürücüleri (GPIO5, GPIO6)
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
ssr_driver(Y5, 1, "R3", "R4", "Q1", "J2 SSR1")
ssr_driver(Y6, 2, "R5", "R6", "Q2", "J3 SSR2")

# --- Röle modülü (GPIO7, GPIO15)
a.line(X, Y7, 760, Y7); a.dot(760, Y7)
a.res_v(760, Y7 - 60, "R7", "10 kΩ", side="l"); a.flag(760, Y7 - 60, "+3V3")
a.line(760, Y7, 940, Y7)
a.line(X, Y15, 860, Y15); a.dot(860, Y15)
a.res_v(860, Y15, "R8", "10 kΩ", side="l"); a.flag(860, Y15 + 60, "+3V3", up=False)
a.line(860, Y15, 940, Y15)
mx, my = 940, 610
a.rect(mx, my, 160, 160, "box")
for n, y in (("VCC", 630), ("IN1", Y7), ("IN2", Y15), ("GND", 750)):
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

# GPIO17 ARM yedek
a.line(X, Y17, 700, Y17); a.line(694, Y17 - 6, 706, Y17 + 6, "c"); a.line(694, Y17 + 6, 706, Y17 - 6, "c")
a.text(714, Y17 + 4, "NC (ARM yedek)", "lbl")

# modül dekuplajı (U3 VCC pinine yakın)
for cx, ref, val, side in ((700, "C3", "10 µF", "r"), (790, "C4", "100 nF", "r")):
    a.flag(cx, 820, "+3V3"); a.cap_v(cx, 820, ref, val, side=side); a.gnd(cx, 870)
a.text(850, 850, "C3/C4: U3 VCC pinine yakın", "note")

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

sheet1 = a.svg(W1, H1, "Sayfa 1: ESP32 DevKit V1 kartı, 5 V besleme, DHT22 sensörü, iki SSR sürücü transistörü ve 2 kanallı röle modülü bağlantıları", "s1")
open('sheet1.svg', 'w').write(sheet1)
