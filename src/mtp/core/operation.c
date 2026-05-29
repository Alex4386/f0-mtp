#include "operation.h"
#include <stdlib.h>
#include <string.h>

MTPResponse* mtp_response_create(void) {
    MTPResponse* response = calloc(1, sizeof(MTPResponse));
    if(!response) {
        return NULL;
    }

    response->response_code = MTP_RESP_UNDEFINED;
    return response;
}

void mtp_response_destroy(MTPResponse* response) {
    if(!response) {
        return;
    }

    // Free data buffer if allocated
    if(response->data) {
        free(response->data);
    }

    // Note: stream_context is NOT freed here
    // The operation handler is responsible for cleanup via destroy callback

    free(response);
}

void mtp_response_set_code(MTPResponse* response, uint16_t code) {
    if(response) {
        response->response_code = code;
    }
}

void mtp_response_add_param(MTPResponse* response, uint32_t param) {
    if(!response || response->param_count >= 5) {
        return;
    }

    response->params[response->param_count++] = param;
}

void mtp_response_set_params(MTPResponse* response, const uint32_t* params, uint8_t count) {
    if(!response || !params || count > 5) {
        return;
    }

    memcpy(response->params, params, sizeof(uint32_t) * count);
    response->param_count = count;
}

void mtp_response_set_data(MTPResponse* response, uint8_t* data, size_t size) {
    if(!response) {
        return;
    }

    // Free existing data if any
    if(response->data) {
        free(response->data);
    }

    response->data = data;
    response->data_size = size;
    response->is_streaming = false;
}

void mtp_response_set_stream(
    MTPResponse* response,
    void* context,
    int (*callback)(void* ctx, uint8_t* buffer, size_t size),
    size_t total_size) {
    if(!response || !callback) {
        return;
    }

    response->is_streaming = true;
    response->stream_context = context;
    response->stream_callback = callback;
    response->stream_total_size = total_size;

    // Clear any existing data buffer
    if(response->data) {
        free(response->data);
        response->data = NULL;
        response->data_size = 0;
    }
}
