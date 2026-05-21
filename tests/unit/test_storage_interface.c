#include "../framework/test.h"
#include "flipper_api_fake.h"
#include <string.h>

// Test: Basic file operations
TEST(flipper_file_operations) {
    flipper_fake_reset();
    flipper_fake_add_file("/test.txt", "hello world");

    Storage* storage = furi_record_open("storage");
    ASSERT(storage != NULL);

    File* file = storage_file_alloc(storage);
    ASSERT(file != NULL);

    // Open and read
    ASSERT(storage_file_open(file, "/test.txt", FSAM_READ, FSOM_OPEN_EXISTING));

    char buffer[32] = {0};
    uint16_t bytes_read = storage_file_read(file, buffer, sizeof(buffer));
    ASSERT_EQ(bytes_read, 11); // strlen("hello world")
    ASSERT_STR_EQ(buffer, "hello world");

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close("storage");
    TEST_PASS();
}

// Test: File write
TEST(flipper_file_write) {
    flipper_fake_reset();
    flipper_fake_add_file("/output.txt", "");

    Storage* storage = furi_record_open("storage");
    File* file = storage_file_alloc(storage);

    ASSERT(storage_file_open(file, "/output.txt", FSAM_WRITE, FSOM_OPEN_EXISTING));

    const char* data = "written data";
    uint16_t bytes_written = storage_file_write(file, data, strlen(data));
    ASSERT_EQ(bytes_written, 12);

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close("storage");
    TEST_PASS();
}

// Test: File exists
TEST(flipper_file_exists) {
    flipper_fake_reset();
    flipper_fake_add_file("/exists.txt", "data");
    flipper_fake_add_dir("/mydir");

    Storage* storage = furi_record_open("storage");

    ASSERT(storage_file_exists(storage, "/exists.txt"));
    ASSERT(!storage_file_exists(storage, "/missing.txt"));
    ASSERT(storage_dir_exists(storage, "/mydir"));
    ASSERT(!storage_dir_exists(storage, "/nodir"));

    furi_record_close("storage");
    TEST_PASS();
}

// Test: Directory operations
TEST(flipper_dir_operations) {
    flipper_fake_reset();
    flipper_fake_add_file("/file1.txt", "one");
    flipper_fake_add_file("/file2.txt", "two");
    flipper_fake_add_dir("/dir");

    Storage* storage = furi_record_open("storage");
    File* dir = storage_file_alloc(storage);

    ASSERT(storage_dir_open(dir, "/"));

    FileInfo info;
    char name[256];

    // Read entries
    bool has_entry = storage_dir_read(dir, &info, name, sizeof(name));
    ASSERT(has_entry);

    has_entry = storage_dir_read(dir, &info, name, sizeof(name));
    ASSERT(has_entry);

    has_entry = storage_dir_read(dir, &info, name, sizeof(name));
    ASSERT(has_entry);

    // No more
    has_entry = storage_dir_read(dir, &info, name, sizeof(name));
    ASSERT(!has_entry);

    storage_dir_close(dir);
    storage_file_free(dir);
    furi_record_close("storage");
    TEST_PASS();
}

// Test: mkdir
TEST(flipper_mkdir) {
    flipper_fake_reset();

    Storage* storage = furi_record_open("storage");

    ASSERT(storage_simply_mkdir(storage, "/newdir"));
    ASSERT(storage_dir_exists(storage, "/newdir"));

    furi_record_close("storage");
    TEST_PASS();
}

// Test: remove
TEST(flipper_remove) {
    flipper_fake_reset();
    flipper_fake_add_file("/remove_me.txt", "data");

    Storage* storage = furi_record_open("storage");

    ASSERT(storage_file_exists(storage, "/remove_me.txt"));

    FS_Error err = storage_common_remove(storage, "/remove_me.txt");
    ASSERT_EQ(err, FSE_OK);

    ASSERT(!storage_file_exists(storage, "/remove_me.txt"));

    furi_record_close("storage");
    TEST_PASS();
}

// Test: rename
TEST(flipper_rename) {
    flipper_fake_reset();
    flipper_fake_add_file("/old.txt", "content");

    Storage* storage = furi_record_open("storage");

    FS_Error err = storage_common_rename(storage, "/old.txt", "/new.txt");
    ASSERT_EQ(err, FSE_OK);

    ASSERT(!storage_file_exists(storage, "/old.txt"));
    ASSERT(storage_file_exists(storage, "/new.txt"));

    furi_record_close("storage");
    TEST_PASS();
}

// Test: stat
TEST(flipper_stat) {
    flipper_fake_reset();
    flipper_fake_add_file("/statme.txt", "12345");
    flipper_fake_add_dir("/statdir");

    Storage* storage = furi_record_open("storage");

    FileInfo info;

    // Stat file
    FS_Error err = storage_common_stat(storage, "/statme.txt", &info);
    ASSERT_EQ(err, FSE_OK);
    ASSERT_EQ(info.size, 5);
    ASSERT(!file_info_is_dir(&info));

    // Stat directory
    err = storage_common_stat(storage, "/statdir", &info);
    ASSERT_EQ(err, FSE_OK);
    ASSERT(file_info_is_dir(&info));

    furi_record_close("storage");
    TEST_PASS();
}

// Test: sd_info
TEST(flipper_sd_info) {
    flipper_fake_reset();

    Storage* storage = furi_record_open("storage");

    SDInfo info;
    FS_Error err = storage_sd_info(storage, &info);
    ASSERT_EQ(err, FSE_OK);
    ASSERT(info.kb_total > 0);
    ASSERT(info.kb_free > 0);

    furi_record_close("storage");
    TEST_PASS();
}

// Test: fail_open error condition
TEST(flipper_fail_open) {
    flipper_fake_reset();
    flipper_fake_add_file("/file.txt", "data");
    flipper_fake_set_fail_open(true);

    Storage* storage = furi_record_open("storage");
    File* file = storage_file_alloc(storage);

    ASSERT(!storage_file_open(file, "/file.txt", FSAM_READ, FSOM_OPEN_EXISTING));

    storage_file_free(file);
    furi_record_close("storage");
    TEST_PASS();
}

// Test: fail_write error condition
TEST(flipper_fail_write) {
    flipper_fake_reset();
    flipper_fake_add_file("/file.txt", "");
    flipper_fake_set_fail_write(true);

    Storage* storage = furi_record_open("storage");
    File* file = storage_file_alloc(storage);

    ASSERT(storage_file_open(file, "/file.txt", FSAM_WRITE, FSOM_OPEN_EXISTING));

    const char* data = "test";
    uint16_t bytes = storage_file_write(file, data, strlen(data));
    ASSERT_EQ(bytes, 0); // Write failed

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close("storage");
    TEST_PASS();
}

int main(void) {
    printf("Running Flipper Storage API Tests\n");
    printf("==================================\n\n");

    RUN_TEST(flipper_file_operations);
    RUN_TEST(flipper_file_write);
    RUN_TEST(flipper_file_exists);
    RUN_TEST(flipper_dir_operations);
    RUN_TEST(flipper_mkdir);
    RUN_TEST(flipper_remove);
    RUN_TEST(flipper_rename);
    RUN_TEST(flipper_stat);
    RUN_TEST(flipper_sd_info);
    RUN_TEST(flipper_fail_open);
    RUN_TEST(flipper_fail_write);

    test_summary();
    return test_exit_code();
}
