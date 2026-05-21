#include "transfer_state.h"
#include "mtp_string.h"
#include "../storage/storage_paths.h"

#ifdef FLIPPER_ZERO
#include <storage/storage.h>
#else
#include "flipper_api_fake.h"
#endif

#include <stdlib.h>
#include <string.h>

#define MTP_OBJECT_INFO_STAGING_SIZE 1024
#define MTP_FILE_SYNC_INTERVAL_BYTES (1024 * 1024)

struct MTPTransferStateManager {
    MTPTransferKind kind;
    uint32_t transaction_id;

    // SendObjectInfo collection state
    uint8_t* staging;
    size_t staging_size;
    size_t staging_capacity;
    uint32_t pending_storage_id;
    uint32_t pending_parent_handle;
    char pending_path[MTP_PATH_SIZE];
    uint32_t pending_handle;
    bool pending_is_dir;

    // SendObject streaming state
    File* file;
    uint32_t bytes_remaining;
    uint32_t bytes_since_sync;
};

// --- Lifecycle ------------------------------------------------------------

MTPTransferStateManager* mtp_transfer_state_manager_create(void) {
    MTPTransferStateManager* mgr = calloc(1, sizeof(*mgr));
    return mgr;
}

void mtp_transfer_state_manager_destroy(MTPTransferStateManager* mgr) {
    if(!mgr) return;
    free(mgr->staging);
    free(mgr);
}

// --- State queries --------------------------------------------------------

MTPTransferKind mtp_transfer_state_kind(const MTPTransferStateManager* mgr) {
    return mgr ? mgr->kind : MTP_TRANSFER_NONE;
}

uint32_t mtp_transfer_state_transaction_id(const MTPTransferStateManager* mgr) {
    return mgr ? mgr->transaction_id : 0;
}

uint32_t mtp_transfer_state_bytes_remaining(const MTPTransferStateManager* mgr) {
    return mgr ? mgr->bytes_remaining : 0;
}

// --- Internal: reset helpers ---------------------------------------------

static void close_file_if_open(MTPTransferStateManager* mgr) {
    if(mgr->file) {
        storage_file_close(mgr->file);
        storage_file_free(mgr->file);
        mgr->file = NULL;
    }
}

void mtp_transfer_state_reset(MTPTransferStateManager* mgr) {
    if(!mgr) return;
    close_file_if_open(mgr);
    free(mgr->staging);
    mgr->staging = NULL;
    mgr->staging_size = 0;
    mgr->staging_capacity = 0;
    mgr->kind = MTP_TRANSFER_NONE;
    mgr->transaction_id = 0;
    mgr->pending_storage_id = 0;
    mgr->pending_parent_handle = 0;
    mgr->pending_path[0] = '\0';
    mgr->pending_handle = 0;
    mgr->pending_is_dir = false;
    mgr->bytes_remaining = 0;
    mgr->bytes_since_sync = 0;
}

// --- COMMAND-phase hooks --------------------------------------------------

void mtp_transfer_state_begin_send_object_info(
    MTPTransferStateManager* mgr,
    uint32_t transaction_id,
    uint32_t storage_id,
    uint32_t parent_handle) {
    if(!mgr) return;
    mtp_transfer_state_reset(mgr);

    mgr->kind = MTP_TRANSFER_SEND_OBJECT_INFO;
    mgr->transaction_id = transaction_id;
    mgr->pending_storage_id = storage_id;
    mgr->pending_parent_handle = parent_handle;
    mgr->staging = malloc(MTP_OBJECT_INFO_STAGING_SIZE);
    mgr->staging_capacity = mgr->staging ? MTP_OBJECT_INFO_STAGING_SIZE : 0;
    mgr->staging_size = 0;
}

