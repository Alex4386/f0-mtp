#include "../framework/test.h"
#include "../../src/mtp/operations/storage_info.h"
#include <string.h>

static MTPStorageInfoInput sample_input(void) {
    MTPStorageInfoInput in = {
        .storage_type = MTP_STORAGE_TYPE_REMOVABLE_RAM,
        .filesystem_type = MTP_FS_TYPE_GENERIC_HIERARCHICAL,
        .access_capability = MTP_ACCESS_READWRITE,
        .max_capacity_blocks = 1000,
        .free_space_blocks = 500,
        .free_space_in_objects = 20,
        .storage_description = "SD Card",
        .volume_identifier = "SD_CARD",
    };
    return in;
}

TEST(storage_info_writes_some_bytes) {
    uint8_t buf[256];
    MTPStorageInfoInput in = sample_input();
    size_t n = mtp_build_storage_info(&in, buf, sizeof(buf));
    ASSERT(n > 0);
    ASSERT(n < sizeof(buf));
    TEST_PASS();
}

TEST(storage_info_header_layout) {
    uint8_t buf[256];
    MTPStorageInfoInput in = sample_input();
    size_t n = mtp_build_storage_info(&in, buf, sizeof(buf));
    ASSERT(n >= 26); // 3*u16 + 2*u64 + u32 = 6 + 16 + 4 = 26

    // storage_type LE
    ASSERT_EQ(buf[0], (uint8_t)(MTP_STORAGE_TYPE_REMOVABLE_RAM & 0xFF));
    ASSERT_EQ(buf[1], (uint8_t)((MTP_STORAGE_TYPE_REMOVABLE_RAM >> 8) & 0xFF));

    // filesystem_type LE
    ASSERT_EQ(buf[2], (uint8_t)(MTP_FS_TYPE_GENERIC_HIERARCHICAL & 0xFF));
    ASSERT_EQ(buf[3], (uint8_t)((MTP_FS_TYPE_GENERIC_HIERARCHICAL >> 8) & 0xFF));

    // access_capability LE
    ASSERT_EQ(buf[4], (uint8_t)(MTP_ACCESS_READWRITE & 0xFF));
    ASSERT_EQ(buf[5], (uint8_t)((MTP_ACCESS_READWRITE >> 8) & 0xFF));

    // max_capacity_blocks LE u64 = 1000
    uint64_t cap = 0;
    for(int i = 0; i < 8; i++) cap |= ((uint64_t)buf[6 + i]) << (i * 8);
    ASSERT_EQ((long)cap, 1000l);

    // free_space_blocks LE u64 = 500
    uint64_t freev = 0;
    for(int i = 0; i < 8; i++) freev |= ((uint64_t)buf[14 + i]) << (i * 8);
    ASSERT_EQ((long)freev, 500l);

    // free_space_in_objects LE u32 = 20
    uint32_t freeobj = (uint32_t)buf[22] | ((uint32_t)buf[23] << 8) |
                       ((uint32_t)buf[24] << 16) | ((uint32_t)buf[25] << 24);
    ASSERT_EQ(freeobj, (uint32_t)20);

    TEST_PASS();
}

TEST(storage_info_overflow) {
    uint8_t buf[10];
    MTPStorageInfoInput in = sample_input();
    size_t n = mtp_build_storage_info(&in, buf, sizeof(buf));
    ASSERT_EQ(n, (size_t)0);
    TEST_PASS();
}

TEST(storage_info_null_safe) {
    uint8_t buf[256];
    MTPStorageInfoInput in = sample_input();
    ASSERT_EQ(mtp_build_storage_info(NULL, buf, sizeof(buf)), (size_t)0);
    ASSERT_EQ(mtp_build_storage_info(&in, NULL, sizeof(buf)), (size_t)0);
    ASSERT_EQ(mtp_build_storage_info(&in, buf, 0), (size_t)0);
    TEST_PASS();
}

TEST(bytes_to_blocks_helper) {
    ASSERT_EQ((long)mtp_bytes_to_blocks(0), 0l);
    ASSERT_EQ((long)mtp_bytes_to_blocks(MTP_BLOCK_SIZE), 1l);
    ASSERT_EQ((long)mtp_bytes_to_blocks(MTP_BLOCK_SIZE * 10), 10l);
    TEST_PASS();
}

TEST(kb_to_blocks_helper) {
    // MTP_BLOCK_SIZE is 65536, so 64 KB = 1 block
    ASSERT_EQ((long)mtp_kb_to_blocks(0), 0l);
    ASSERT_EQ((long)mtp_kb_to_blocks(64), 1l);
    ASSERT_EQ((long)mtp_kb_to_blocks(128), 2l);
    TEST_PASS();
}

int main(void) {
    printf("Running Storage Info Builder Tests\n");
    printf("===================================\n\n");

    RUN_TEST(storage_info_writes_some_bytes);
    RUN_TEST(storage_info_header_layout);
    RUN_TEST(storage_info_overflow);
    RUN_TEST(storage_info_null_safe);
    RUN_TEST(bytes_to_blocks_helper);
    RUN_TEST(kb_to_blocks_helper);

    test_summary();
    return test_exit_code();
}
