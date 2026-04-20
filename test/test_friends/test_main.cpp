#include <unity.h>
#include "pure/friend_lookup.h"
#include "pure/mac_lookup.h"

void setUp() {}
void tearDown() {}

// Mirrors the friends[] table in src/main.cpp.
static const int64_t IDS[] = {
    7612996805LL,  // NM
    7015581601LL,  // ST
    8252040084LL,  // GO
    8293810017LL,  // TD
    8472668102LL,  // MN
};
static const int NUM = sizeof(IDS) / sizeof(IDS[0]);

void test_known_telegram_id_returns_index() {
    TEST_ASSERT_EQUAL_INT(0, pure::getFriendIdx(7612996805LL, IDS, NUM));
    TEST_ASSERT_EQUAL_INT(1, pure::getFriendIdx(7015581601LL, IDS, NUM));
    TEST_ASSERT_EQUAL_INT(4, pure::getFriendIdx(8472668102LL, IDS, NUM));
}

void test_unknown_telegram_id_returns_minus_one() {
    TEST_ASSERT_EQUAL_INT(-1, pure::getFriendIdx(1234LL, IDS, NUM));
    TEST_ASSERT_EQUAL_INT(-1, pure::getFriendIdx(0LL, IDS, NUM));
}

// MAC lookup tests — mirrors MAC_TABLE in src/main.cpp.
static const MacMapping TABLE[] = {
    {0x85, 0x6C, 0x38, 0},  // NM
    {0xB3, 0xF3, 0x04, 1},  // ST
    {0x6E, 0x4A, 0xC8, 2},  // GO
    {0x6C, 0xB1, 0xE0, 3},  // TD
    {0x6E, 0x49, 0xC0, 4},  // MN
};
static const int TABLE_SIZE = sizeof(TABLE) / sizeof(TABLE[0]);

void test_known_mac_returns_friend_index() {
    uint8_t nm[6] = {0x10, 0x51, 0xDB, 0x85, 0x6C, 0x38};
    TEST_ASSERT_EQUAL_INT(0, pure::lookupMacOwner(nm, TABLE, TABLE_SIZE, 99));

    uint8_t mn[6] = {0x10, 0x20, 0xBA, 0x6E, 0x49, 0xC0};
    TEST_ASSERT_EQUAL_INT(4, pure::lookupMacOwner(mn, TABLE, TABLE_SIZE, 99));
}

void test_unknown_mac_falls_back_to_default() {
    uint8_t unknown[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    TEST_ASSERT_EQUAL_INT(1, pure::lookupMacOwner(unknown, TABLE, TABLE_SIZE, 1));
    TEST_ASSERT_EQUAL_INT(99, pure::lookupMacOwner(unknown, TABLE, TABLE_SIZE, 99));
}

void test_only_last_three_octets_matter() {
    // First 3 octets differ, but last 3 match NM => still maps to 0.
    uint8_t spoof[6] = {0xFF, 0xFF, 0xFF, 0x85, 0x6C, 0x38};
    TEST_ASSERT_EQUAL_INT(0, pure::lookupMacOwner(spoof, TABLE, TABLE_SIZE, 99));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_known_telegram_id_returns_index);
    RUN_TEST(test_unknown_telegram_id_returns_minus_one);
    RUN_TEST(test_known_mac_returns_friend_index);
    RUN_TEST(test_unknown_mac_falls_back_to_default);
    RUN_TEST(test_only_last_three_octets_matter);
    return UNITY_END();
}
