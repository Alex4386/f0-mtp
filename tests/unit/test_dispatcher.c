#include "../framework/test.h"
#include "../mocks/transport_capture.h"
#include "../../src/mtp/core/dispatcher.h"
#include "../../src/mtp/core/context.h"
#include "../../src/mtp/core/operation_registry.h"
#include "../../src/mtp/operations/op_device.h"
#include <stdlib.h>
#include <string.h>

// Build a COMMAND container into `buf`. Returns its total length.
static size_t build_command(
    uint8_t* buf,
    uint16_t op_code,
    uint32_t transaction_id,
    const uint32_t* params,
    uint8_t param_count) {
    MTPContainerHeader header;
    size_t payload = (size_t)param_count * 4;
    mtp_container_header_init(
        &header, MTP_CONTAINER_TYPE_COMMAND, op_code, transaction_id, (uint32_t)payload);
    mtp_container_header_write(&header, buf);
    for(uint8_t i = 0; i < param_count; i++) {
        uint32_t v = params[i];
        buf[12 + i * 4 + 0] = (uint8_t)(v & 0xFF);
        buf[12 + i * 4 + 1] = (uint8_t)((v >> 8) & 0xFF);
        buf[12 + i * 4 + 2] = (uint8_t)((v >> 16) & 0xFF);
        buf[12 + i * 4 + 3] = (uint8_t)((v >> 24) & 0xFF);
    }
    return 12 + payload;
}

// Stub Storage* — only the pointer identity matters since the device-info
// handler we exercise here never dereferences it.
static char fake_storage_storage = 0;

static MTPContext* make_ctx(void) {
    return mtp_context_create((Storage*)&fake_storage_storage);
}

static MTPOperationRegistry* make_registry(void) {
    MTPOperationRegistry* r = mtp_operation_registry_create();
    static const MTPOperationEntry entry_get_device_info = {
        .op_code = MTP_OP_GET_DEVICE_INFO,
        .name = "get_device_info",
        .handler = mtp_op_get_device_info,
    };
    static const MTPOperationEntry entry_open_session = {
        .op_code = MTP_OP_OPEN_SESSION,
        .name = "open_session",
        .handler = mtp_op_open_session,
    };
    static const MTPOperationEntry entry_close_session = {
        .op_code = MTP_OP_CLOSE_SESSION,
        .name = "close_session",
        .handler = mtp_op_close_session,
    };

    mtp_operation_registry_add(r, &entry_get_device_info);
    mtp_operation_registry_add(r, &entry_open_session);
    mtp_operation_registry_add(r, &entry_close_session);
    return r;
}

TEST(dispatcher_create_destroy) {
    MTPContext* ctx = make_ctx();
    MTPOperationRegistry* reg = make_registry();
    MTPTransport* transport = mtp_transport_create(NULL);

    MTPDispatcher* d = mtp_dispatcher_create(ctx, reg, transport);
    ASSERT_NOT_NULL(d);

    mtp_dispatcher_destroy(d);
    mtp_transport_destroy(transport);
    mtp_operation_registry_destroy(reg);
    mtp_context_destroy(ctx);
    TEST_PASS();
}

TEST(dispatcher_rejects_unknown_op) {
    MTPContext* ctx = make_ctx();
    MTPOperationRegistry* reg = make_registry();
    MTPTransport* transport = mtp_transport_create(NULL);
    TransportCapture cap;
    transport_capture_reset(&cap);
    transport_capture_attach(&cap, transport);

    MTPDispatcher* d = mtp_dispatcher_create(ctx, reg, transport);

    uint8_t pkt[64];
    size_t pkt_len = build_command(pkt, 0x9999, 42, NULL, 0);
    ASSERT(mtp_dispatcher_handle_packet(d, pkt, pkt_len));

    // One container should have been sent: a RESPONSE with OPERATION_NOT_SUPPORTED.
    ASSERT(cap.container_count >= 1);

    MTPContainerHeader header;
    ASSERT(mtp_container_header_parse(
        &header, cap.sent_buffer + cap.container_offsets[0], cap.container_sizes[0]));
    ASSERT_EQ(header.type, (uint16_t)MTP_CONTAINER_TYPE_RESPONSE);
    ASSERT_EQ(header.code, (uint16_t)MTP_RESP_OPERATION_NOT_SUPPORTED);
    ASSERT_EQ(header.transaction_id, (uint32_t)42);

    mtp_dispatcher_destroy(d);
    mtp_transport_destroy(transport);
    mtp_operation_registry_destroy(reg);
    mtp_context_destroy(ctx);
    TEST_PASS();
}

TEST(dispatcher_open_close_session) {
    MTPContext* ctx = make_ctx();
    MTPOperationRegistry* reg = make_registry();
    MTPTransport* transport = mtp_transport_create(NULL);
    TransportCapture cap;
    transport_capture_reset(&cap);
    transport_capture_attach(&cap, transport);

    MTPDispatcher* d = mtp_dispatcher_create(ctx, reg, transport);

    uint8_t pkt[64];
    uint32_t params[1] = {1};
    size_t pkt_len = build_command(pkt, MTP_OP_OPEN_SESSION, 7, params, 1);
    ASSERT(mtp_dispatcher_handle_packet(d, pkt, pkt_len));
    ASSERT(mtp_context_is_session_open(ctx));

    // Now close
    transport_capture_reset(&cap);
    pkt_len = build_command(pkt, MTP_OP_CLOSE_SESSION, 8, NULL, 0);
    ASSERT(mtp_dispatcher_handle_packet(d, pkt, pkt_len));
    ASSERT(!mtp_context_is_session_open(ctx));

    mtp_dispatcher_destroy(d);
    mtp_transport_destroy(transport);
    mtp_operation_registry_destroy(reg);
    mtp_context_destroy(ctx);
    TEST_PASS();
}

