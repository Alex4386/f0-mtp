// End-to-end MTP protocol exerciser. Drives a fully-wired dispatcher
// through a realistic session and verifies the bytes-on-wire response.
//
// Flow:
//   1. Build a command container in memory
//   2. Hand it to mtp_dispatcher_handle_packet()
//   3. Read back what the transport sent and decode the headers
//
// All storage I/O runs against the fake Flipper API.

#include "../framework/test.h"
#include "../mocks/flipper_api_fake.h"
#include "../mocks/transport_capture.h"

#include "../../src/mtp/core/dispatcher.h"
#include "../../src/mtp/core/context.h"
#include "../../src/mtp/core/operation_registry.h"
#include "../../src/mtp/operations/register_all.h"

#include <stdlib.h>
#include <string.h>

// --- helpers --------------------------------------------------------------

typedef struct {
    MTPContext* ctx;
    MTPOperationRegistry* registry;
    MTPTransport* transport;
    MTPDispatcher* dispatcher;
    TransportCapture cap;
} Harness;

static void harness_setup(Harness* h) {
    flipper_fake_reset();
    flipper_fake_add_dir("/ext");
    flipper_fake_add_file("/ext/hello.txt", "world");

    Storage* s = furi_record_open("storage");
    h->ctx = mtp_context_create(s);
    mtp_context_set_manufacturer(h->ctx, "Flipper Devices Inc.");
    mtp_context_set_model(h->ctx, "Flipper Zero");
    mtp_context_set_firmware_version(h->ctx, "1.0.0");
    mtp_context_set_serial(h->ctx, "TESTSERIAL");

    h->registry = mtp_operation_registry_create();
    mtp_register_all_operations(h->registry);

    h->transport = mtp_transport_create(NULL);
    transport_capture_reset(&h->cap);
    transport_capture_attach(&h->cap, h->transport);

    h->dispatcher = mtp_dispatcher_create(h->ctx, h->registry, h->transport);
}

static void harness_teardown(Harness* h) {
    mtp_dispatcher_destroy(h->dispatcher);
    mtp_transport_destroy(h->transport);
    mtp_operation_registry_destroy(h->registry);
    mtp_context_destroy(h->ctx);
}

static size_t
    make_command(uint8_t* buf, uint16_t op, uint32_t tx, const uint32_t* params, uint8_t pcount) {
    MTPContainerHeader h;
    size_t payload = (size_t)pcount * 4;
    mtp_container_header_init(&h, MTP_CONTAINER_TYPE_COMMAND, op, tx, (uint32_t)payload);
    mtp_container_header_write(&h, buf);
    for(uint8_t i = 0; i < pcount; i++) {
        uint32_t v = params[i];
        uint8_t* p = buf + 12 + i * 4;
        p[0] = (uint8_t)(v & 0xFF);
        p[1] = (uint8_t)((v >> 8) & 0xFF);
        p[2] = (uint8_t)((v >> 16) & 0xFF);
        p[3] = (uint8_t)((v >> 24) & 0xFF);
    }
    return 12 + payload;
}

// Search the captured stream for a container matching type+code+tx.
static bool find_container(
    const TransportCapture* cap,
    uint16_t type,
    uint16_t code,
    uint32_t tx,
    MTPContainerHeader* out_header) {
    for(size_t i = 0; i < cap->container_count; i++) {
        if(cap->container_sizes[i] < 12) continue;
        MTPContainerHeader h;
        if(!mtp_container_header_parse(
               &h, cap->sent_buffer + cap->container_offsets[i], cap->container_sizes[i]))
            continue;
        if(h.type == type && h.code == code && h.transaction_id == tx) {
            if(out_header) *out_header = h;
            return true;
        }
    }
    return false;
}

// --- tests ---------------------------------------------------------------

TEST(e2e_open_session_get_device_info_close) {
    Harness h;
    harness_setup(&h);

    uint8_t pkt[64];
    uint32_t params[1] = {1};
    size_t n = make_command(pkt, MTP_OP_OPEN_SESSION, 1, params, 1);
    ASSERT(mtp_dispatcher_handle_packet(h.dispatcher, pkt, n));
    ASSERT(mtp_context_is_session_open(h.ctx));

    transport_capture_reset(&h.cap);
    n = make_command(pkt, MTP_OP_GET_DEVICE_INFO, 2, NULL, 0);
    ASSERT(mtp_dispatcher_handle_packet(h.dispatcher, pkt, n));

    MTPContainerHeader hdr;
    ASSERT(find_container(&h.cap, MTP_CONTAINER_TYPE_DATA, MTP_OP_GET_DEVICE_INFO, 2, &hdr));
    ASSERT(find_container(&h.cap, MTP_CONTAINER_TYPE_RESPONSE, MTP_RESP_OK, 2, NULL));

    transport_capture_reset(&h.cap);
    n = make_command(pkt, MTP_OP_CLOSE_SESSION, 3, NULL, 0);
    ASSERT(mtp_dispatcher_handle_packet(h.dispatcher, pkt, n));
    ASSERT(!mtp_context_is_session_open(h.ctx));
    ASSERT(find_container(&h.cap, MTP_CONTAINER_TYPE_RESPONSE, MTP_RESP_OK, 3, NULL));

    harness_teardown(&h);
    TEST_PASS();
}

