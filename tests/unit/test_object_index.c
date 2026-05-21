#include "../framework/test.h"
#include "../../src/mtp/core/object_index.h"
#include <string.h>

TEST(object_index_create_success) {
    MTPObjectIndex* index = mtp_object_index_create();
    ASSERT_NOT_NULL(index);
    ASSERT_EQ(mtp_object_index_count(index), (size_t)0);
    mtp_object_index_destroy(index);
    TEST_PASS();
}

TEST(object_index_add_returns_nonzero_handle) {
    MTPObjectIndex* index = mtp_object_index_create();
    ASSERT_NOT_NULL(index);

    uint32_t handle = mtp_object_index_add(index, "/ext/test.txt");
    ASSERT_NE(handle, (uint32_t)0);
    ASSERT_EQ(mtp_object_index_count(index), (size_t)1);

    mtp_object_index_destroy(index);
    TEST_PASS();
}

TEST(object_index_get_path_returns_correct_path) {
    MTPObjectIndex* index = mtp_object_index_create();

    uint32_t handle = mtp_object_index_add(index, "/ext/test.txt");
    const char* path = mtp_object_index_get_path(index, handle);

    ASSERT_NOT_NULL(path);
    ASSERT_STR_EQ(path, "/ext/test.txt");

    mtp_object_index_destroy(index);
    TEST_PASS();
}

TEST(object_index_deduplicates_paths) {
    MTPObjectIndex* index = mtp_object_index_create();

    uint32_t h1 = mtp_object_index_add(index, "/ext/test.txt");
    uint32_t h2 = mtp_object_index_add(index, "/ext/test.txt");

    ASSERT_EQ(h1, h2);
    ASSERT_EQ(mtp_object_index_count(index), (size_t)1);

    mtp_object_index_destroy(index);
    TEST_PASS();
}

TEST(object_index_handles_multiple_paths) {
    MTPObjectIndex* index = mtp_object_index_create();

    uint32_t h1 = mtp_object_index_add(index, "/ext/file1.txt");
    uint32_t h2 = mtp_object_index_add(index, "/ext/file2.txt");
    uint32_t h3 = mtp_object_index_add(index, "/ext/file3.txt");

    ASSERT_NE(h1, h2);
    ASSERT_NE(h2, h3);
    ASSERT_NE(h1, h3);
    ASSERT_EQ(mtp_object_index_count(index), (size_t)3);

    mtp_object_index_destroy(index);
    TEST_PASS();
}

TEST(object_index_update_path_works) {
    MTPObjectIndex* index = mtp_object_index_create();

    uint32_t h = mtp_object_index_add(index, "/ext/old.txt");
    ASSERT(mtp_object_index_update_path(index, h, "/ext/new.txt"));

    const char* p = mtp_object_index_get_path(index, h);
    ASSERT_STR_EQ(p, "/ext/new.txt");

    // Update nonexistent fails
    ASSERT(!mtp_object_index_update_path(index, 9999, "/x"));

    mtp_object_index_destroy(index);
    TEST_PASS();
}

TEST(object_index_remove_works) {
    MTPObjectIndex* index = mtp_object_index_create();

    uint32_t h = mtp_object_index_add(index, "/ext/file.txt");
    ASSERT(mtp_object_index_contains(index, h));
    ASSERT(mtp_object_index_remove(index, h));
    ASSERT(!mtp_object_index_contains(index, h));
    ASSERT_EQ(mtp_object_index_count(index), (size_t)0);

    // Remove twice fails
    ASSERT(!mtp_object_index_remove(index, h));

    mtp_object_index_destroy(index);
    TEST_PASS();
}

TEST(object_index_contains_works) {
    MTPObjectIndex* index = mtp_object_index_create();
    ASSERT(!mtp_object_index_contains(index, 1));

    uint32_t h = mtp_object_index_add(index, "/ext/file.txt");
    ASSERT(mtp_object_index_contains(index, h));
    ASSERT(!mtp_object_index_contains(index, h + 999));

    mtp_object_index_destroy(index);
    TEST_PASS();
}

TEST(object_index_clear_works) {
    MTPObjectIndex* index = mtp_object_index_create();
    mtp_object_index_add(index, "/a");
    mtp_object_index_add(index, "/b");
    mtp_object_index_add(index, "/c");
    ASSERT_EQ(mtp_object_index_count(index), (size_t)3);

    mtp_object_index_clear(index);
    ASSERT_EQ(mtp_object_index_count(index), (size_t)0);

    // Handles after clear should be distinct from before (no reuse)
    uint32_t new_handle = mtp_object_index_add(index, "/a");
    ASSERT(new_handle > 3);

    mtp_object_index_destroy(index);
    TEST_PASS();
}

TEST(object_index_handles_many_entries) {
    MTPObjectIndex* index = mtp_object_index_create();
    char path[64];
    for(int i = 0; i < 100; i++) {
        snprintf(path, sizeof(path), "/ext/file_%d.txt", i);
        uint32_t h = mtp_object_index_add(index, path);
        ASSERT_NE(h, (uint32_t)0);
    }
    ASSERT_EQ(mtp_object_index_count(index), (size_t)100);

    // Random lookups
    for(int i = 0; i < 100; i += 7) {
        snprintf(path, sizeof(path), "/ext/file_%d.txt", i);
        // Re-adding returns same handle (dedup)
        uint32_t h = mtp_object_index_add(index, path);
        const char* got = mtp_object_index_get_path(index, h);
        ASSERT_STR_EQ(got, path);
    }

    mtp_object_index_destroy(index);
    TEST_PASS();
}

int main(void) {
    printf("Running Object Index Tests\n");
    printf("===========================\n\n");

    RUN_TEST(object_index_create_success);
    RUN_TEST(object_index_add_returns_nonzero_handle);
    RUN_TEST(object_index_get_path_returns_correct_path);
    RUN_TEST(object_index_deduplicates_paths);
    RUN_TEST(object_index_handles_multiple_paths);
    RUN_TEST(object_index_update_path_works);
    RUN_TEST(object_index_remove_works);
    RUN_TEST(object_index_contains_works);
    RUN_TEST(object_index_clear_works);
    RUN_TEST(object_index_handles_many_entries);

    test_summary();
    return test_exit_code();
}
