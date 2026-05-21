#pragma once

// Tiny in-memory "USB" used by test_transport.c. It records bytes the
// MTPTransport's send callback emits and feeds canned bytes back to its
// receive callback.

#include "../../src/mtp/core/transport.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define USB_FAKE_BUF_SIZE 4096

typedef struct {
    uint8_t sent[USB_FAKE_BUF_SIZE];
    size_t sent_len;
    int send_call_count;
    bool fail_send;

    uint8_t recv[USB_FAKE_BUF_SIZE];
    size_t recv_len;
    size_t recv_pos;
    bool fail_receive;
} UsbFake;

void usb_fake_reset(UsbFake* u);

// Wire up an MTPTransport to operate against `u`.
void usb_fake_attach(UsbFake* u, MTPTransport* transport);

// Push bytes that the next mtp_transport_receive_container call will see.
void usb_fake_push_recv(UsbFake* u, const uint8_t* data, size_t size);
