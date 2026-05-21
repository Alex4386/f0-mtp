#include "../framework/test.h"
#include "../mocks/flipper_api_fake.h"
#include "../../src/mtp/core/context.h"
#include "../../src/mtp/core/transfer_state.h"
#include "../../src/mtp/core/mtp_string.h"
#include <stdlib.h>
#include <string.h>

static MTPContext* fresh_ctx(void) {
    flipper_fake_reset();
    flipper_fake_add_dir("/ext");
    Storage* s = furi_record_open("storage");
    MTPContext* ctx = mtp_context_create(s);
    ctx->transfer_manager = mtp_transfer_state_manager_create();
    ctx->transfer_manager_destroy = mtp_transfer_state_manager_destroy;
    return ctx;
}

// Build a minimal ObjectInfo dataset: 52 bytes of fixed header followed by
// the filename string. The fixed bytes can be zero except for the format
// field at offset 4.
static size_t build_object_info_bytes(
    uint8_t* out,
    size_t out_size,
    bool is_dir,
    const char* filename) {
    memset(out, 0, 52);
    uint16_t fmt = is_dir ? MTP_FORMAT_ASSOCIATION : MTP_FORMAT_UNDEFINED;
    out[4] = (uint8_t)(fmt & 0xFF);
    out[5] = (uint8_t)((fmt >> 8) & 0xFF);

    size_t n = mtp_string_write(out + 52, filename);
    (void)out_size;
    return 52 + n;
}

TEST(initial_state_is_none) {
    MTPTransferStateManager* m = mtp_transfer_state_manager_create();
    ASSERT_EQ((int)mtp_transfer_state_kind(m), (int)MTP_TRANSFER_NONE);
    mtp_transfer_state_manager_destroy(m);
    TEST_PASS();
}

TEST(send_object_info_creates_file_and_handle) {
    MTPContext* ctx = fresh_ctx();

    mtp_transfer_state_begin_send_object_info(
        ctx->transfer_manager, 1, MTP_STORAGE_ID_EXTERNAL, 0xffffffff);

    uint8_t buf[256];
    size_t n = build_object_info_bytes(buf, sizeof(buf), false, "newfile.txt");

    bool done = false;
    ASSERT(mtp_transfer_state_feed_data(
        ctx->transfer_manager, ctx, buf, n, (uint32_t)n, true, &done));
    ASSERT(done);

    uint32_t sid = 0, parent = 0, handle = 0;
    uint16_t rc = mtp_transfer_state_finalize_object_info(
        ctx->transfer_manager, ctx, &sid, &parent, &handle);
    ASSERT_EQ(rc, (uint16_t)MTP_RESP_OK);
    ASSERT_EQ(sid, (uint32_t)MTP_STORAGE_ID_EXTERNAL);
    ASSERT_NE(handle, (uint32_t)0);
    ASSERT_STR_EQ(mtp_object_index_get_path(ctx->object_index, handle), "/ext/newfile.txt");

    Storage* s = furi_record_open("storage");
    ASSERT(storage_file_exists(s, "/ext/newfile.txt"));

    mtp_context_destroy(ctx);
    TEST_PASS();
}