bool mtp_transfer_state_begin_send_object(
    MTPTransferStateManager* mgr,
    MTPContext* ctx,
    uint32_t transaction_id) {
    if(!mgr || !ctx) return false;
    if(mgr->pending_handle == 0 || mgr->pending_path[0] == '\0' || mgr->pending_is_dir) {
        return false;
    }

    // Don't reset everything — we want to preserve pending_path. Just transition
    // kind and open the file.
    close_file_if_open(mgr);
    mgr->kind = MTP_TRANSFER_SEND_OBJECT;
    mgr->transaction_id = transaction_id;
    mgr->bytes_remaining = 0; // set on first data packet
    mgr->bytes_since_sync = 0;

    mgr->file = storage_file_alloc(ctx->storage);
    if(!mgr->file) return false;

    if(!storage_file_open(mgr->file, mgr->pending_path, FSAM_WRITE, FSOM_OPEN_EXISTING)) {
        storage_file_free(mgr->file);
        mgr->file = NULL;
        return false;
    }
    return true;
}

// --- DATA-phase ingest ----------------------------------------------------

bool mtp_transfer_state_feed_data(
    MTPTransferStateManager* mgr,
    MTPContext* ctx,
    const uint8_t* data,
    size_t data_size,
    uint32_t total_payload_size,
    bool first_packet,
    bool* completed) {
    if(!mgr || !data) return false;
    if(completed) *completed = false;

    switch(mgr->kind) {
    case MTP_TRANSFER_SEND_OBJECT_INFO: {
        // Accumulate into staging buffer. If it doesn't fit, grow once up to
        // a hard ceiling — ObjectInfo datasets are tiny in practice.
        if(mgr->staging_size + data_size > mgr->staging_capacity) {
            size_t new_cap = mgr->staging_capacity * 2;
            if(new_cap < mgr->staging_size + data_size) {
                new_cap = mgr->staging_size + data_size;
            }
            if(new_cap > 8192) return false;
            uint8_t* new_buf = realloc(mgr->staging, new_cap);
            if(!new_buf) return false;
            mgr->staging = new_buf;
            mgr->staging_capacity = new_cap;
        }
        memcpy(mgr->staging + mgr->staging_size, data, data_size);
        mgr->staging_size += data_size;

        if(mgr->staging_size >= total_payload_size) {
            if(completed) *completed = true;
        }
        return true;
    }

    case MTP_TRANSFER_SEND_OBJECT: {
        if(!mgr->file) return false;
        if(first_packet) {
            mgr->bytes_remaining = total_payload_size;
            mgr->bytes_since_sync = 0;
        }

        size_t to_write = data_size;
        if(to_write > mgr->bytes_remaining) to_write = mgr->bytes_remaining;

        // storage_file_write takes a uint16_t — chunk for large packets.
        const uint8_t* p = data;
        size_t left = to_write;
        while(left > 0) {
            uint16_t chunk = left > 0xFFFF ? 0xFFFF : (uint16_t)left;
            uint16_t wrote = storage_file_write(mgr->file, p, chunk);
            if(wrote != chunk) {
                close_file_if_open(mgr);
                mgr->kind = MTP_TRANSFER_NONE;
                return false;
            }
            p += wrote;
            left -= wrote;
        }

        mgr->bytes_remaining -= to_write;
        mgr->bytes_since_sync += to_write;

        if(mgr->bytes_since_sync >= MTP_FILE_SYNC_INTERVAL_BYTES) {
            storage_file_sync(mgr->file);
            mgr->bytes_since_sync = 0;
        }

        if(mgr->bytes_remaining == 0) {
            storage_file_sync(mgr->file);
            close_file_if_open(mgr);
            mgr->kind = MTP_TRANSFER_NONE;
            if(completed) *completed = true;
        }
        return true;
    }

    default:
        (void)ctx;
        return false;
    }
}

