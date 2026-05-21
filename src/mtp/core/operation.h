#pragma once

#include "mtp_types.h"

// Forward declarations
typedef struct MTPContext MTPContext;

// MTP Request structure
typedef struct {
    uint16_t op_code;
    uint32_t transaction_id;
    uint32_t params[5];

    // For data phase operations
    const uint8_t* data;
    size_t data_size;
} MTPRequest;

// MTP Response structure
typedef struct {
    uint16_t response_code;  // MTP_RESP_OK, etc.
    uint32_t params[5];
    uint8_t param_count;

    // Data response (owned by response, will be freed)
    uint8_t* data;
    size_t data_size;

    // Streaming response (callback-based for large data)
    bool is_streaming;
    void* stream_context;
    int (*stream_callback)(void* ctx, uint8_t* buffer, size_t size);
    size_t stream_total_size;
} MTPResponse;

// Operation handler function signature
typedef void (*MTPOperationHandler)(
    MTPContext* ctx,
    const MTPRequest* request,
    MTPResponse* response
);

// Operation registry entry
typedef struct {
    uint16_t op_code;
    const char* name;
    MTPOperationHandler handler;
} MTPOperationEntry;

// Response management
MTPResponse* mtp_response_create(void);
void mtp_response_destroy(MTPResponse* response);

void mtp_response_set_code(MTPResponse* response, uint16_t code);
void mtp_response_add_param(MTPResponse* response, uint32_t param);
void mtp_response_set_params(MTPResponse* response, const uint32_t* params, uint8_t count);
void mtp_response_set_data(MTPResponse* response, uint8_t* data, size_t size);
void mtp_response_set_stream(
    MTPResponse* response,
    void* context,
    int (*callback)(void* ctx, uint8_t* buffer, size_t size),
    size_t total_size
);

// Helper macro for cleaner operation handler definitions
#define MTP_OPERATION_HANDLER(name) \
    void mtp_op_##name(MTPContext* ctx, const MTPRequest* request, MTPResponse* response)

// Helper macro for operation entry definition
#define MTP_OPERATION_ENTRY(op_code, name) \
    const MTPOperationEntry mtp_op_entry_##name = { \
        .op_code = op_code, \
        .name = #name, \
        .handler = mtp_op_##name \
    }
