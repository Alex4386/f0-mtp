#include "../framework/test.h"
#include "../../src/mtp/operations/object_info.h"
#include <string.h>

static MTPObjectInfoInput sample(void) {
    MTPObjectInfoInput in = {
        .storage_id = MTP_STORAGE_ID_EXTERNAL,
        .object_format = MTP_FORMAT_UNDEFINED,
        .protection_status = 0,
        .object_compressed_size = 12345,
        .association_type = 0,
        .association_desc = 0,
        .parent_object = 0xffffffff,
        .filename = "hello.txt",
        .date_created = "20240608T010702",
        .date_modified = "20240608T010702",
        .keywords = "",
    };
    return in;
}

TEST(object_info_writes_some_bytes) {
    uint8_t buf[256];
    MTPObjectInfoInput in = sample();
    size_t n = mtp_build_object_info(&in, buf, sizeof(buf));
    ASSERT(n > 52);
    TEST_PASS();
}

TEST(object_info_fixed_layout_starts_with_storage_id) {
    uint8_t buf[256];
    MTPObjectInfoInput in = sample();
    size_t n = mtp_build_object_info(&in, buf, sizeof(buf));
    ASSERT(n > 4);
    uint32_t storage_id = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
                          ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    ASSERT_EQ(storage_id, (uint32_t)MTP_STORAGE_ID_EXTERNAL);
    TEST_PASS();
}

TEST(object_info_format_at_offset_4) {
    uint8_t buf[256];
    MTPObjectInfoInput in = sample();
    in.object_format = MTP_FORMAT_ASSOCIATION;
    in.association_type = 0x0001;
    size_t n = mtp_build_object_info(&in, buf, sizeof(buf));
    ASSERT(n > 6);
    uint16_t fmt = (uint16_t)buf[4] | ((uint16_t)buf[5] << 8);
    ASSERT_EQ(fmt, (uint16_t)MTP_FORMAT_ASSOCIATION);
    TEST_PASS();
}

TEST(object_info_compressed_size) {
    uint8_t buf[256];
    MTPObjectInfoInput in = sample();
    in.object_compressed_size = 0xdeadbeef;
    size_t n = mtp_build_object_info(&in, buf, sizeof(buf));
    ASSERT(n > 12);
    // offset 8: compressed_size (after storage_id 4B, format 2B, protection 2B)
    uint32_t cs = (uint32_t)buf[8] | ((uint32_t)buf[9] << 8) | ((uint32_t)buf[10] << 16) |
                  ((uint32_t)buf[11] << 24);
    ASSERT_EQ(cs, (uint32_t)0xdeadbeef);
    TEST_PASS();
}

TEST(object_info_overflow) {
    uint8_t buf[16];
    MTPObjectInfoInput in = sample();
    size_t n = mtp_build_object_info(&in, buf, sizeof(buf));
    ASSERT_EQ(n, (size_t)0);
    TEST_PASS();
}

TEST(object_info_null_safe) {
    uint8_t buf[256];
    MTPObjectInfoInput in = sample();
    ASSERT_EQ(mtp_build_object_info(NULL, buf, sizeof(buf)), (size_t)0);
    ASSERT_EQ(mtp_build_object_info(&in, NULL, sizeof(buf)), (size_t)0);
    ASSERT_EQ(mtp_build_object_info(&in, buf, 0), (size_t)0);
    TEST_PASS();
}

int main(void) {
    printf("Running Object Info Builder Tests\n");
    printf("==================================\n\n");

    RUN_TEST(object_info_writes_some_bytes);
    RUN_TEST(object_info_fixed_layout_starts_with_storage_id);
    RUN_TEST(object_info_format_at_offset_4);
    RUN_TEST(object_info_compressed_size);
    RUN_TEST(object_info_overflow);
    RUN_TEST(object_info_null_safe);

    test_summary();
    return test_exit_code();
}
