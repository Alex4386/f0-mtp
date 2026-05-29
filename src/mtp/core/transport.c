#include "transport.h"
#include <stdlib.h>
#include <string.h>

// Create transport
MTPTransport* mtp_transport_create(void* usb_context) {
    MTPTransport* transport = calloc(1, sizeof(MTPTransport));
    if(!transport) {
        return NULL;
    }

    transport->usb_context = usb_context;
    transport->connected = false;

    // Callbacks set later
    transport->receive = NULL;
    transport->send = NULL;
    transport->flush = NULL;

    // Clear statistics
    transport->packets_sent = 0;
    transport->packets_received = 0;
    transport->bytes_sent = 0;
    transport->bytes_received = 0;

    return transport;
}

void mtp_transport_destroy(MTPTransport* transport) {
    if(!transport) {
        return;
    }

    // Note: usb_context not freed (owned by caller)
    free(transport);
}

void mtp_transport_set_callbacks(
    MTPTransport* transport,
    MTPTransportReceiveCallback receive,
    MTPTransportSendCallback send,
    MTPTransportFlushCallback flush) {
    if(!transport) {
        return;
    }

    transport->receive = receive;
    transport->send = send;
    transport->flush = flush;
}

void mtp_transport_connect(MTPTransport* transport) {
    if(transport) {
        transport->connected = true;
    }
}

void mtp_transport_disconnect(MTPTransport* transport) {
    if(transport) {
        transport->connected = false;
    }
}

bool mtp_transport_is_connected(const MTPTransport* transport) {
    return transport ? transport->connected : false;
}

// Receive container from USB
bool mtp_transport_receive_container(
    MTPTransport* transport,
    MTPContainerHeader* header,
    uint8_t* payload,
    size_t max_payload_size,
    size_t* payload_received) {
    if(!transport || !transport->receive || !header) {
        return false;
    }

    // Read header (12 bytes)
    uint8_t header_buf[sizeof(MTPContainerHeader)];
    size_t received = 0;

    if(!transport->receive(transport, header_buf, sizeof(header_buf), &received)) {
        return false;
    }

    if(received != sizeof(header_buf)) {
        return false;
    }

    // Parse header
    if(!mtp_container_header_parse(header, header_buf, sizeof(header_buf))) {
        return false;
    }

    transport->packets_received++;
    transport->bytes_received += received;

    // Calculate payload size
    size_t expected_payload = header->length - sizeof(MTPContainerHeader);

    if(expected_payload > max_payload_size) {
        return false; // Payload too large
    }

    // Read payload if present
    if(expected_payload > 0 && payload) {
        if(!transport->receive(transport, payload, expected_payload, &received)) {
            return false;
        }

        if(received != expected_payload) {
            return false;
        }

        transport->bytes_received += received;

        if(payload_received) {
            *payload_received = received;
        }
    } else {
        if(payload_received) {
            *payload_received = 0;
        }
    }

    return true;
}

// Send container to USB
bool mtp_transport_send_container(
    MTPTransport* transport,
    const MTPContainerHeader* header,
    const uint8_t* payload,
    size_t payload_size) {
    if(!transport || !transport->send || !header) {
        return false;
    }

    // Validate header length matches payload
    size_t expected_length = sizeof(MTPContainerHeader) + payload_size;
    if(header->length != expected_length) {
        return false;
    }

    // Write header
    uint8_t header_buf[sizeof(MTPContainerHeader)];
    mtp_container_header_write(header, header_buf);

    if(!transport->send(transport, header_buf, sizeof(header_buf))) {
        return false;
    }

    transport->packets_sent++;
    transport->bytes_sent += sizeof(header_buf);

    // Write payload if present
    if(payload_size > 0 && payload) {
        if(!transport->send(transport, payload, payload_size)) {
            return false;
        }

        transport->bytes_sent += payload_size;
    }

    // Flush
    if(transport->flush) {
        transport->flush(transport);
    }

    return true;
}

// Parse container header from buffer (little-endian)
bool mtp_container_header_parse(MTPContainerHeader* header, const uint8_t* buffer, size_t size) {
    if(!header || !buffer || size < sizeof(MTPContainerHeader)) {
        return false;
    }

    header->length = (uint32_t)buffer[0] | ((uint32_t)buffer[1] << 8) |
                     ((uint32_t)buffer[2] << 16) | ((uint32_t)buffer[3] << 24);

    header->type = (uint16_t)buffer[4] | ((uint16_t)buffer[5] << 8);

    header->code = (uint16_t)buffer[6] | ((uint16_t)buffer[7] << 8);

    header->transaction_id = (uint32_t)buffer[8] | ((uint32_t)buffer[9] << 8) |
                             ((uint32_t)buffer[10] << 16) | ((uint32_t)buffer[11] << 24);

    return true;
}

// Write container header to buffer (little-endian)
void mtp_container_header_write(const MTPContainerHeader* header, uint8_t* buffer) {
    if(!header || !buffer) {
        return;
    }

    buffer[0] = (uint8_t)(header->length & 0xFF);
    buffer[1] = (uint8_t)((header->length >> 8) & 0xFF);
    buffer[2] = (uint8_t)((header->length >> 16) & 0xFF);
    buffer[3] = (uint8_t)((header->length >> 24) & 0xFF);

    buffer[4] = (uint8_t)(header->type & 0xFF);
    buffer[5] = (uint8_t)((header->type >> 8) & 0xFF);

    buffer[6] = (uint8_t)(header->code & 0xFF);
    buffer[7] = (uint8_t)((header->code >> 8) & 0xFF);

    buffer[8] = (uint8_t)(header->transaction_id & 0xFF);
    buffer[9] = (uint8_t)((header->transaction_id >> 8) & 0xFF);
    buffer[10] = (uint8_t)((header->transaction_id >> 16) & 0xFF);
    buffer[11] = (uint8_t)((header->transaction_id >> 24) & 0xFF);
}
