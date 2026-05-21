#include "../framework/test.h"
#include "../../src/mtp/core/mtp_string.h"
#include <stdlib.h>
#include <string.h>

TEST(create_and_destroy_basic) {
    MTPString* s = mtp_string_create("hello");
    ASSERT_NOT_NULL(s);
    ASSERT_EQ(s->length, (uint16_t)6); // 5 + null
    mtp_string_destroy(s);
    TEST_PASS();
}

TEST(create_null_returns_null) {
    ASSERT_NULL(mtp_string_create(NULL));
    TEST_PASS();
}

TEST(write_and_decode_roundtrip) {
    uint8_t buf[64];
    size_t n = mtp_string_write(buf, "world");
    ASSERT(n > 0);

    char* decoded = mtp_string_decode(buf, n);
    ASSERT_NOT_NULL(decoded);
    ASSERT_STR_EQ(decoded, "world");
    free(decoded);
    TEST_PASS();
}

TEST(write_empty_is_single_zero) {
    uint8_t buf[4] = {0xAA, 0xAA, 0xAA, 0xAA};
    size_t n = mtp_string_write(buf, "");
    ASSERT_EQ(n, (size_t)1);
    ASSERT_EQ(buf[0], (uint8_t)0);
    TEST_PASS();
}

TEST(decode_empty_returns_empty_string) {
    uint8_t buf[1] = {0};
    char* decoded = mtp_string_decode(buf, sizeof(buf));
    ASSERT_NOT_NULL(decoded);
    ASSERT_STR_EQ(decoded, "");
    free(decoded);
    TEST_PASS();
}

TEST(read_length_basic) {
    uint8_t buf[1] = {7};
    ASSERT_EQ((int)mtp_string_read_length(buf), 7);
    ASSERT_EQ((int)mtp_string_read_length(NULL), 0);
    TEST_PASS();
}

TEST(has_unicode_detects_high_bytes) {
    // Construct a buffer with one UTF-16 character above ASCII range.
    uint8_t buf[5];
    buf[0] = 1; // length: 1 char
    buf[1] = 0xC1; // low byte
    buf[2] = 0x00; // high byte -> total = 0x00C1 = Á
    buf[3] = 0x00;
    buf[4] = 0x00;
    ASSERT(mtp_string_has_unicode(buf));
    TEST_PASS();
}

TEST(has_unicode_false_for_ascii) {
    uint8_t buf[5];
    buf[0] = 1;
    buf[1] = 'A';
    buf[2] = 0x00;
    buf[3] = 0x00;
    buf[4] = 0x00;
    ASSERT(!mtp_string_has_unicode(buf));
    TEST_PASS();
}

TEST(is_valid_utf8_basic) {
    ASSERT(mtp_string_is_valid_utf8("hello"));
    ASSERT(!mtp_string_is_valid_utf8("Á"));
    ASSERT(!mtp_string_is_valid_utf8(NULL));
    TEST_PASS();
}

int main(void) {
    printf("Running MTP String Tests\n");
    printf("========================\n\n");

    RUN_TEST(create_and_destroy_basic);
    RUN_TEST(create_null_returns_null);
    RUN_TEST(write_and_decode_roundtrip);
    RUN_TEST(write_empty_is_single_zero);
    RUN_TEST(decode_empty_returns_empty_string);
    RUN_TEST(read_length_basic);
    RUN_TEST(has_unicode_detects_high_bytes);
    RUN_TEST(has_unicode_false_for_ascii);
    RUN_TEST(is_valid_utf8_basic);

    test_summary();
    return test_exit_code();
}
