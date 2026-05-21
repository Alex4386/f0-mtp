#include "usb_fake.h"
#include <string.h>

static bool fake_send(MTPTransport* transport, const uint8_t* buffer, size_t size) {
    UsbFake* u = transport->usb_context;
    if(!u || u->fail_send) return false;
    if(u->sent_len + size > USB_FAKE_BUF_SIZE) return false;
    memcpy(u->sent + u->sent_len, buffer, size);
    u->sent_len += size;
    u->send_call_count++;
    return true;
}

static bool fake_receive(MTPTransport* transport, uint8_t* buffer, size_t size, size_t* received) {
    UsbFake* u = transport->usb_context;
    if(!u || u->fail_receive) return false;
    if(u->recv_pos + size > u->recv_len) return false;
    memcpy(buffer, u->recv + u->recv_pos, size);
    u->recv_pos += size;
    if(received) *received = size;
    return true;
}

static void fake_flush(MTPTransport* transport) {
    (void)transport;
}

void usb_fake_reset(UsbFake* u) {
    memset(u, 0, sizeof(*u));
}

void usb_fake_attach(UsbFake* u, MTPTransport* transport) {
    transport->usb_context = u;
    mtp_transport_set_callbacks(transport, fake_receive, fake_send, fake_flush);
    mtp_transport_connect(transport);
}

void usb_fake_push_recv(UsbFake* u, const uint8_t* data, size_t size) {
    if(u->recv_len + size > USB_FAKE_BUF_SIZE) return;
    memcpy(u->recv + u->recv_len, data, size);
    u->recv_len += size;
}
