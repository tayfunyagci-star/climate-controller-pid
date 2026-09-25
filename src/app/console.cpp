#include "console.h"
#include <Arduino.h>
#include <cstdlib>
#include <cstring>
#include "boot_state.h"
#include "hal_led.h"
#include "hal_outputs.h"
#include "pins.h"
#include "net_manager.h"
#include "tasks.h"

namespace app {

namespace {

char g_line[128];
uint8_t g_len = 0;
uint32_t g_ev_seq = 0;       // yazdırılan son olay
uint32_t g_led_ms = 0, g_status_ms = 0;
bool g_autostatus = false;
BootState g_boot;

const char* onoff(bool b) { return b ? "ON" : "OFF"; }

void printReply(const char* what, const cc::CmdReply& r) {
  Serial.printf("%s -> %s", what, cc::name(r.result));
  if (r.reason != cc::Reason::NONE) Serial.printf(" (%s)", cc::name(r.reason));
  Serial.println();
}

// Olaylar kilit altında yerel tampona alınır, yazdırma kilit dışında yapılır
void drainEvents() {
  cc::Event buf[16];
  uint8_t n = 0;
  uint32_t overwritten = 0;
  if (!coreLock(20)) return;
  const auto& ev = core().events();
  for (uint16_t i = 0; i < ev.size() && n < 16; ++i) {
    const cc::Event& e = ev.at(i);
    if (e.seq > g_ev_seq) buf[n++] = e;
  }
  overwritten = ev.overwritten();
  coreUnlock();
  for (uint8_t i = 0; i < n; ++i) {
    const cc::Event& e = buf[i];
    Serial.printf("[%6lus] #%lu %-8s %-10s %s", (unsigned long)e.up_s, (unsigned long)e.seq, cc::name(e.sev),
                  cc::name(e.src), cc::name(e.code));
    if (!std::isnan(e.val)) Serial.printf(" %.2f", e.val);
    if (e.actor != cc::CmdSource::SYSTEM) Serial.printf(" <%s>", cc::name(e.actor));
    Serial.println();
    g_ev_seq = e.seq;
  }
  (void)overwritten;
}

void printStatus() {
  cc::CoreSnapshot s;
  bool outs[4];
  if (!coreLock(100)) { Serial.println("status: cekirdek kilidi alinamadi"); return; }
  s = core().snapshot();
  for (uint8_t k = 0; k < 4; ++k) outs[k] = core().outputs()[k];
  coreUnlock();
  const TaskStats ts = stats();
  Serial.println("---------------------------------------------------------------");
  Serial.printf("sys=%s ctrl=%s phase=%s vent=%s failsafe=%s reason=%s\n", cc::name(s.sys_state), cc::name(s.controller_state),
                cc::name(s.heating_phase), cc::name(s.ventilation_state), cc::name(s.failsafe_reason), cc::name(s.heating_reason));
  Serial.printf("T1=%.2f (%s, %lus) RH=%.1f  SP=%.1f eff=%.2f src=%s profil=%s mod=%s\n", s.temperature,
                cc::name(s.temperature_quality), (unsigned long)s.sensor_age_s, s.humidity, s.temperature_setpoint,
                s.setpoint_effective, cc::name(s.setpoint_source), cc::name(s.profile_active), cc::name(s.operating_mode));
  Serial.printf("PID out=%.1f P=%.1f I=%.1f  talep=%.1f%% kademe=%u R1=%.0f%% R2=%.0f%%  postcool=%lus\n", s.pid_output, s.pid_p,
                s.pid_i, s.heat_demand, s.power_stage, s.r1_duty, s.r2_duty, (unsigned long)s.post_cool_remaining_s);
  Serial.printf("CIKIS R1=%s R2=%s HF=%s VF=%s  | neden R1=%s R2=%s HF=%s VF=%s\n", onoff(outs[0]), onoff(outs[1]),
                onoff(outs[2]), onoff(outs[3]), cc::name(s.reason[0]), cc::name(s.reason[1]), cc::name(s.reason[2]),
                cc::name(s.reason[3]));
  const net::Status ns = net::status();
  Serial.printf("alarm=%s aktif=%u onaysiz=%u  program=%d saat=%s\n", cc::name(s.alarm_state), s.active_alarm_count,
                s.unacked_alarm_count, s.program_index, s.time_valid ? "gecerli" : "BEKLENIYOR");
  Serial.printf("ag: %s  ssid='%s' ip=%s rssi=%d  AP=%s%s  mDNS=%s.local  OTA=%s  %s\n",
                ns.sta_ok ? "BAGLI" : (ns.configured ? "BAGLANAMADI" : "KURULUM"), ns.ssid, ns.ip, ns.rssi,
                ns.ap_mode ? ns.ap_name : "kapali", ns.ap_mode ? " (192.168.4.1, sifre etikette)" : "",
                net::settings().mdns, ns.ota_ready ? "hazir" : (net::otaPasswordSet() ? "baglanti bekliyor" : "kapali (otapass)"),
                ns.note);
  Serial.printf("gorev yas(ms) saf=%lu out=%lu ctl=%lu sen=%lu | azami(us) %lu/%lu/%lu/%lu | kilit zaman asimi=%lu\n",
                (unsigned long)ts.age_ms[0], (unsigned long)ts.age_ms[1], (unsigned long)ts.age_ms[2], (unsigned long)ts.age_ms[3],
                (unsigned long)ts.max_us[0], (unsigned long)ts.max_us[1], (unsigned long)ts.max_us[2], (unsigned long)ts.max_us[3],
                (unsigned long)ts.lock_timeouts);
  Serial.printf("DHT22 son=%s ok=%lu hata=%lu (timeout %lu, crc %lu) darbe=%lu | yigin bos %lu/%lu/%lu/%lu B | heap=%lu min=%lu\n",
                cc::name(ts.dht_last), (unsigned long)ts.dht_ok, (unsigned long)ts.dht_err, (unsigned long)ts.dht_timeout,
                (unsigned long)ts.dht_crc, (unsigned long)ts.dht_pulses, (unsigned long)ts.stack_free[0],
                (unsigned long)ts.stack_free[1], (unsigned long)ts.stack_free[2], (unsigned long)ts.stack_free[3],
                (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap());
  Serial.printf("boot: reset=%s hatali_boot=%u uptime=%lus\n", g_boot.reset_reason, rtcFaultBoots(), (unsigned long)s.uptime_s);
}

void printHelp() {
  Serial.println(
      "Komutlar:\n"
      "  status | s            durum ozeti\n"
      "  watch                 10 s'de bir durum (ac/kapa)\n"
      "  set <id> <deger>      cekirdek komutu (LOCAL_SERVICE), or. set operating_mode AUTO, set temperature_setpoint 21.5\n"
      "  ack | reset           alarm onayi / guvenlik kilidi sifirlama\n"
      "  service on|off        servis modu;  test <0-3> on|off  servis cikis testi (R1 R2 HF VF)\n"
      "  recovery              restart firtinasi sonrasi operator onayi\n"
      "  wifi <ssid> [parola]  Wi-Fi kaydet ve baglan (parola yazdirilmaz);  wifi clear -> kurulum AP'si\n"
      "  ntp <sunucu>          NTP sunucusu (varsayilan pool.ntp.org; 2. sunucu ag gecidi)\n"
      "  otapass <parola>|clear  OTA parolasi (8-64; parolasiz OTA yok, D-17)\n"
      "  ota                   OTA hazirligi: isitma durur, post-cool biter; sonra pio -t upload\n"
      "  reboot                guvenli yeniden baslatma\n"
#ifdef CC_HIL
      "HIL (yalniz hil imaji):\n"
      "  sim <T> [RH] | sim off   DHT22 yerine benzetim olcumu\n"
      "  sim fail | sim ok        sensor yanit vermiyor benzetimi\n"
      "  hang safety|output|control|sensor   gorevi dondur (watchdog / acil yol / heartbeat testi)\n"
#endif
  );
}

void execute(char* line) {
  char* argv[4] = {nullptr, nullptr, nullptr, nullptr};
  uint8_t argc = 0;
  for (char* tok = strtok(line, " \t"); tok && argc < 4; tok = strtok(nullptr, " \t")) argv[argc++] = tok;
  if (!argc) return;
  const char* c = argv[0];
  using cc::CmdSource;
  const CmdSource src = CmdSource::LOCAL_SERVICE;  // seri port = fiziksel erişim
  auto locked = [&](auto fn) {
    if (!coreLock(200)) { Serial.println("kilit alinamadi"); return; }
    fn();
    coreUnlock();
  };
  if (!strcmp(c, "help") || !strcmp(c, "?")) printHelp();
  else if (!strcmp(c, "status") || !strcmp(c, "s")) printStatus();
  else if (!strcmp(c, "watch")) { g_autostatus = !g_autostatus; Serial.printf("watch %s\n", onoff(g_autostatus)); }
  else if (!strcmp(c, "set") && argc >= 3) {
    cc::CmdReply r;
    locked([&] { r = core().command(argv[1], argv[2], src); });
    printReply(argv[1], r);
  } else if (!strcmp(c, "ack")) { cc::CmdReply r; locked([&] { r = core().alarmAck(src); }); printReply("ack", r); }
  else if (!strcmp(c, "reset")) { cc::CmdReply r; locked([&] { r = core().alarmReset(src); }); printReply("reset", r); }
  else if (!strcmp(c, "recovery")) { cc::CmdReply r; locked([&] { r = core().recoveryAck(src); }); printReply("recovery", r); }
  else if (!strcmp(c, "service") && argc >= 2) {
    cc::CmdReply r;
    const bool on = !strcmp(argv[1], "on");
    locked([&] { r = on ? core().serviceEnter(src) : core().serviceExit(src); });
    printReply("service", r);
  } else if (!strcmp(c, "test") && argc >= 3) {
    const int k = atoi(argv[1]);
    if (k < 0 || k > 3) { Serial.println("cikis 0-3"); return; }
    cc::CmdReply r;
    locked([&] { r = core().serviceTest((uint8_t)k, !strcmp(argv[2], "on"), src); });
    printReply("test", r);
  } else if (!strcmp(c, "wifi") && argc >= 2) {
    const char* err = nullptr;
    const char* field = nullptr;
    if (!strcmp(argv[1], "clear")) Serial.println(net::resetWifi(&err) ? "wifi kimligi silindi; kurulum AP'si aciliyor" : err);
    else {
      const bool ok = net::apply(net::settings(), argv[1], argc >= 3 ? argv[2] : "", &err, &field, nullptr);
      Serial.println(ok ? "kaydedildi, baglaniliyor" : err);
    }
    memset(g_line, 0, sizeof g_line);   // satır tamponunda kalan parolayı sil
  } else if (!strcmp(c, "ntp") && argc >= 2) {
    net::NetSettings n = net::settings();
    strncpy(n.ntp, argv[1], sizeof n.ntp - 1);
    const char* err = nullptr;
    const char* field = nullptr;
    Serial.println(net::apply(n, nullptr, nullptr, &err, &field, nullptr) ? "kaydedildi" : err);
  } else if (!strcmp(c, "otapass") && argc >= 2) {
    const char* err = nullptr;
    const bool ok = net::setOtaPassword(!strcmp(argv[1], "clear") ? "" : argv[1], &err);
    Serial.println(ok ? (strcmp(argv[1], "clear") ? "OTA parolasi kaydedildi (yalniz ozet saklanir)" : "OTA kapatildi") : err);
    memset(g_line, 0, sizeof g_line);
  } else if (!strcmp(c, "ota")) {
    cc::CmdReply r;
    locked([&] { r = core().otaBegin(src); });
    printReply("ota hazirlik (isitma durur, post-cool biter, sonra yukleme kabul edilir)", r);
  }
  else if (!strcmp(c, "reboot")) {
    bool heating = false;
    locked([&] { heating = core().outputs()[cc::R1] || core().outputs()[cc::R2] || core().snapshot().post_cool_remaining_s > 0; });
    if (heating) { Serial.println("isitma/post-cool suruyor: once 'set operating_mode OFF' ve post-cool bitsin"); return; }
    Serial.println("yeniden baslatiliyor");
    Serial.flush();
    ESP.restart();
  }
#ifdef CC_HIL
  else if (!strcmp(c, "sim") && argc >= 2) {
    if (!strcmp(argv[1], "off")) { hilSim(false, 0, 0); Serial.println("sim kapali (DHT22)"); }
    else if (!strcmp(argv[1], "fail")) { hilSensorFail(true); Serial.println("sensor yanit vermiyor"); }
    else if (!strcmp(argv[1], "ok")) { hilSensorFail(false); Serial.println("sensor normal"); }
    else {
      const float t = strtof(argv[1], nullptr), rh = argc >= 3 ? strtof(argv[2], nullptr) : 50.0f;
      hilSim(true, t, rh);
      Serial.printf("sim T=%.2f RH=%.1f\n", t, rh);
    }
  } else if (!strcmp(c, "hang") && argc >= 2) {
    const char* names[4] = {"safety", "output", "control", "sensor"};
    for (uint8_t k = 0; k < 4; ++k) if (!strcmp(argv[1], names[k])) { Serial.printf("gorev donduruluyor: %s\n", names[k]); hilHang(k); return; }
    Serial.println("bilinmeyen gorev");
  }
#endif
  else Serial.println("? (help)");
}

// Durum LED'i (GPIO2): FAILSAFE hızlı, ısıtma sürekli, post-cool/havalandırma yavaş, servis çift çakma,
// boşta kalp atışı; kritik alarm (FAILSAFE dışı) hızlı. Ayrıntı web/MQTT'de.
hal::LedPattern ledFor(cc::CtrlState cs, cc::Severity al) {
  using P = hal::LedPattern;
  if (cs == cc::CtrlState::FAILSAFE || al == cc::Severity::CRITICAL) return P::FAST;
  switch (cs) {
    case cc::CtrlState::HEATING: return P::ON;
    case cc::CtrlState::POST_COOL: case cc::CtrlState::VENTILATING: return P::SLOW;
    case cc::CtrlState::SERVICE: case cc::CtrlState::RECOVERY: return P::DOUBLE;
    case cc::CtrlState::BOOT: case cc::CtrlState::SELF_TEST: return P::SLOW;
    default: return P::HEARTBEAT;
  }
}
hal::LedPattern g_led = hal::LedPattern::SLOW;

void updateLed() {
  if (coreLock(10)) {
    g_led = ledFor(core().snapshot().controller_state, core().snapshot().alarm_state);
    coreUnlock();
  }
  if (g_led == hal::LedPattern::HEARTBEAT && net::status().ap_mode) g_led = hal::LedPattern::SLOW;   // boşta + kurulum AP'si
}

// BOOT butonu (GPIO0): 10 s basılı tutma → Wi-Fi kimliği silinir, kurulum AP'si açılır (SECURITY §2 kurtarma).
// Kısa basış yok sayılır; buton boot'ta strapping olduğundan yalnız çalışma sırasında okunur.
uint32_t g_btn_since = 0;
bool g_btn_done = false;
void serviceButton(uint32_t now) {
  const bool down = gpio_get_level(hw::PIN_BOOT_BTN) == 0;
  if (!down) { g_btn_since = 0; g_btn_done = false; return; }
  if (!g_btn_since) { g_btn_since = now ? now : 1; return; }
  if (!g_btn_done && now - g_btn_since >= 10000) {
    g_btn_done = true;
    const char* err = nullptr;
    Serial.println(net::resetWifi(&err) ? "[BUTON] 10 s: Wi-Fi silindi, kurulum AP'si aciliyor" : err);
  }
}

}  // namespace

void consoleBegin(const BootState& bs) {
  g_boot = bs;
  gpio_set_direction(hw::PIN_BOOT_BTN, GPIO_MODE_INPUT);
  gpio_pullup_en(hw::PIN_BOOT_BTN);
  hal::ledBegin();
  printHelp();
}

void consoleService() {
  while (Serial.available()) {
    const int ch = Serial.read();
    if (ch == '\r' || ch == '\n') {
      if (g_len) {
        g_line[g_len] = 0;
        g_len = 0;
        execute(g_line);
      }
    } else if (ch == 8 || ch == 127) {
      if (g_len) --g_len;
    } else if (g_len < sizeof g_line - 1 && ch >= 32) {
      g_line[g_len++] = (char)ch;
    }
  }
  drainEvents();
  const uint32_t now = millis();
  if (now - g_led_ms >= 250) { g_led_ms = now; updateLed(); }
  hal::ledService(g_led, now);
  serviceButton(now);
  if (g_autostatus && now - g_status_ms >= 10000) { g_status_ms = now; printStatus(); }
}

}  // namespace app
