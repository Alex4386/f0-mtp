#include "../framework/test.h"
#include "../mocks/flipper_api_fake.h"
#include "../../src/mtp/core/context.h"
#include "../../src/mtp/core/operation.h"
#include "../../src/mtp/operations/op_storage.h"
#include <stdlib.h>
#include <string.h>

static MTPContext* make_ctx_with_storage(void) {
    Storage* storage = furi_record_open("storage");
    return mtp_context_create(storage);
}

static MTPRequest req_with_params(uint32_t p0) {
    MTPRequest r = {0};
    r.params[0] = p0;
    return r;
}

TEST(storage_ids_lists_internal_and_external_when_sd_present) {
    flipper_fake_reset();
    MTPContext* ctx = make_ctx_with_storage();
    MTPResponse* resp = mtp_response_create();
    MTPRequest req = {0};

    mtp_op_get_storage_ids(ctx, &req, resp);

    ASSERT_EQ(resp->response_code, (uint16_t)MTP_RESP_OK);
    ASSERT(resp->data_size >= 4 + 4 * 2);
    uint32_t count = (uint32_t)resp->data[0] | ((uint32_t)resp->data[1] << 8) |
                     ((uint32_t)resp->data[2] << 16) | ((uint32_t)resp->data[3] << 24);
    ASSERT_EQ((long)count, 2l);

    mtp_response_destroy(resp);
    mtp_context_destroy(ctx);
    TEST_PASS();
}

TEST(storage_info_external) {
    flipper_fake_reset();
    MTPContext* ctx = make_ctx_with_storage();
    MTPResponse* resp = mtp_response_create();
    MTPRequest req = req_with_params(MTP_STORAGE_ID_EXTERNAL);

    mtp_op_get_storage_info(ctx, &req, resp);

    ASSERT_EQ(resp->response_code, (uint16_t)MTP_RESP_OK);
    ASSERT(resp->data_size > 26);
    uint16_t stype = (uint16_t)resp->data[0] | ((uint16_t)resp->data[1] << 8);
    ASSERT_EQ(stype, (uint16_t)0x0004); // REMOVABLE_RAM

    mtp_response_destroy(resp);
    mtp_context_destroy(ctx);
    TEST_PASS();
}

TEST(storage_info_internal) {
    flipper_fake_reset();
    MTPContext* ctx = make_ctx_with_storage();
    MTPResponse* resp = mtp_response_create();
    MTPRequest req = req_with_params(MTP_STORAGE_ID_INTERNAL);

    mtp_op_get_storage_info(ctx, &req, resp);

    ASSERT_EQ(resp->response_code, (uint16_t)MTP_RESP_OK);
    uint16_t stype = (uint16_t)resp->data[0] | ((uint16_t)resp->data[1] << 8);
    ASSERT_EQ(stype, (uint16_t)0x0003); // FIXED_RAM

    mtp_response_destroy(resp);
    mtp_context_destroy(ctx);
    TEST_PASS();
}

TEST(storage_info_invalid_id) {
    flipper_fake_reset();
    MTPContext* ctx = make_ctx_with_storage();
    MTPResponse* resp = mtp_response_create();
    MTPRequest req = req_with_params(0xdeadbeef);

    mtp_op_get_storage_info(ctx, &req, resp);
    ASSERT_EQ(resp->response_code, (uint16_t)MTP_RESP_INVALID_STORAGE_ID);

    mtp_response_destroy(resp);
    mtp_context_destroy(ctx);
    TEST_PASS();
}

int main(void) {
    printf("Running op_storage Tests\n");
    printf("========================\n\n");

    RUN_TEST(storage_ids_lists_internal_and_external_when_sd_present);
    RUN_TEST(storage_info_external);
    RUN_TEST(storage_info_internal);
    RUN_TEST(storage_info_invalid_id);

    test_summary();
    return test_exit_code();
}
