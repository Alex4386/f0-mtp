#pragma once

#include "operation.h"
#include "operation_registry.h"
#include "transport.h"
#include "context.h"

// MTP dispatcher: ties together a transport, an operation registry, and an
// MTP context. It owns the per-request flow:
//
//   1. Receive a container from the transport
//   2. If COMMAND: look up handler in registry, invoke it, send DATA + RESPONSE
//   3. If DATA: hand off to the transfer state for the active transaction
//   4. If RESPONSE/EVENT: ignored (we are the responder)
//
// The dispatcher does NOT spawn threads — drive it from the USB worker loop
// by calling `mtp_dispatcher_handle_packet` whenever a bulk OUT packet
// arrives. This keeps the protocol layer testable without USB.

typedef struct MTPDispatcher MTPDispatcher;

MTPDispatcher*
    mtp_dispatcher_create(MTPContext* ctx, MTPOperationRegistry* registry, MTPTransport* transport);

void mtp_dispatcher_destroy(MTPDispatcher* d);

// Feed one bulk OUT packet (header + payload concatenated) to the dispatcher.
// Returns true if the packet was consumed (a response may or may not have
// been emitted via the transport's send callback).
bool mtp_dispatcher_handle_packet(MTPDispatcher* d, const uint8_t* buffer, size_t size);

// Build and send a response container with no data payload. Used by handlers
// that only need to ack with a code (e.g. OpenSession).
bool mtp_dispatcher_send_response(
    MTPDispatcher* d,
    uint32_t transaction_id,
    uint16_t response_code,
    const uint32_t* params,
    uint8_t param_count);

// Build and send a data container followed by a response container.
// `data`/`data_size` describe the data-phase payload.
bool mtp_dispatcher_send_data_and_response(
    MTPDispatcher* d,
    uint32_t transaction_id,
    uint16_t op_code,
    const uint8_t* data,
    size_t data_size,
    uint16_t response_code);

// Accessors (mostly for tests)
MTPContext* mtp_dispatcher_context(MTPDispatcher* d);
MTPOperationRegistry* mtp_dispatcher_registry(MTPDispatcher* d);
MTPTransport* mtp_dispatcher_transport(MTPDispatcher* d);
