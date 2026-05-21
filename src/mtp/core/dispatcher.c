#include "dispatcher.h"
#include <stdlib.h>
#include <string.h>

struct MTPDispatcher {
    MTPContext* ctx;
    MTPOperationRegistry* registry;
    MTPTransport* transport;
};

MTPDispatcher* mtp_dispatcher_create(
    MTPContext* ctx,
    MTPOperationRegistry* registry,
    MTPTransport* transport) {
    if(!ctx || !registry || !transport) return NULL;

    MTPDispatcher* d = calloc(1, sizeof(MTPDispatcher));
    if(!d) return NULL;

    d->ctx = ctx;
    d->registry = registry;
    d->transport = transport;
    return d;
}

void mtp_dispatcher_destroy(MTPDispatcher* d) {
    free(d);
}

MTPContext* mtp_dispatcher_context(MTPDispatcher* d) {
    return d ? d->ctx : NULL;
}

MTPOperationRegistry* mtp_dispatcher_registry(MTPDispatcher* d) {
    return d ? d->registry : NULL;
}

MTPTransport* mtp_dispatcher_transport(MTPDispatcher* d) {
    return d ? d->transport : NULL;
}

bool mtp_dispatcher_send_response(
    MTPDispatcher* d,
    uint32_t transaction_id,
    uint16_t response_code,
    const uint32_t* params,
    uint8_t param_count) {
    if(!d) return false;
    if(param_count > 5) param_count = 5;

    uint8_t payload[5 * sizeof(uint32_t)];
    size_t payload_size = (size_t)param_count * sizeof(uint32_t);

    if(param_count > 0 && params) {
        for(uint8_t i = 0; i < param_count; i++) {
            uint32_t v = params[i];
            payload[i * 4 + 0] = (uint8_t)(v & 0xFF);
            payload[i * 4 + 1] = (uint8_t)((v >> 8) & 0xFF);
            payload[i * 4 + 2] = (uint8_t)((v >> 16) & 0xFF);
            payload[i * 4 + 3] = (uint8_t)((v >> 24) & 0xFF);
        }
    }

    MTPContainerHeader header;
    mtp_container_header_init(
        &header,
        MTP_CONTAINER_TYPE_RESPONSE,
        response_code,
        transaction_id,
        (uint32_t)payload_size);

    return mtp_transport_send_container(
        d->transport, &header, payload_size > 0 ? payload : NULL, payload_size);
}

bool mtp_dispatcher_send_data_and_response(
    MTPDispatcher* d,
    uint32_t transaction_id,
    uint16_t op_code,
    const uint8_t* data,
    size_t data_size,
    uint16_t response_code) {
    if(!d) return false;

    MTPContainerHeader data_header;
    mtp_container_header_init(
        &data_header, MTP_CONTAINER_TYPE_DATA, op_code, transaction_id, (uint32_t)data_size);

    if(!mtp_transport_send_container(d->transport, &data_header, data, data_size)) {
        return false;
    }

    return mtp_dispatcher_send_response(d, transaction_id, response_code, NULL, 0);
}

// Build an MTPRequest from a parsed command container.
static void
    build_request(MTPRequest* req, const MTPContainerHeader* header, const uint8_t* payload, size_t payload_size) {
    req->op_code = header->code;
    req->transaction_id = header->transaction_id;
    req->data = NULL;
    req->data_size = 0;
    memset(req->params, 0, sizeof(req->params));

    // Command containers carry up to 5 uint32 parameters in the payload.
    size_t available = payload_size / 4;
    if(available > 5) available = 5;
    for(size_t i = 0; i < available; i++) {
        const uint8_t* p = payload + i * 4;
        req->params[i] = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
                         ((uint32_t)p[3] << 24);
    }
}

// Emit a response built into MTPResponse via the transport.
static bool emit_response(MTPDispatcher* d, const MTPRequest* req, MTPResponse* resp) {
    // Data phase
    if(resp->data && resp->data_size > 0) {
        MTPContainerHeader data_header;
        mtp_container_header_init(
            &data_header,
            MTP_CONTAINER_TYPE_DATA,
            req->op_code,
            req->transaction_id,
            (uint32_t)resp->data_size);
        if(!mtp_transport_send_container(d->transport, &data_header, resp->data, resp->data_size)) {
            return false;
        }
    }

    // Response phase
    return mtp_dispatcher_send_response(
        d, req->transaction_id, resp->response_code, resp->params, resp->param_count);
}

bool mtp_dispatcher_handle_packet(MTPDispatcher* d, const uint8_t* buffer, size_t size) {
    if(!d || !buffer || size < sizeof(MTPContainerHeader)) return false;

    MTPContainerHeader header;
    if(!mtp_container_header_parse(&header, buffer, size)) return false;

    const uint8_t* payload = buffer + sizeof(MTPContainerHeader);
    size_t total = (size_t)header.length;
    if(total < sizeof(MTPContainerHeader)) return false;
    size_t payload_size = total - sizeof(MTPContainerHeader);
    if(payload_size > size - sizeof(MTPContainerHeader)) {
        // Multi-packet container — caller must accumulate. For now we reject;
        // the transfer-state manager (todo #7) will own this case.
        payload_size = size - sizeof(MTPContainerHeader);
    }

    if(header.type == MTP_CONTAINER_TYPE_COMMAND) {
        MTPOperationHandler handler = mtp_operation_registry_find(d->registry, header.code);
        if(!handler) {
            return mtp_dispatcher_send_response(
                d, header.transaction_id, MTP_RESP_OPERATION_NOT_SUPPORTED, NULL, 0);
        }

        MTPRequest req;
        build_request(&req, &header, payload, payload_size);

        MTPResponse* resp = mtp_response_create();
        if(!resp) {
            return mtp_dispatcher_send_response(
                d, header.transaction_id, MTP_RESP_GENERAL_ERROR, NULL, 0);
        }

        handler(d->ctx, &req, resp);

        // If handler did not set a response code, default to GENERAL_ERROR so we
        // never silently leave the host hanging.
        if(resp->response_code == MTP_RESP_UNDEFINED) {
            resp->response_code = MTP_RESP_GENERAL_ERROR;
        }

        bool ok = emit_response(d, &req, resp);
        mtp_response_destroy(resp);
        return ok;
    }

    // DATA/RESPONSE/EVENT for ongoing multi-packet operations will be handled
    // by the transfer state manager (Phase B). For now: ignore politely.
    return true;
}
