#include "../framework/test.h"
#include "../mocks/flipper_api_fake.h"
#include "../../src/mtp/core/context.h"
#include "../../src/mtp/core/operation.h"
#include "../../src/mtp/operations/op_device_props.h"
#include <stdlib.h>
#include <string.h>

static const char* fake_name(void) {
    return "MyFlipper";
}
static uint8_t fake_battery(void) {
    return 73;
}

static MTPContext* make_ctx(void) {
    Storage* s = furi_record_open("storage");
    return mtp_context_create(s);
}

TEST(get_device_prop_value_friendly_name_uses_provider) {
    MTPContext* ctx = make_ctx();
    MTPDevicePropsProvider provider = {.get_device_name = fake_name,
                                       .get_battery_level = fake_battery};
    mtp_device_props_install(ctx, &provider);

    MTPResponse* resp = mtp_response_create();
    MTPRequest req = {0};
    req.params[0] = MTP_DEVICE_PROP_DEVICE_FRIENDLY_NAME;
    mtp_op_get_device_prop_value(ctx, &req, resp);

    ASSERT_EQ(resp->response_code, (uint16_t)MTP_RESP_OK);
    // First byte: length = strlen("MyFlipper") + 1 = 10
    ASSERT_EQ((int)resp->data[0], 10);

    mtp_response_destroy(resp);
    mtp_context_destroy(ctx);
    TEST_PASS();
}

TEST(get_device_prop_value_battery_returns_one_byte) {
    MTPContext* ctx = make_ctx();
    MTPDevicePropsProvider provider = {.get_device_name = fake_name,
                                       .get_battery_level = fake_battery};
    mtp_device_props_install(ctx, &provider);

    MTPResponse* resp = mtp_response_create();
    MTPRequest req = {0};
    req.params[0] = MTP_DEVICE_PROP_BATTERY_LEVEL;
    mtp_op_get_device_prop_value(ctx, &req, resp);

    ASSERT_EQ(resp->response_code, (uint16_t)MTP_RESP_OK);
    ASSERT_EQ(resp->data_size, (size_t)1);
    ASSERT_EQ((int)resp->data[0], 73);

    mtp_response_destroy(resp);
    mtp_context_destroy(ctx);
    TEST_PASS();
}

TEST(get_device_prop_value_falls_back_when_no_provider) {
    MTPContext* ctx = make_ctx();
    MTPResponse* resp = mtp_response_create();
    MTPRequest req = {0};
    req.params[0] = MTP_DEVICE_PROP_BATTERY_LEVEL;
    mtp_op_get_device_prop_value(ctx, &req, resp);

    ASSERT_EQ(resp->response_code, (uint16_t)MTP_RESP_OK);
    ASSERT_EQ(resp->data_size, (size_t)1);
    ASSERT_EQ((int)resp->data[0], 0);

    mtp_response_destroy(resp);
    mtp_context_destroy(ctx);
    TEST_PASS();
}

TEST(get_device_prop_value_unsupported_code) {
    MTPContext* ctx = make_ctx();
    MTPResponse* resp = mtp_response_create();
    MTPRequest req = {0};
    req.params[0] = 0xDEAD;
    mtp_op_get_device_prop_value(ctx, &req, resp);

    ASSERT_EQ(resp->response_code, (uint16_t)MTP_RESP_DEVICE_PROP_NOT_SUPPORTED);

    mtp_response_destroy(resp);
    mtp_context_destroy(ctx);
    TEST_PASS();
}

TEST(get_device_prop_desc_friendly_name) {
    MTPContext* ctx = make_ctx();
    MTPDevicePropsProvider provider = {.get_device_name = fake_name,
                                       .get_battery_level = fake_battery};
    mtp_device_props_install(ctx, &provider);

    MTPResponse* resp = mtp_response_create();
    MTPRequest req = {0};
    req.params[0] = MTP_DEVICE_PROP_DEVICE_FRIENDLY_NAME;
    mtp_op_get_device_prop_desc(ctx, &req, resp);

    ASSERT_EQ(resp->response_code, (uint16_t)MTP_RESP_OK);
    ASSERT(resp->data_size > 5);
    // Bytes 0-1: prop code, 2-3: type 0xffff (string)
    ASSERT_EQ((int)resp->data[2], 0xff);
    ASSERT_EQ((int)resp->data[3], 0xff);

    mtp_response_destroy(resp);
    mtp_context_destroy(ctx);
    TEST_PASS();
}

int main(void) {
    printf("Running op_device_props Tests\n");
    printf("=============================\n\n");

    RUN_TEST(get_device_prop_value_friendly_name_uses_provider);
    RUN_TEST(get_device_prop_value_battery_returns_one_byte);
    RUN_TEST(get_device_prop_value_falls_back_when_no_provider);
    RUN_TEST(get_device_prop_value_unsupported_code);
    RUN_TEST(get_device_prop_desc_friendly_name);

    test_summary();
    return test_exit_code();
}
