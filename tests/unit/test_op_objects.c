#include "../framework/test.h"
#include "../mocks/flipper_api_fake.h"
#include "../../src/mtp/core/context.h"
#include "../../src/mtp/core/operation.h"
#include "../../src/mtp/operations/op_objects.h"
#include <stdlib.h>
#include <string.h>

static MTPContext* setup_with_files(void) {
    flipper_fake_reset();
    flipper_fake_add_dir("/ext");
    flipper_fake_add_file("/ext/a.txt", "alpha");
    flipper_fake_add_file("/ext/b.txt", "beta");
    flipper_fake_add_dir("/ext/sub");
    flipper_fake_add_file("/ext/sub/inner.txt", "inside");

    Storage* storage = furi_record_open("storage");
    return mtp_context_create(storage);
}

static MTPRequest req_with(uint32_t p0, uint32_t p1, uint32_t p2) {
    MTPRequest r = {0};
    r.params[0] = p0;
    r.params[1] = p1;
    r.params[2] = p2;
    return r;
}

TEST(get_object_handles_at_root_of_external) {
    MTPContext* ctx = setup_with_files();
    MTPResponse* resp = mtp_response_create();

    MTPRequest req = req_with(MTP_STORAGE_ID_EXTERNAL, 0, 0xffffffff);
    mtp_op_get_object_handles(ctx, &req, resp);

    ASSERT_EQ(resp->response_code, (uint16_t)MTP_RESP_OK);
    ASSERT(resp->data_size >= 4);
    uint32_t count = (uint32_t)resp->data[0] | ((uint32_t)resp->data[1] << 8) |
                     ((uint32_t)resp->data[2] << 16) | ((uint32_t)resp->data[3] << 24);
    ASSERT_EQ((long)count, 3l); // a.txt, b.txt, sub

    mtp_response_destroy(resp);
    mtp_context_destroy(ctx);
    TEST_PASS();
}

TEST(get_object_handles_rejects_format_filter) {
    MTPContext* ctx = setup_with_files();
    MTPResponse* resp = mtp_response_create();

    MTPRequest req = req_with(MTP_STORAGE_ID_EXTERNAL, MTP_FORMAT_UNDEFINED, 0xffffffff);
    mtp_op_get_object_handles(ctx, &req, resp);
    ASSERT_EQ(resp->response_code, (uint16_t)MTP_RESP_SPEC_BY_FORMAT_UNSUPPORTED);

    mtp_response_destroy(resp);
    mtp_context_destroy(ctx);
    TEST_PASS();
}

TEST(get_object_info_for_file) {
    MTPContext* ctx = setup_with_files();
    uint32_t h = mtp_object_index_add(ctx->object_index, "/ext/a.txt");

    MTPResponse* resp = mtp_response_create();
    MTPRequest req = req_with(h, 0, 0);
    mtp_op_get_object_info(ctx, &req, resp);

    ASSERT_EQ(resp->response_code, (uint16_t)MTP_RESP_OK);
    // First 4 bytes = storage_id
    uint32_t sid = (uint32_t)resp->data[0] | ((uint32_t)resp->data[1] << 8) |
                   ((uint32_t)resp->data[2] << 16) | ((uint32_t)resp->data[3] << 24);
    ASSERT_EQ(sid, (uint32_t)MTP_STORAGE_ID_EXTERNAL);

    // Size at offset 8: u32
    uint32_t size = (uint32_t)resp->data[8] | ((uint32_t)resp->data[9] << 8) |
                    ((uint32_t)resp->data[10] << 16) | ((uint32_t)resp->data[11] << 24);
    ASSERT_EQ((long)size, 5l); // "alpha"

    mtp_response_destroy(resp);
    mtp_context_destroy(ctx);
    TEST_PASS();
}

TEST(get_object_info_invalid_handle) {
    MTPContext* ctx = setup_with_files();
    MTPResponse* resp = mtp_response_create();
    MTPRequest req = req_with(9999, 0, 0);
    mtp_op_get_object_info(ctx, &req, resp);
    ASSERT_EQ(resp->response_code, (uint16_t)MTP_RESP_INVALID_OBJECT_HANDLE);

    mtp_response_destroy(resp);
    mtp_context_destroy(ctx);
    TEST_PASS();
}

