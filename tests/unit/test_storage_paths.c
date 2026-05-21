#include "../framework/test.h"
#include "../../src/mtp/storage/storage_paths.h"
#include <string.h>

TEST(path_from_id_internal) {
    const char* p = mtp_storage_path_from_id(MTP_STORAGE_ID_INTERNAL);
    ASSERT_NOT_NULL(p);
    ASSERT_STR_EQ(p, MTP_PATH_PREFIX_INTERNAL);
    TEST_PASS();
}

TEST(path_from_id_external) {
    const char* p = mtp_storage_path_from_id(MTP_STORAGE_ID_EXTERNAL);
    ASSERT_NOT_NULL(p);
    ASSERT_STR_EQ(p, MTP_PATH_PREFIX_EXTERNAL);
    TEST_PASS();
}

TEST(path_from_id_unknown_returns_null) {
    ASSERT_NULL(mtp_storage_path_from_id(0));
    ASSERT_NULL(mtp_storage_path_from_id(0xdeadbeef));
    TEST_PASS();
}

TEST(id_from_path_internal) {
    ASSERT_EQ(mtp_storage_id_from_path("/int"), (uint32_t)MTP_STORAGE_ID_INTERNAL);
    ASSERT_EQ(mtp_storage_id_from_path("/int/foo.txt"), (uint32_t)MTP_STORAGE_ID_INTERNAL);
    ASSERT_EQ(mtp_storage_id_from_path("/int/sub/file"), (uint32_t)MTP_STORAGE_ID_INTERNAL);
    TEST_PASS();
}

TEST(id_from_path_external) {
    ASSERT_EQ(mtp_storage_id_from_path("/ext"), (uint32_t)MTP_STORAGE_ID_EXTERNAL);
    ASSERT_EQ(mtp_storage_id_from_path("/ext/foo.txt"), (uint32_t)MTP_STORAGE_ID_EXTERNAL);
    TEST_PASS();
}

TEST(id_from_path_must_be_real_boundary) {
    // "/internal_foo" should NOT match "/int" because the boundary is wrong.
    ASSERT_EQ(mtp_storage_id_from_path("/internal_foo"), (uint32_t)0);
    ASSERT_EQ(mtp_storage_id_from_path("/external"), (uint32_t)0);
    ASSERT_EQ(mtp_storage_id_from_path("/other"), (uint32_t)0);
    TEST_PASS();
}

TEST(id_from_path_null_safe) {
    ASSERT_EQ(mtp_storage_id_from_path(NULL), (uint32_t)0);
    TEST_PASS();
}

TEST(merge_path_basic) {
    char buf[64];
    size_t n = mtp_storage_merge_path("/ext", "file.txt", buf, sizeof(buf));
    ASSERT_EQ(n, (size_t)strlen("/ext/file.txt"));
    ASSERT_STR_EQ(buf, "/ext/file.txt");
    TEST_PASS();
}

TEST(merge_path_strips_trailing_slash_from_base) {
    char buf[64];
    size_t n = mtp_storage_merge_path("/ext/", "file.txt", buf, sizeof(buf));
    ASSERT_EQ(n, (size_t)strlen("/ext/file.txt"));
    ASSERT_STR_EQ(buf, "/ext/file.txt");
    TEST_PASS();
}

TEST(merge_path_strips_leading_slash_from_name) {
    char buf[64];
    size_t n = mtp_storage_merge_path("/ext", "/file.txt", buf, sizeof(buf));
    ASSERT_EQ(n, (size_t)strlen("/ext/file.txt"));
    ASSERT_STR_EQ(buf, "/ext/file.txt");
    TEST_PASS();
}

TEST(merge_path_root_base) {
    char buf[64];
    size_t n = mtp_storage_merge_path("/", "x", buf, sizeof(buf));
    ASSERT_EQ(n, (size_t)2);
    ASSERT_STR_EQ(buf, "/x");
    TEST_PASS();
}

TEST(merge_path_overflow_returns_zero) {
    char buf[8];
    size_t n = mtp_storage_merge_path("/ext", "this_is_a_long_filename.txt", buf, sizeof(buf));
    ASSERT_EQ(n, (size_t)0);
    TEST_PASS();
}

TEST(merge_path_null_safe) {
    char buf[64];
    ASSERT_EQ(mtp_storage_merge_path(NULL, "a", buf, sizeof(buf)), (size_t)0);
    ASSERT_EQ(mtp_storage_merge_path("/ext", NULL, buf, sizeof(buf)), (size_t)0);
    ASSERT_EQ(mtp_storage_merge_path("/ext", "a", NULL, 16), (size_t)0);
    ASSERT_EQ(mtp_storage_merge_path("/ext", "a", buf, 0), (size_t)0);
    TEST_PASS();
}

TEST(basename_normal) {
    ASSERT_STR_EQ(mtp_storage_basename("/ext/foo/bar.txt"), "bar.txt");
    TEST_PASS();
}

TEST(basename_no_slash) {
    ASSERT_STR_EQ(mtp_storage_basename("nopath"), "nopath");
    TEST_PASS();
}

TEST(basename_root) {
    ASSERT_STR_EQ(mtp_storage_basename("/"), "");
    TEST_PASS();
}

int main(void) {
    printf("Running Storage Paths Tests\n");
    printf("============================\n\n");

    RUN_TEST(path_from_id_internal);
    RUN_TEST(path_from_id_external);
    RUN_TEST(path_from_id_unknown_returns_null);
    RUN_TEST(id_from_path_internal);
    RUN_TEST(id_from_path_external);
    RUN_TEST(id_from_path_must_be_real_boundary);
    RUN_TEST(id_from_path_null_safe);
    RUN_TEST(merge_path_basic);
    RUN_TEST(merge_path_strips_trailing_slash_from_base);
    RUN_TEST(merge_path_strips_leading_slash_from_name);
    RUN_TEST(merge_path_root_base);
    RUN_TEST(merge_path_overflow_returns_zero);
    RUN_TEST(merge_path_null_safe);
    RUN_TEST(basename_normal);
    RUN_TEST(basename_no_slash);
    RUN_TEST(basename_root);

    test_summary();
    return test_exit_code();
}
