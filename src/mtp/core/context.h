#pragma once

#include "mtp_types.h"
#include "object_index.h"
#include "error.h"

#ifdef FLIPPER_ZERO
#include <storage/storage.h>
#else
// For testing: use fake Flipper API
typedef struct Storage Storage;
#endif

// Forward declarations
typedef struct MTPTransferStateManager MTPTransferStateManager;

// Destructor for an MTPTransferStateManager. Set when the transfer state
// module is initialized via `mtp_transfer_state_manager_create()` so that
// context destruction can clean up the manager without a hard link
// dependency on transfer_state.c. Tests / modules that never use the
// transfer manager don't need to link transfer_state.c.
typedef void (*MTPTransferStateManagerDestroyFn)(MTPTransferStateManager*);

// MTP Context (Dependency Injection Container)
typedef struct MTPContext {
    // Flipper storage (direct API, no abstraction)
    Storage* storage;

    // Object index (handle ↔ path mapping)
    MTPObjectIndex* object_index;

    // Session state
    struct {
        uint32_t session_id;
        bool is_open;
    } session;

    // Transfer state manager (for multi-packet operations)
    MTPTransferStateManager* transfer_manager;
    // Set by transfer_state.c when a manager is created; called from
    // mtp_context_destroy.
    MTPTransferStateManagerDestroyFn transfer_manager_destroy;

    // Device information
    struct {
        const char* device_name;
        const char* manufacturer;
        const char* model;
        const char* serial;
        const char* firmware_version;
    } device_info;

    // Error context
    MTPErrorContext last_error;

    // User data (for platform-specific extensions)
    void* user_data;
} MTPContext;

// Context lifecycle
MTPContext* mtp_context_create(Storage* storage);
void mtp_context_destroy(MTPContext* ctx);

// Device info setters
void mtp_context_set_device_name(MTPContext* ctx, const char* name);
void mtp_context_set_manufacturer(MTPContext* ctx, const char* manufacturer);
void mtp_context_set_model(MTPContext* ctx, const char* model);
void mtp_context_set_serial(MTPContext* ctx, const char* serial);
void mtp_context_set_firmware_version(MTPContext* ctx, const char* version);

// Session management
void mtp_context_open_session(MTPContext* ctx, uint32_t session_id);
void mtp_context_close_session(MTPContext* ctx);
bool mtp_context_is_session_open(const MTPContext* ctx);

// Error helpers
#define MTP_CONTEXT_SET_ERROR(ctx, error_code, fmt, ...) \
    MTP_SET_ERROR(&(ctx)->last_error, error_code, fmt, ##__VA_ARGS__)

#define MTP_CONTEXT_CLEAR_ERROR(ctx) \
    MTP_CLEAR_ERROR(&(ctx)->last_error)

#define MTP_CONTEXT_HAS_ERROR(ctx) \
    MTP_HAS_ERROR(&(ctx)->last_error)

// Validation helpers
#define MTP_REQUIRE_SESSION(ctx, response) \
    if(!(ctx)->session.is_open) { \
        MTP_CONTEXT_SET_ERROR(ctx, MTP_ERROR_SESSION_NOT_OPEN, "Session not open"); \
        mtp_response_set_code(response, MTP_RESP_SESSION_NOT_OPEN); \
        return; \
    }

#define MTP_REQUIRE_VALID_HANDLE(ctx, handle, response) \
    if(!mtp_object_index_contains((ctx)->object_index, handle)) { \
        MTP_CONTEXT_SET_ERROR(ctx, MTP_ERROR_INVALID_HANDLE, "Invalid handle: %u", handle); \
        mtp_response_set_code(response, MTP_RESP_INVALID_OBJECT_HANDLE); \
        return; \
    }
