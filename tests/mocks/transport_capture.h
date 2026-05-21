#pragma once

// In-memory MTPTransport mock that captures every send and serves canned
// receives. Lets us unit-test the dispatcher and operation handlers without
// any USB plumbing.

#include "../../src/mtp/core/transport.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define TRANSPORT_CAPTURE_MAX_BYTES (64 * 1024)
#define TRANSPORT_CAPTURE_MAX_CONTAINERS 32

typedef struct {
    uint8_t sent_buffer[TRANSPORT_CAPTURE_MAX_BYTES];
    size_t sent_bytes;

    // Offsets and sizes of each "send" boundary, so tests can see how the
    // dispatcher framed the output into header + payload + ...
    size_t container_offsets[TRANSPORT_CAPTURE_MAX_CONTAINERS];
    size_t container_sizes[TRANSPORT_CAPTURE_MAX_CONTAINERS];
    size_t container_count;

    // Whether the send callback should pretend to fail
    bool fail_send;

    // Recv: optional canned buffer the transport will hand back when asked
    const uint8_t* recv_buffer;
    size_t recv_size;
    size_t recv_offset;
} TransportCapture;

void transport_capture_reset(TransportCapture* cap);

// Wire `cap` into `transport` so subsequent send/recv go through it.
void transport_capture_attach(TransportCapture* cap, MTPTransport* transport);

// Set canned receive data (consumed by mtp_transport_receive_container).
void transport_capture_set_recv(TransportCapture* cap, const uint8_t* data, size_t size);