TEST(send_object_info_then_send_object_streams_bytes) {
    MTPContext* ctx = fresh_ctx();

    // Phase A: SendObjectInfo
    mtp_transfer_state_begin_send_object_info(
        ctx->transfer_manager, 1, MTP_STORAGE_ID_EXTERNAL, 0xffffffff);
    uint8_t infobuf[256];
    size_t n = build_object_info_bytes(infobuf, sizeof(infobuf), false, "stream.bin");
    bool done = false;
    mtp_transfer_state_feed_data(ctx->transfer_manager, ctx, infobuf, n, (uint32_t)n, true, &done);
    uint32_t handle = 0;
    mtp_transfer_state_finalize_object_info(ctx->transfer_manager, ctx, NULL, NULL, &handle);
    ASSERT_NE(handle, (uint32_t)0);

    // Phase B: SendObject — stream 100 KB across many chunks. This is the
    // regression-test for the 64K crash bug: we must NEVER allocate a single
    // contiguous buffer for the payload.
    ASSERT(mtp_transfer_state_begin_send_object(ctx->transfer_manager, ctx, 2));

    const uint32_t total = 100 * 1024;
    uint32_t pushed = 0;
    bool completed = false;
    uint8_t chunk[1024];
    for(size_t i = 0; i < sizeof(chunk); i++) chunk[i] = (uint8_t)(i & 0xFF);

    while(pushed < total) {
        uint32_t to_push = total - pushed > sizeof(chunk) ? sizeof(chunk) : (total - pushed);
        ASSERT(mtp_transfer_state_feed_data(
            ctx->transfer_manager,
            ctx,
            chunk,
            to_push,
            total,
            pushed == 0,
            &completed));
        pushed += to_push;
    }

    ASSERT(completed);
    ASSERT_EQ((int)mtp_transfer_state_kind(ctx->transfer_manager), (int)MTP_TRANSFER_NONE);

    const char* contents = flipper_fake_get_file_contents("/ext/stream.bin");
    ASSERT_NOT_NULL(contents);

    Storage* s = furi_record_open("storage");
    File* f = storage_file_alloc(s);
    ASSERT(storage_file_open(f, "/ext/stream.bin", FSAM_READ, FSOM_OPEN_EXISTING));
    ASSERT_EQ((long)storage_file_size(f), (long)total);
    storage_file_close(f);
    storage_file_free(f);

    mtp_context_destroy(ctx);
    TEST_PASS();
}

TEST(send_object_without_prior_info_fails) {
    MTPContext* ctx = fresh_ctx();
    ASSERT(!mtp_transfer_state_begin_send_object(ctx->transfer_manager, ctx, 5));
    mtp_context_destroy(ctx);
    TEST_PASS();
}

TEST(send_object_info_directory_creates_dir) {
    MTPContext* ctx = fresh_ctx();

    mtp_transfer_state_begin_send_object_info(
        ctx->transfer_manager, 1, MTP_STORAGE_ID_EXTERNAL, 0xffffffff);
    uint8_t buf[256];
    size_t n = build_object_info_bytes(buf, sizeof(buf), true, "newdir");
    bool done = false;
    mtp_transfer_state_feed_data(ctx->transfer_manager, ctx, buf, n, (uint32_t)n, true, &done);

    uint16_t rc = mtp_transfer_state_finalize_object_info(
        ctx->transfer_manager, ctx, NULL, NULL, NULL);
    ASSERT_EQ(rc, (uint16_t)MTP_RESP_OK);

    Storage* s = furi_record_open("storage");
    ASSERT(storage_dir_exists(s, "/ext/newdir"));

    mtp_context_destroy(ctx);
    TEST_PASS();
}

TEST(reset_cleans_up_open_file) {
    MTPContext* ctx = fresh_ctx();
    mtp_transfer_state_begin_send_object_info(
        ctx->transfer_manager, 1, MTP_STORAGE_ID_EXTERNAL, 0xffffffff);
    uint8_t buf[256];
    size_t n = build_object_info_bytes(buf, sizeof(buf), false, "x.bin");
    bool done = false;
    mtp_transfer_state_feed_data(ctx->transfer_manager, ctx, buf, n, (uint32_t)n, true, &done);
    mtp_transfer_state_finalize_object_info(ctx->transfer_manager, ctx, NULL, NULL, NULL);
    mtp_transfer_state_begin_send_object(ctx->transfer_manager, ctx, 2);

    mtp_transfer_state_reset(ctx->transfer_manager);
    ASSERT_EQ((int)mtp_transfer_state_kind(ctx->transfer_manager), (int)MTP_TRANSFER_NONE);

    mtp_context_destroy(ctx);
    TEST_PASS();
}

int main(void) {
    printf("Running transfer_state Tests\n");
    printf("============================\n\n");

    RUN_TEST(initial_state_is_none);
    RUN_TEST(send_object_info_creates_file_and_handle);
    RUN_TEST(send_object_info_then_send_object_streams_bytes);
    RUN_TEST(send_object_without_prior_info_fails);
    RUN_TEST(send_object_info_directory_creates_dir);
    RUN_TEST(reset_cleans_up_open_file);

    test_summary();
    return test_exit_code();
}
