#include "../framework/test.h"
#include "../mocks/usb_fake.h"
#include "../../src/mtp/core/transport.h"
#include "../../src/mtp/core/mtp_types.h"
#include <string.h>

TEST(transport_create_destroy) {
    MTPTransport* t = mtp_transport_create(NULL);
    ASSERT_NOT_NULL(t);
    ASSERT(!mtp_transport_is_connected(t));
    mtp_transport_destroy(t);
    TEST_PASS();
}

TEST(transport_connection) {
    MTPTransport* t = mtp_transport_create(NULL);
    ASSERT(!mtp_transport_is_connected(t));
    mtp_transport_connect(t);
    ASSERT(mtp_transport_is_connected(t));
    mtp_transport_disconnect(t);
    ASSERT(!mtp_transport_is_connected(t));
    mtp_transport_destroy(t);
    TEST_PASS();
}

TEST(transport_set_callbacks) {
    MTPTransport* t = mtp_transport_create(NULL);
    UsbFake u;
    usb_fake_reset(&u);
    usb_fake_attach(&u, t);
    ASSERT(mtp_transport_is_connected(t));
    mtp_transport_destroy(t);
    TEST_PASS();
}

TEST(transport_header_parse) {
    uint8_t buf[12];
    // little-endian: length=12, type=1, code=0x1001, transaction_id=42
    buf[0] = 12;
    buf[1] = 0;
    buf[2] = 0;
    buf[3] = 0;
    buf[4] = 1;
    buf[5] = 0;
    buf[6] = 0x01;
    buf[7] = 0x10;
    buf[8] = 42;
    buf[9] = 0;
    buf[10] = 0;
    buf[11] = 0;

    MTPContainerHeader h;
    ASSERT(mtp_container_header_parse(&h, buf, sizeof(buf)));
    ASSERT_EQ(h.length, (uint32_t)12);
    ASSERT_EQ(h.type, (uint16_t)1);
    ASSERT_EQ(h.code, (uint16_t)0x1001);
    ASSERT_EQ(h.transaction_id, (uint32_t)42);
    TEST_PASS();
}

TEST(transport_header_write) {
    MTPContainerHeader h;
    mtp_container_header_init(&h, MTP_CONTAINER_TYPE_COMMAND, 0x1001, 42, 0);

    uint8_t buf[12];
    mtp_container_header_write(&h, buf);

    MTPContainerHeader parsed;
    ASSERT(mtp_container_header_parse(&parsed, buf, sizeof(buf)));
    ASSERT_EQ(parsed.length, (uint32_t)12);
    ASSERT_EQ(parsed.type, (uint16_t)MTP_CONTAINER_TYPE_COMMAND);
    ASSERT_EQ(parsed.code, (uint16_t)0x1001);
    ASSERT_EQ(parsed.transaction_id, (uint32_t)42);
    TEST_PASS();
}

TEST(transport_send_container) {
    MTPTransport* t = mtp_transport_create(NULL);
    UsbFake u;
    usb_fake_reset(&u);
    usb_fake_attach(&u, t);

    MTPContainerHeader h;
    mtp_container_header_init(&h, MTP_CONTAINER_TYPE_RESPONSE, MTP_RESP_OK, 7, 0);
    ASSERT(mtp_transport_send_container(t, &h, NULL, 0));

    // 12-byte header only
    ASSERT_EQ(u.sent_len, (size_t)12);

    MTPContainerHeader parsed;
    ASSERT(mtp_container_header_parse(&parsed, u.sent, u.sent_len));
    ASSERT_EQ(parsed.code, (uint16_t)MTP_RESP_OK);
    ASSERT_EQ(parsed.transaction_id, (uint32_t)7);

    mtp_transport_destroy(t);
    TEST_PASS();
}

TEST(transport_send_with_payload) {
    MTPTransport* t = mtp_transport_create(NULL);
    UsbFake u;
    usb_fake_reset(&u);
    usb_fake_attach(&u, t);

    uint8_t payload[] = {1, 2, 3, 4};
    MTPContainerHeader h;
    mtp_container_header_init(
        &h, MTP_CONTAINER_TYPE_DATA, MTP_OP_GET_DEVICE_INFO, 11, sizeof(payload));

    ASSERT(mtp_transport_send_container(t, &h, payload, sizeof(payload)));
    ASSERT_EQ(u.sent_len, (size_t)(12 + 4));
    ASSERT_EQ(u.sent[12], (uint8_t)1);
    ASSERT_EQ(u.sent[13], (uint8_t)2);
    ASSERT_EQ(u.sent[14], (uint8_t)3);
    ASSERT_EQ(u.sent[15], (uint8_t)4);

    mtp_transport_destroy(t);
    TEST_PASS();
}

