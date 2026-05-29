#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// MTP USB Transport Layer
// Simple wrapper around USB operations for MTP protocol

// MTP container header (12 bytes)
typedef struct {
    uint32_t length; // Total container length including header
    uint16_t type; // Container type (command, data, response)
    uint16_t code; // Operation/Response/Event code
    uint32_t transaction_id; // Transaction ID
} MTPContainerHeader;

// Container types
#define MTP_CONTAINER_TYPE_UNDEFINED 0x0000
#define MTP_CONTAINER_TYPE_COMMAND   0x0001
#define MTP_CONTAINER_TYPE_DATA      0x0002
#define MTP_CONTAINER_TYPE_RESPONSE  0x0003
#define MTP_CONTAINER_TYPE_EVENT     0x0004

// USB packet buffer
#define MTP_USB_PACKET_SIZE 512
#define MTP_USB_MAX_PAYLOAD (64 * 1024) // 64KB max payload

// Transport callbacks
typedef struct MTPTransport MTPTransport;

typedef bool (*MTPTransportReceiveCallback)(
    MTPTransport* transport,
    uint8_t* buffer,
    size_t size,
    size_t* received);
typedef bool (
    *MTPTransportSendCallback)(MTPTransport* transport, const uint8_t* buffer, size_t size);
typedef void (*MTPTransportFlushCallback)(MTPTransport* transport);

// Transport structure
struct MTPTransport {
    // USB context (platform-specific, e.g., Flipper's FuriHalUsb)
    void* usb_context;

    // Callbacks
    MTPTransportReceiveCallback receive;
    MTPTransportSendCallback send;
    MTPTransportFlushCallback flush;

    // State
    bool connected;

    // Statistics
    uint32_t packets_sent;
    uint32_t packets_received;
    uint32_t bytes_sent;
    uint32_t bytes_received;
};

// Transport lifecycle
MTPTransport* mtp_transport_create(void* usb_context);
void mtp_transport_destroy(MTPTransport* transport);

// Set callbacks
void mtp_transport_set_callbacks(
    MTPTransport* transport,
    MTPTransportReceiveCallback receive,
    MTPTransportSendCallback send,
    MTPTransportFlushCallback flush);

// Connection management
void mtp_transport_connect(MTPTransport* transport);
void mtp_transport_disconnect(MTPTransport* transport);
bool mtp_transport_is_connected(const MTPTransport* transport);

// Container operations
bool mtp_transport_receive_container(
    MTPTransport* transport,
    MTPContainerHeader* header,
    uint8_t* payload,
    size_t max_payload_size,
    size_t* payload_received);

bool mtp_transport_send_container(
    MTPTransport* transport,
    const MTPContainerHeader* header,
    const uint8_t* payload,
    size_t payload_size);

// Helper: Create container header
static inline void mtp_container_header_init(
    MTPContainerHeader* header,
    uint16_t type,
    uint16_t code,
    uint32_t transaction_id,
    uint32_t payload_size) {
    header->length = sizeof(MTPContainerHeader) + payload_size;
    header->type = type;
    header->code = code;
    header->transaction_id = transaction_id;
}

// Helper: Parse container header from buffer
bool mtp_container_header_parse(MTPContainerHeader* header, const uint8_t* buffer, size_t size);

// Helper: Write container header to buffer
void mtp_container_header_write(const MTPContainerHeader* header, uint8_t* buffer);