TEST(e2e_get_storage_ids) {
    Harness h;
    harness_setup(&h);

    uint8_t pkt[64];
    size_t n = make_command(pkt, MTP_OP_GET_STORAGE_IDS, 10, NULL, 0);
    ASSERT(mtp_dispatcher_handle_packet(h.dispatcher, pkt, n));
    ASSERT(find_container(&h.cap, MTP_CONTAINER_TYPE_DATA, MTP_OP_GET_STORAGE_IDS, 10, NULL));
    ASSERT(find_container(&h.cap, MTP_CONTAINER_TYPE_RESPONSE, MTP_RESP_OK, 10, NULL));

    harness_teardown(&h);
    TEST_PASS();
}

TEST(e2e_get_object_handles_lists_files) {
    Harness h;
    harness_setup(&h);

    uint8_t pkt[64];
    uint32_t params[3] = {MTP_STORAGE_ID_EXTERNAL, 0, 0xffffffff};
    size_t n = make_command(pkt, MTP_OP_GET_OBJECT_HANDLES, 20, params, 3);
    ASSERT(mtp_dispatcher_handle_packet(h.dispatcher, pkt, n));

    MTPContainerHeader data_hdr;
    ASSERT(
        find_container(&h.cap, MTP_CONTAINER_TYPE_DATA, MTP_OP_GET_OBJECT_HANDLES, 20, &data_hdr));
    ASSERT(find_container(&h.cap, MTP_CONTAINER_TYPE_RESPONSE, MTP_RESP_OK, 20, NULL));

    harness_teardown(&h);
    TEST_PASS();
}

TEST(e2e_get_object_returns_file_bytes) {
    Harness h;
    harness_setup(&h);

    // Populate the object index by walking the directory.
    uint8_t pkt[64];
    uint32_t handles_params[3] = {MTP_STORAGE_ID_EXTERNAL, 0, 0xffffffff};
    size_t n = make_command(pkt, MTP_OP_GET_OBJECT_HANDLES, 1, handles_params, 3);
    ASSERT(mtp_dispatcher_handle_packet(h.dispatcher, pkt, n));

    // Find the handle for /ext/hello.txt
    uint32_t target_handle = mtp_object_index_add(h.ctx->object_index, "/ext/hello.txt");
    ASSERT_NE(target_handle, (uint32_t)0);

    transport_capture_reset(&h.cap);
    uint32_t get_params[1] = {target_handle};
    n = make_command(pkt, MTP_OP_GET_OBJECT, 2, get_params, 1);
    ASSERT(mtp_dispatcher_handle_packet(h.dispatcher, pkt, n));

    MTPContainerHeader data_hdr;
    ASSERT(find_container(&h.cap, MTP_CONTAINER_TYPE_DATA, MTP_OP_GET_OBJECT, 2, &data_hdr));
    ASSERT_EQ(data_hdr.length, (uint32_t)(12 + 5)); // header + "world"
    ASSERT(find_container(&h.cap, MTP_CONTAINER_TYPE_RESPONSE, MTP_RESP_OK, 2, NULL));

    harness_teardown(&h);
    TEST_PASS();
}

TEST(e2e_delete_object_removes_file) {
    Harness h;
    harness_setup(&h);

    uint32_t handle = mtp_object_index_add(h.ctx->object_index, "/ext/hello.txt");

    uint8_t pkt[64];
    uint32_t params[1] = {handle};
    size_t n = make_command(pkt, MTP_OP_DELETE_OBJECT, 1, params, 1);
    ASSERT(mtp_dispatcher_handle_packet(h.dispatcher, pkt, n));
    ASSERT(find_container(&h.cap, MTP_CONTAINER_TYPE_RESPONSE, MTP_RESP_OK, 1, NULL));

    Storage* s = furi_record_open("storage");
    ASSERT(!storage_file_exists(s, "/ext/hello.txt"));

    harness_teardown(&h);
    TEST_PASS();
}

TEST(e2e_unsupported_op_returns_op_not_supported) {
    Harness h;
    harness_setup(&h);

    uint8_t pkt[16];
    size_t n = make_command(pkt, 0xFFFF, 99, NULL, 0);
    ASSERT(mtp_dispatcher_handle_packet(h.dispatcher, pkt, n));
    ASSERT(find_container(
        &h.cap, MTP_CONTAINER_TYPE_RESPONSE, MTP_RESP_OPERATION_NOT_SUPPORTED, 99, NULL));

    harness_teardown(&h);
    TEST_PASS();
}

int main(void) {
    printf("Running End-to-End MTP Tests\n");
    printf("============================\n\n");

    RUN_TEST(e2e_open_session_get_device_info_close);
    RUN_TEST(e2e_get_storage_ids);
    RUN_TEST(e2e_get_object_handles_lists_files);
    RUN_TEST(e2e_get_object_returns_file_bytes);
    RUN_TEST(e2e_delete_object_removes_file);
    RUN_TEST(e2e_unsupported_op_returns_op_not_supported);

    test_summary();
    return test_exit_code();
}