TEST(dispatcher_open_session_zero_is_rejected) {
    MTPContext* ctx = make_ctx();
    MTPOperationRegistry* reg = make_registry();
    MTPTransport* transport = mtp_transport_create(NULL);
    TransportCapture cap;
    transport_capture_reset(&cap);
    transport_capture_attach(&cap, transport);

    MTPDispatcher* d = mtp_dispatcher_create(ctx, reg, transport);

    uint8_t pkt[64];
    uint32_t params[1] = {0};
    size_t pkt_len = build_command(pkt, MTP_OP_OPEN_SESSION, 1, params, 1);
    ASSERT(mtp_dispatcher_handle_packet(d, pkt, pkt_len));
    ASSERT(!mtp_context_is_session_open(ctx));

    MTPContainerHeader header;
    ASSERT(mtp_container_header_parse(
        &header, cap.sent_buffer + cap.container_offsets[0], cap.container_sizes[0]));
    ASSERT_EQ(header.code, (uint16_t)MTP_RESP_INVALID_TRANSACTION_ID);

    mtp_dispatcher_destroy(d);
    mtp_transport_destroy(transport);
    mtp_operation_registry_destroy(reg);
    mtp_context_destroy(ctx);
    TEST_PASS();
}

TEST(dispatcher_get_device_info_emits_data_and_response) {
    MTPContext* ctx = make_ctx();
    mtp_context_set_manufacturer(ctx, "Flipper Devices Inc.");
    mtp_context_set_model(ctx, "Flipper Zero");
    mtp_context_set_firmware_version(ctx, "1.0.0");
    mtp_context_set_serial(ctx, "ABC123");

    MTPOperationRegistry* reg = make_registry();
    MTPTransport* transport = mtp_transport_create(NULL);
    TransportCapture cap;
    transport_capture_reset(&cap);
    transport_capture_attach(&cap, transport);

    MTPDispatcher* d = mtp_dispatcher_create(ctx, reg, transport);

    uint8_t pkt[64];
    size_t pkt_len = build_command(pkt, MTP_OP_GET_DEVICE_INFO, 99, NULL, 0);
    ASSERT(mtp_dispatcher_handle_packet(d, pkt, pkt_len));

    // Expect 3 sends from the dispatcher's data + response flow:
    //   1. DATA header
    //   2. DATA payload
    //   3. RESPONSE header (no payload)
    // The exact framing depends on `mtp_transport_send_container`, which we
    // know splits header + payload into two send calls.
    ASSERT(cap.container_count >= 2);

    // First container should be a DATA container with op=GetDeviceInfo,
    // transaction_id=99.
    MTPContainerHeader header;
    ASSERT(mtp_container_header_parse(
        &header, cap.sent_buffer + cap.container_offsets[0], cap.container_sizes[0]));
    ASSERT_EQ(header.type, (uint16_t)MTP_CONTAINER_TYPE_DATA);
    ASSERT_EQ(header.code, (uint16_t)MTP_OP_GET_DEVICE_INFO);
    ASSERT_EQ(header.transaction_id, (uint32_t)99);

    // Find a RESPONSE container with OK in the stream.
    bool found_ok_response = false;
    for(size_t i = 0; i < cap.container_count; i++) {
        if(cap.container_sizes[i] < 12) continue;
        MTPContainerHeader h;
        if(!mtp_container_header_parse(
               &h, cap.sent_buffer + cap.container_offsets[i], cap.container_sizes[i]))
            continue;
        if(h.type == MTP_CONTAINER_TYPE_RESPONSE && h.code == MTP_RESP_OK &&
           h.transaction_id == 99) {
            found_ok_response = true;
            break;
        }
    }
    ASSERT(found_ok_response);

    mtp_dispatcher_destroy(d);
    mtp_transport_destroy(transport);
    mtp_operation_registry_destroy(reg);
    mtp_context_destroy(ctx);
    TEST_PASS();
}

TEST(dispatcher_rejects_short_buffer) {
    MTPContext* ctx = make_ctx();
    MTPOperationRegistry* reg = make_registry();
    MTPTransport* transport = mtp_transport_create(NULL);
    MTPDispatcher* d = mtp_dispatcher_create(ctx, reg, transport);

    uint8_t pkt[8] = {0};
    ASSERT(!mtp_dispatcher_handle_packet(d, pkt, sizeof(pkt)));

    mtp_dispatcher_destroy(d);
    mtp_transport_destroy(transport);
    mtp_operation_registry_destroy(reg);
    mtp_context_destroy(ctx);
    TEST_PASS();
}

int main(void) {
    printf("Running Dispatcher Tests\n");
    printf("========================\n\n");

    RUN_TEST(dispatcher_create_destroy);
    RUN_TEST(dispatcher_rejects_unknown_op);
    RUN_TEST(dispatcher_open_close_session);
    RUN_TEST(dispatcher_open_session_zero_is_rejected);
    RUN_TEST(dispatcher_get_device_info_emits_data_and_response);
    RUN_TEST(dispatcher_rejects_short_buffer);

    test_summary();
    return test_exit_code();
}
