#include "../framework/test.h"
#include "../../src/mtp/operations/object_props.h"
#include <string.h>

TEST(prop_storage_id_basic) {
    uint8_t buf[64];
    size_t n = mtp_build_object_prop_value(
        MTP_PROP_STORAGE_ID, MTP_STORAGE_ID_EXTERNAL, NULL, buf, sizeof(buf));
    // header (u32 prop + u32 type + u8 flag) + value (u32) + trailer (u8) = 14
    ASSERT_EQ(n, (size_t)14);

    uint32_t prop = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) |
                    ((uint32_t)buf[3] << 24);
    ASSERT_EQ(prop, (uint32_t)MTP_PROP_STORAGE_ID);

    uint32_t storage = (uint32_t)buf[9] | ((uint32_t)buf[10] << 8) | ((uint32_t)buf[11] << 16) |
                       ((uint32_t)buf[12] << 24);
    ASSERT_EQ(storage, (uint32_t)MTP_STORAGE_ID_EXTERNAL);
    TEST_PASS();
}

TEST(prop_object_format_basic) {
    uint8_t buf[64];
    size_t n = mtp_build_object_prop_value(
        MTP_PROP_OBJECT_FORMAT, MTP_STORAGE_ID_EXTERNAL, NULL, buf, sizeof(buf));
    // header (u32 + u32 + u8) + value (u16) + trailer (u8) = 12
    ASSERT_EQ(n, (size_t)12);
    uint16_t fmt = (uint16_t)buf[9] | ((uint16_t)buf[10] << 8);
    ASSERT_EQ(fmt, (uint16_t)MTP_FORMAT_UNDEFINED);
    TEST_PASS();
}

TEST(prop_file_name_basic) {
    uint8_t buf[128];
    size_t n =
        mtp_build_object_prop_value(MTP_PROP_OBJECT_FILE_NAME, 0, "hello.txt", buf, sizeof(buf));
    ASSERT(n > 12);
    // First 4 bytes = prop_code
    uint32_t prop = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) |
                    ((uint32_t)buf[3] << 24);
    ASSERT_EQ(prop, (uint32_t)MTP_PROP_OBJECT_FILE_NAME);
    TEST_PASS();
}

TEST(prop_unsupported_returns_zero) {
    uint8_t buf[64];
    size_t n = mtp_build_object_prop_value(0xFFFF, 0, NULL, buf, sizeof(buf));
    ASSERT_EQ(n, (size_t)0);
    TEST_PASS();
}

TEST(prop_overflow_returns_zero) {
    uint8_t buf[4];
    size_t n = mtp_build_object_prop_value(
        MTP_PROP_STORAGE_ID, MTP_STORAGE_ID_EXTERNAL, NULL, buf, sizeof(buf));
    ASSERT_EQ(n, (size_t)0);
    TEST_PASS();
}

TEST(prop_null_buffer) {
    ASSERT_EQ(mtp_build_object_prop_value(MTP_PROP_STORAGE_ID, 0, NULL, NULL, 64), (size_t)0);
    TEST_PASS();
}

int main(void) {
    printf("Running Object Props Tests\n");
    printf("===========================\n\n");

    RUN_TEST(prop_storage_id_basic);
    RUN_TEST(prop_object_format_basic);
    RUN_TEST(prop_file_name_basic);
    RUN_TEST(prop_unsupported_returns_zero);
    RUN_TEST(prop_overflow_returns_zero);
    RUN_TEST(prop_null_buffer);

    test_summary();
    return test_exit_code();
}
