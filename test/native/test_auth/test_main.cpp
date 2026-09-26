// Web erişim mantığı: oturum tablosu, deneme sınırı, sabit zamanlı karşılaştırma, hex (SECURITY §2).
#include <unity.h>
#include <cstring>
#include "cc_auth.h"

using namespace cc;

void setUp() {}
void tearDown() {}

static void tok(uint8_t t[kTokenLen], uint8_t v) { memset(t, v, kTokenLen); }

void test_sessions_expire_and_evict_oldest() {
  SessionTable st;
  uint8_t a[kTokenLen], b[kTokenLen];
  tok(a, 1);
  tok(b, 2);
  st.add(a, 100, 60);
  TEST_ASSERT_TRUE(st.valid(a, 159));
  TEST_ASSERT_FALSE(st.valid(a, 160));          // süre sonu
  TEST_ASSERT_FALSE(st.valid(b, 120));
  for (uint8_t i = 0; i < kMaxSessions; ++i) { uint8_t t[kTokenLen]; tok(t, 10 + i); st.add(t, 200 + i, 1000); }
  uint8_t t0[kTokenLen];
  tok(t0, 10);
  TEST_ASSERT_TRUE(st.valid(t0, 300));
  uint8_t t5[kTokenLen];
  tok(t5, 99);
  st.add(t5, 400, 1000);                         // dolu → en eski (10) düşer
  TEST_ASSERT_FALSE(st.valid(t0, 401));
  TEST_ASSERT_TRUE(st.valid(t5, 401));
  TEST_ASSERT_EQUAL(kMaxSessions, st.count(401));
  TEST_ASSERT_TRUE(st.remove(t5));
  TEST_ASSERT_FALSE(st.valid(t5, 402));
  st.clear();                                    // parola değişimi
  TEST_ASSERT_EQUAL(0, st.count(402));
}

void test_limiter_locks_after_five_and_releases() {
  LoginLimiter l;
  const uint32_t ip = 0x0A000001, other = 0x0A000002;
  for (int i = 0; i < 4; ++i) { TEST_ASSERT_EQUAL(0, l.lockedFor(ip, 10 + i)); l.fail(ip, 10 + i); }
  TEST_ASSERT_EQUAL(0, l.lockedFor(ip, 14));
  l.fail(ip, 14);                                // 5. hata
  TEST_ASSERT_EQUAL(LoginLimiter::kLockS, l.lockedFor(ip, 14));
  TEST_ASSERT_EQUAL(0, l.lockedFor(other, 14));  // başka IP etkilenmez
  TEST_ASSERT_EQUAL(0, l.lockedFor(ip, 14 + LoginLimiter::kLockS));
  // Pencere dışındaki hatalar birikmez
  LoginLimiter m;
  for (int i = 0; i < 4; ++i) m.fail(ip, i);
  m.fail(ip, 1000);
  TEST_ASSERT_EQUAL(0, m.lockedFor(ip, 1000));
  // Başarı sayacı sıfırlar
  LoginLimiter n;
  for (int i = 0; i < 4; ++i) n.fail(ip, i);
  n.success(ip);
  n.fail(ip, 5);
  TEST_ASSERT_EQUAL(0, n.lockedFor(ip, 5));
}

void test_limiter_global_distributed() {
  LoginLimiter l;
  for (uint32_t i = 0; i < LoginLimiter::kMaxFails * 4; ++i) l.fail(0x0A000100 + i, 50);   // her IP'den tek hata
  TEST_ASSERT_TRUE(l.lockedFor(0x0B000001, 51) > 0);   // genel kilit
}

void test_ct_equal_and_hex() {
  uint8_t a[4] = {1, 2, 3, 4}, b[4] = {1, 2, 3, 5};
  TEST_ASSERT_TRUE(ctEqual(a, a, 4));
  TEST_ASSERT_FALSE(ctEqual(a, b, 4));
  char h[9];
  toHex(a, 4, h);
  TEST_ASSERT_EQUAL_STRING("01020304", h);
  uint8_t c[4];
  TEST_ASSERT_TRUE(fromHex("01020304", c, 4));
  TEST_ASSERT_EQUAL_MEMORY(a, c, 4);
  TEST_ASSERT_FALSE(fromHex("0102030", c, 4));
  TEST_ASSERT_FALSE(fromHex("0102030g", c, 4));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_sessions_expire_and_evict_oldest);
  RUN_TEST(test_limiter_locks_after_five_and_releases);
  RUN_TEST(test_limiter_global_distributed);
  RUN_TEST(test_ct_equal_and_hex);
  return UNITY_END();
}