TEST(get_object_returns_file_contents) {
    MTPContext* ctx = setup_with_files();
    uint32_t h = mtp_object_index_add(ctx->object_index, "/ext/a.txt");

    MTPResponse* resp = mtp_response_create();
    MTPRequest req = req_with(h, 0, 0);
    mtp_op_get_object(ctx, &req, resp);

    ASSERT_EQ(resp->response_code, (uint16_t)MTP_RESP_OK);
    ASSERT_EQ(resp->data_size, (size_t)5);
    ASSERT_EQ(resp->data[0], (uint8_t)'a');
    ASSERT_EQ(resp->data[4], (uint8_t)'a');

    mtp_response_destroy(resp);
    mtp_context_destroy(ctx);
    TEST_PASS();
}

TEST(delete_object_removes_file) {
    MTPContext* ctx = setup_with_files();
    uint32_t h = mtp_object_index_add(ctx->object_index, "/ext/a.txt");

    MTPResponse* resp = mtp_response_create();
    MTPRequest req = req_with(h, 0, 0);
    mtp_op_delete_object(ctx, &req, resp);

    ASSERT_EQ(resp->response_code, (uint16_t)MTP_RESP_OK);
    ASSERT_NULL(flipper_fake_get_file_contents("/ext/a.txt"));
    ASSERT(!mtp_object_index_contains(ctx->object_index, h));

    mtp_response_destroy(resp);
    mtp_context_destroy(ctx);
    TEST_PASS();
}

TEST(delete_object_recursive_on_directory) {
    MTPContext* ctx = setup_with_files();
    uint32_t h = mtp_object_index_add(ctx->object_index, "/ext/sub");

    MTPResponse* resp = mtp_response_create();
    MTPRequest req = req_with(h, 0, 0);
    mtp_op_delete_object(ctx, &req, resp);

    ASSERT_EQ(resp->response_code, (uint16_t)MTP_RESP_OK);
    Storage* s = furi_record_open("storage");
    ASSERT(!storage_dir_exists(s, "/ext/sub"));
    ASSERT(!storage_file_exists(s, "/ext/sub/inner.txt"));

    mtp_response_destroy(resp);
    mtp_context_destroy(ctx);
    TEST_PASS();
}

TEST(move_object_renames_path) {
    flipper_fake_reset();
    flipper_fake_add_dir("/ext");
    flipper_fake_add_dir("/ext/dst");
    flipper_fake_add_file("/ext/a.txt", "alpha");

    Storage* storage = furi_record_open("storage");
    MTPContext* ctx = mtp_context_create(storage);

    uint32_t h = mtp_object_index_add(ctx->object_index, "/ext/a.txt");
    uint32_t parent_h = mtp_object_index_add(ctx->object_index, "/ext/dst");

    MTPResponse* resp = mtp_response_create();
    MTPRequest req = req_with(h, MTP_STORAGE_ID_EXTERNAL, parent_h);
    mtp_op_move_object(ctx, &req, resp);

    ASSERT_EQ(resp->response_code, (uint16_t)MTP_RESP_OK);
    ASSERT(storage_file_exists(storage, "/ext/dst/a.txt"));
    ASSERT(!storage_file_exists(storage, "/ext/a.txt"));
    ASSERT_STR_EQ(mtp_object_index_get_path(ctx->object_index, h), "/ext/dst/a.txt");

    mtp_response_destroy(resp);
    mtp_context_destroy(ctx);
    TEST_PASS();
}

int main(void) {
    printf("Running op_objects Tests\n");
    printf("========================\n\n");

    RUN_TEST(get_object_handles_at_root_of_external);
    RUN_TEST(get_object_handles_rejects_format_filter);
    RUN_TEST(get_object_info_for_file);
    RUN_TEST(get_object_info_invalid_handle);
    RUN_TEST(get_object_returns_file_contents);
    RUN_TEST(delete_object_removes_file);
    RUN_TEST(delete_object_recursive_on_directory);
    RUN_TEST(move_object_renames_path);

    test_summary();
    return test_exit_code();
}
