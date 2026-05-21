#include "transport_capture.h"
#include <string.h>

static bool capture_send(MTPTransport* transport, const uint8_t* buffer, size_t size) {
    TransportCapture* cap = (TransportCapture*)transport->usb_context;
    if(!cap || cap->fail_send) return false;
    if(cap->sent_bytes + size > TRANSPORT_CAPTURE_MAX_BYTES) return false;
    if(cap->container_count >= TRANSPORT_CAPTURE_MAX_CONTAINERS) return false;

    cap->container_offsets[cap->container_count] = cap->sent_bytes;
    cap->container_sizes[cap->container_count] = size;
    cap->container_count++;

    memcpy(cap->sent_buffer + cap->sent_bytes, buffer, size);
    cap->sent_bytes += size;
    return true;
}

static bool
    capture_receive(MTPTransport* transport, uint8_t* buffer, size_t size, size_t* received) {
    TransportCapture* cap = (TransportCapture*)transport->usb_context;
    if(!cap || !cap->recv_buffer) return false;
    size_t remaining = cap->recv_size - cap->recv_offset;
    if(remaining < size) return false;

    memcpy(buffer, cap->recv_buffer + cap->recv_offset, size);
    cap->recv_offset += size;
    if(received) *received = size;
    return true;
}

static void capture_flush(MTPTransport* transport) {
    (void)transport;
}

void transport_capture_reset(TransportCapture* cap) {
    memset(cap, 0, sizeof(*cap));
}

void transport_capture_attach(TransportCapture* cap, MTPTransport* transport) {
    transport->usb_context = cap;
    mtp_transport_set_callbacks(transport, capture_receive, capture_send, capture_flush);
    mtp_transport_connect(transport);
}

void transport_capture_set_recv(TransportCapture* cap, const uint8_t* data, size_t size) {
    cap->recv_buffer = data;
    cap->recv_size = size;
    cap->recv_offset = 0;
}