TEST(transport_receive_container) {
    MTPTransport* t = mtp_transport_create(NULL);
    UsbFake u;
    usb_fake_reset(&u);
    usb_fake_attach(&u, t);

    // Build a 12-byte header (no payload)
    MTPContainerHeader h;
    mtp_container_header_init(&h, MTP_CONTAINER_TYPE_COMMAND, MTP_OP_OPEN_SESSION, 99, 0);
    uint8_t buf[12];
    mtp_container_header_write(&h, buf);
    usb_fake_push_recv(&u, buf, sizeof(buf));

    MTPContainerHeader recv;
    size_t payload_received = 12345;
    ASSERT(mtp_transport_receive_container(t, &recv, NULL, 0, &payload_received));
    ASSERT_EQ(recv.code, (uint16_t)MTP_OP_OPEN_SESSION);
    ASSERT_EQ(recv.transaction_id, (uint32_t)99);
    ASSERT_EQ(payload_received, (size_t)0);

    mtp_transport_destroy(t);
    TEST_PASS();
}

TEST(transport_receive_with_payload) {
    MTPTransport* t = mtp_transport_create(NULL);
    UsbFake u;
    usb_fake_reset(&u);
    usb_fake_attach(&u, t);

    uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
    MTPContainerHeader h;
    mtp_container_header_init(
        &h, MTP_CONTAINER_TYPE_DATA, MTP_OP_GET_DEVICE_INFO, 17, sizeof(payload));

    uint8_t hbuf[12];
    mtp_container_header_write(&h, hbuf);
    usb_fake_push_recv(&u, hbuf, sizeof(hbuf));
    usb_fake_push_recv(&u, payload, sizeof(payload));

    MTPContainerHeader recv;
    uint8_t got_payload[16];
    size_t got_size = 0;
    ASSERT(mtp_transport_receive_container(t, &recv, got_payload, sizeof(got_payload), &got_size));
    ASSERT_EQ(recv.code, (uint16_t)MTP_OP_GET_DEVICE_INFO);
    ASSERT_EQ(got_size, sizeof(payload));
    ASSERT_EQ(got_payload[0], (uint8_t)0xDE);
    ASSERT_EQ(got_payload[3], (uint8_t)0xEF);

    mtp_transport_destroy(t);
    TEST_PASS();
}

TEST(transport_send_failure) {
    MTPTransport* t = mtp_transport_create(NULL);
    UsbFake u;
    usb_fake_reset(&u);
    usb_fake_attach(&u, t);
    u.fail_send = true;

    MTPContainerHeader h;
    mtp_container_header_init(&h, MTP_CONTAINER_TYPE_RESPONSE, MTP_RESP_OK, 7, 0);
    ASSERT(!mtp_transport_send_container(t, &h, NULL, 0));

    mtp_transport_destroy(t);
    TEST_PASS();
}

TEST(transport_receive_failure) {
    MTPTransport* t = mtp_transport_create(NULL);
    UsbFake u;
    usb_fake_reset(&u);
    usb_fake_attach(&u, t);
    u.fail_receive = true;

    MTPContainerHeader recv;
    ASSERT(!mtp_transport_receive_container(t, &recv, NULL, 0, NULL));

    mtp_transport_destroy(t);
    TEST_PASS();
}

int main(void) {
    printf("Running Transport Layer Tests\n");
    printf("==============================\n\n");

    RUN_TEST(transport_create_destroy);
    RUN_TEST(transport_connection);
    RUN_TEST(transport_set_callbacks);
    RUN_TEST(transport_header_parse);
    RUN_TEST(transport_header_write);
    RUN_TEST(transport_send_container);
    RUN_TEST(transport_send_with_payload);
    RUN_TEST(transport_receive_container);
    RUN_TEST(transport_receive_with_payload);
    RUN_TEST(transport_send_failure);
    RUN_TEST(transport_receive_failure);

    test_summary();
    return test_exit_code();
}