uint16_t mtp_transfer_state_finalize_object_info(
    MTPTransferStateManager* mgr,
    MTPContext* ctx,
    uint32_t* out_storage_id,
    uint32_t* out_parent_handle,
    uint32_t* out_handle) {
    if(!mgr || !ctx) return MTP_RESP_GENERAL_ERROR;
    if(mgr->kind != MTP_TRANSFER_SEND_OBJECT_INFO) return MTP_RESP_GENERAL_ERROR;
    if(mgr->staging_size < 16) return MTP_RESP_GENERAL_ERROR; // need at least the header

    // Parse the fixed part of the ObjectInfo dataset.
    //   offset 0:  storage_id (u32)  -- ignored, we trust the COMMAND param
    //   offset 4:  format (u16)
    //   offset 6:  protection (u16)
    //   offset 8:  compressed_size (u32)
    //   offset 12: thumb_format (u16)
    //   ... lots of thumb/image fields ...
    //   The filename string lives after the entire fixed part. The legacy
    //   implementation hard-coded an offset of `sizeof(ObjectInfoHeader)+12`,
    //   which corresponds to: 48-byte header + sequence_number(4) + 8 ?
    //   That code worked in practice; we replicate the same offset for
    //   compatibility with the same MTP client behavior.
    const size_t filename_offset = 52; // 52 = full fixed part per MTP 1.1 spec
    if(mgr->staging_size <= filename_offset) return MTP_RESP_GENERAL_ERROR;

    uint16_t format = (uint16_t)mgr->staging[4] | ((uint16_t)mgr->staging[5] << 8);
    bool is_dir = (format == MTP_FORMAT_ASSOCIATION);

    const uint8_t* fn_buf = mgr->staging + filename_offset;
    size_t fn_buf_size = mgr->staging_size - filename_offset;

    char* name = NULL;
    if(mtp_string_has_unicode(fn_buf)) {
        name = strdup(is_dir ? "New Folder" : "New File");
    } else {
        name = mtp_string_decode(fn_buf, fn_buf_size);
    }
    if(!name) return MTP_RESP_GENERAL_ERROR;
    if(name[0] == '\0') {
        free(name);
        name = strdup(is_dir ? "New Folder" : "New File");
        if(!name) return MTP_RESP_GENERAL_ERROR;
    }

    // Resolve parent directory.
    const char* parent_dir = NULL;
    if(mgr->pending_parent_handle == 0 || mgr->pending_parent_handle == 0xffffffff) {
        parent_dir = mtp_storage_path_from_id(mgr->pending_storage_id);
    } else {
        parent_dir = mtp_object_index_get_path(ctx->object_index, mgr->pending_parent_handle);
    }
    if(!parent_dir) {
        free(name);
        return MTP_RESP_INVALID_STORAGE_ID;
    }

    char full[MTP_PATH_SIZE];
    if(mtp_storage_merge_path(parent_dir, name, full, sizeof(full)) == 0) {
        free(name);
        return MTP_RESP_GENERAL_ERROR;
    }
    free(name);

    // Create the underlying filesystem entry.
    uint16_t result = MTP_RESP_OK;
    if(is_dir) {
        if(!storage_dir_exists(ctx->storage, full)) {
            if(!storage_simply_mkdir(ctx->storage, full)) {
                result = MTP_RESP_GENERAL_ERROR;
            }
        }
    } else {
        if(!storage_file_exists(ctx->storage, full)) {
            File* f = storage_file_alloc(ctx->storage);
            if(!f) {
                result = MTP_RESP_GENERAL_ERROR;
            } else {
                if(!storage_file_open(f, full, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                    result = MTP_RESP_GENERAL_ERROR;
                }
                storage_file_close(f);
                storage_file_free(f);
            }
        }
    }

    if(result != MTP_RESP_OK) return result;

    // Allocate an object handle for this new entry.
    uint32_t handle = mtp_object_index_add(ctx->object_index, full);
    if(handle == 0) return MTP_RESP_GENERAL_ERROR;

    mgr->pending_handle = handle;
    mgr->pending_is_dir = is_dir;
    strncpy(mgr->pending_path, full, sizeof(mgr->pending_path) - 1);
    mgr->pending_path[sizeof(mgr->pending_path) - 1] = '\0';

    // Free staging now — caller has the result.
    free(mgr->staging);
    mgr->staging = NULL;
    mgr->staging_size = 0;
    mgr->staging_capacity = 0;
    // Leave kind as SEND_OBJECT_INFO until the host moves on. The next
    // SendObject command will transition us via begin_send_object().

    if(out_storage_id) *out_storage_id = mgr->pending_storage_id;
    if(out_parent_handle) *out_parent_handle = mgr->pending_parent_handle;
    if(out_handle) *out_handle = handle;

    return MTP_RESP_OK;
}
