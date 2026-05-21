#pragma once

#include "context.h"
#include "operation.h"

// Multi-packet transfer state machine.
//
// MTP's SendObjectInfo and SendObject are split across two USB transactions:
//   1. COMMAND container with op_code + params (handled by an operation
//      handler, which stashes the params into the active transfer)
//   2. DATA container(s) carrying the payload (handled by feed_data())
//
// The 64KB-crash bug in the original implementation came from buffering the
// entire SEND_OBJECT payload before writing it to disk. This manager streams
// bytes directly into storage_file_write() as they arrive, so the only
// memory used per transfer is one fixed-size receive buffer plus the
// MTP_NAME_SIZE staging buffer for SendObjectInfo.

typedef enum {
    MTP_TRANSFER_NONE = 0,
    MTP_TRANSFER_SEND_OBJECT_INFO, // collecting the ObjectInfo dataset
    MTP_TRANSFER_SEND_OBJECT, // streaming bytes into an open File*
} MTPTransferKind;

typedef struct MTPTransferStateManager MTPTransferStateManager;

// --- Lifecycle ------------------------------------------------------------

MTPTransferStateManager* mtp_transfer_state_manager_create(void);
void mtp_transfer_state_manager_destroy(MTPTransferStateManager* mgr);

// --- State queries --------------------------------------------------------

MTPTransferKind mtp_transfer_state_kind(const MTPTransferStateManager* mgr);
uint32_t mtp_transfer_state_transaction_id(const MTPTransferStateManager* mgr);
uint32_t mtp_transfer_state_bytes_remaining(const MTPTransferStateManager* mgr);

// --- COMMAND-phase hooks --------------------------------------------------

// Called by the SendObjectInfo handler to begin collecting the dataset.
void mtp_transfer_state_begin_send_object_info(
    MTPTransferStateManager* mgr,
    uint32_t transaction_id,
    uint32_t storage_id,
    uint32_t parent_handle);

// Called by the SendObject handler to begin streaming. `total_size` is the
// payload size taken from the DATA container's header.
//
// Returns false if no previous SendObjectInfo created a target path.
bool mtp_transfer_state_begin_send_object(
    MTPTransferStateManager* mgr,
    MTPContext* ctx,
    uint32_t transaction_id);

// --- DATA-phase ingest ----------------------------------------------------

// Feed payload bytes that just arrived for the active DATA container.
// `total_payload_size` is the value taken from the DATA container header
// (only used on the first call to know when we're done).
//
// Returns true on success, false on any I/O failure. When the transfer
// completes (all bytes received), `*completed` is set to true.
bool mtp_transfer_state_feed_data(
    MTPTransferStateManager* mgr,
    MTPContext* ctx,
    const uint8_t* data,
    size_t data_size,
    uint32_t total_payload_size,
    bool first_packet,
    bool* completed);

// Finalize SendObjectInfo: parse the collected dataset, create the empty
// file (or directory), allocate a handle, and return it via *out_handle.
// Returns the MTP response code (OK or an error).
//
// After this call the manager is reset and ready for the matching
// SendObject DATA phase that follows.
uint16_t mtp_transfer_state_finalize_object_info(
    MTPTransferStateManager* mgr,
    MTPContext* ctx,
    uint32_t* out_storage_id,
    uint32_t* out_parent_handle,
    uint32_t* out_handle);

// Reset to idle (e.g. after a Cancel).
void mtp_transfer_state_reset(MTPTransferStateManager* mgr);
