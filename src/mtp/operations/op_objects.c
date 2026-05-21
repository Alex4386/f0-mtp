#include "op_objects.h"
#include "object_info.h"
#include "../core/context.h"
#include "../core/transport.h"
#include "../storage/storage_paths.h"

#ifdef FLIPPER_ZERO
#include <storage/storage.h>
#else
#include "flipper_api_fake.h"
#endif

#include <stdlib.h>
#include <string.h>

// --- Internals -----------------------------------------------------------

// Iterate over the immediate children of `dir_path`, optionally writing each
// child's handle into `out_handles` (NULL = count only). Returns the number
// of children listed.
static uint32_t list_immediate_children(
    MTPContext* ctx,
    const char* dir_path,
    uint32_t* out_handles,
    size_t out_capacity) {
    File* dir = storage_file_alloc(ctx->storage);
    if(!dir) return 0;

    if(!storage_dir_open(dir, dir_path)) {
        storage_file_free(dir);
        return 0;
    }

    uint32_t count = 0;
    FileInfo info;
    char name[MTP_NAME_SIZE];

    while(storage_dir_read(dir, &info, name, sizeof(name))) {
        if(name[0] == '\0') continue;

        char full[MTP_PATH_SIZE];
        if(mtp_storage_merge_path(dir_path, name, full, sizeof(full)) == 0) continue;

        uint32_t handle = mtp_object_index_add(ctx->object_index, full);
        if(out_handles && count < out_capacity) {
            out_handles[count] = handle;
        }
        count++;
    }

    storage_dir_close(dir);
    storage_file_free(dir);
    return count;
}

// --- GetNumObjects --------------------------------------------------------

MTP_OPERATION_HANDLER(get_num_objects) {
    uint32_t storage_id = request->params[0];
    // params[1] is format (unsupported)
    uint32_t parent = request->params[2];

    const char* dir = NULL;
    if(parent == 0 || parent == 0xffffffff) {
        dir = mtp_storage_path_from_id(storage_id);
        if(!dir) {
            mtp_response_set_code(response, MTP_RESP_INVALID_STORAGE_ID);
            return;
        }
    } else {
        dir = mtp_object_index_get_path(ctx->object_index, parent);
        if(!dir) {
            mtp_response_set_code(response, MTP_RESP_INVALID_OBJECT_HANDLE);
            return;
        }
    }

    uint32_t count = list_immediate_children(ctx, dir, NULL, 0);

    mtp_response_add_param(response, count);
    mtp_response_set_code(response, MTP_RESP_OK);
}

// --- GetObjectHandles -----------------------------------------------------

MTP_OPERATION_HANDLER(get_object_handles) {
    uint32_t storage_id = request->params[0];
    uint32_t format = request->params[1];
    uint32_t parent = request->params[2];

    if(format != 0) {
        // No filtering-by-format support
        mtp_response_set_code(response, MTP_RESP_SPEC_BY_FORMAT_UNSUPPORTED);
        return;
    }

    const char* dir = NULL;
    if(parent == 0 || parent == 0xffffffff) {
        dir = mtp_storage_path_from_id(storage_id);
        if(!dir) {
            mtp_response_set_code(response, MTP_RESP_INVALID_STORAGE_ID);
            return;
        }
    } else {
        dir = mtp_object_index_get_path(ctx->object_index, parent);
        if(!dir) {
            mtp_response_set_code(response, MTP_RESP_INVALID_OBJECT_HANDLE);
            return;
        }
    }

    // Count first so we can allocate exactly enough.
    uint32_t count = list_immediate_children(ctx, dir, NULL, 0);

    size_t payload_size = sizeof(uint32_t) * (1 + count);
    uint8_t* buf = malloc(payload_size > 0 ? payload_size : 4);
    if(!buf) {
        mtp_response_set_code(response, MTP_RESP_GENERAL_ERROR);
        return;
    }

    // Write count
    buf[0] = (uint8_t)(count & 0xFF);
    buf[1] = (uint8_t)((count >> 8) & 0xFF);
    buf[2] = (uint8_t)((count >> 16) & 0xFF);
    buf[3] = (uint8_t)((count >> 24) & 0xFF);

    if(count > 0) {
        uint32_t* handles = (uint32_t*)(buf + 4);
        list_immediate_children(ctx, dir, handles, count);

        // Defensive byte-swap if we ever target a big-endian host (no-op on
        // every Flipper target). Keeping the explicit byte writes for clarity:
        for(uint32_t i = 0; i < count; i++) {
            uint32_t h = handles[i];
            uint8_t* p = buf + 4 + i * 4;
            p[0] = (uint8_t)(h & 0xFF);
            p[1] = (uint8_t)((h >> 8) & 0xFF);
            p[2] = (uint8_t)((h >> 16) & 0xFF);
            p[3] = (uint8_t)((h >> 24) & 0xFF);
        }
    }

    mtp_response_set_data(response, buf, payload_size);
    mtp_response_set_code(response, MTP_RESP_OK);
}

// --- GetObjectInfo --------------------------------------------------------

MTP_OPERATION_HANDLER(get_object_info) {
    uint32_t handle = request->params[0];
    const char* path = mtp_object_index_get_path(ctx->object_index, handle);
    if(!path) {
        mtp_response_set_code(response, MTP_RESP_INVALID_OBJECT_HANDLE);
        return;
    }

    FileInfo info;
    FS_Error err = storage_common_stat(ctx->storage, path, &info);
    if(err != FSE_OK) {
        mtp_response_set_code(response, MTP_RESP_INVALID_OBJECT_HANDLE);
        return;
    }

    bool is_dir = file_info_is_dir(&info);

    MTPObjectInfoInput in = {
        .storage_id = mtp_storage_id_from_path(path),
        .object_format = is_dir ? MTP_FORMAT_ASSOCIATION : MTP_FORMAT_UNDEFINED,
        .protection_status = 0,
        .object_compressed_size = (uint32_t)info.size,
        .association_type = is_dir ? 0x0001 : 0x0000,
        .association_desc = 0,
        .parent_object = 0,
        .filename = mtp_storage_basename(path),
        .date_created = "20240608T010702",
        .date_modified = "20240608T010702",
        .keywords = "",
    };

    if(in.storage_id == 0) {
        mtp_response_set_code(response, MTP_RESP_INVALID_STORAGE_ID);
        return;
    }

    uint8_t* buf = malloc(MTP_BUFFER_SIZE);
    if(!buf) {
        mtp_response_set_code(response, MTP_RESP_GENERAL_ERROR);
        return;
    }

    size_t n = mtp_build_object_info(&in, buf, MTP_BUFFER_SIZE);
    if(n == 0) {
        free(buf);
        mtp_response_set_code(response, MTP_RESP_GENERAL_ERROR);
        return;
    }

    mtp_response_set_data(response, buf, n);
    mtp_response_set_code(response, MTP_RESP_OK);
}

// --- GetObject ------------------------------------------------------------
//
// Reads a file into a single allocated buffer. For files larger than
// MTP_BUFFER_SIZE this should be replaced by a streaming response — see
// transfer_state.c.

MTP_OPERATION_HANDLER(get_object) {
    uint32_t handle = request->params[0];
    const char* path = mtp_object_index_get_path(ctx->object_index, handle);
    if(!path) {
        mtp_response_set_code(response, MTP_RESP_INVALID_OBJECT_HANDLE);
        return;
    }

    File* file = storage_file_alloc(ctx->storage);
    if(!file) {
        mtp_response_set_code(response, MTP_RESP_GENERAL_ERROR);
        return;
    }

    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        mtp_response_set_code(response, MTP_RESP_INVALID_OBJECT_HANDLE);
        return;
    }

    uint64_t size = storage_file_size(file);
    if(size > MTP_USB_MAX_PAYLOAD) {
        storage_file_close(file);
        storage_file_free(file);
        mtp_response_set_code(response, MTP_RESP_GENERAL_ERROR);
        return;
    }

    uint8_t* buf = malloc((size_t)size);
    if(!buf && size > 0) {
        storage_file_close(file);
        storage_file_free(file);
        mtp_response_set_code(response, MTP_RESP_GENERAL_ERROR);
        return;
    }

    size_t read_total = 0;
    while(read_total < size) {
        uint16_t want = (size - read_total > 0xFFFF) ? 0xFFFF : (uint16_t)(size - read_total);
        uint16_t got = storage_file_read(file, buf + read_total, want);
        if(got == 0) break;
        read_total += got;
    }

    storage_file_close(file);
    storage_file_free(file);

    if(read_total != size) {
        free(buf);
        mtp_response_set_code(response, MTP_RESP_GENERAL_ERROR);
        return;
    }

    mtp_response_set_data(response, buf, read_total);
    mtp_response_set_code(response, MTP_RESP_OK);
}

// --- DeleteObject ---------------------------------------------------------

MTP_OPERATION_HANDLER(delete_object) {
    uint32_t handle = request->params[0];
    const char* path = mtp_object_index_get_path(ctx->object_index, handle);
    if(!path) {
        mtp_response_set_code(response, MTP_RESP_INVALID_OBJECT_HANDLE);
        return;
    }

    FileInfo info;
    FS_Error err = storage_common_stat(ctx->storage, path, &info);
    if(err != FSE_OK) {
        mtp_response_set_code(response, MTP_RESP_INVALID_OBJECT_HANDLE);
        return;
    }

    bool ok;
    if(file_info_is_dir(&info)) {
        ok = storage_simply_remove_recursive(ctx->storage, path);
    } else {
        ok = (storage_common_remove(ctx->storage, path) == FSE_OK);
    }

    if(!ok) {
        mtp_response_set_code(response, MTP_RESP_GENERAL_ERROR);
        return;
    }

    mtp_object_index_remove(ctx->object_index, handle);
    mtp_response_set_code(response, MTP_RESP_OK);
}

// --- MoveObject -----------------------------------------------------------

MTP_OPERATION_HANDLER(move_object) {
    uint32_t handle = request->params[0];
    uint32_t storage_id = request->params[1];
    uint32_t parent = request->params[2];

    const char* path = mtp_object_index_get_path(ctx->object_index, handle);
    if(!path) {
        mtp_response_set_code(response, MTP_RESP_INVALID_OBJECT_HANDLE);
        return;
    }

    const char* parent_path = NULL;
    if(parent == 0 || parent == 0xffffffff) {
        parent_path = mtp_storage_path_from_id(storage_id);
        if(!parent_path) {
            mtp_response_set_code(response, MTP_RESP_INVALID_STORAGE_ID);
            return;
        }
    } else {
        parent_path = mtp_object_index_get_path(ctx->object_index, parent);
        if(!parent_path) {
            mtp_response_set_code(response, MTP_RESP_INVALID_OBJECT_HANDLE);
            return;
        }
    }

    const char* base = mtp_storage_basename(path);
    char new_path[MTP_PATH_SIZE];
    if(mtp_storage_merge_path(parent_path, base, new_path, sizeof(new_path)) == 0) {
        mtp_response_set_code(response, MTP_RESP_GENERAL_ERROR);
        return;
    }

    if(storage_common_rename(ctx->storage, path, new_path) != FSE_OK) {
        mtp_response_set_code(response, MTP_RESP_GENERAL_ERROR);
        return;
    }

    mtp_object_index_update_path(ctx->object_index, handle, new_path);
    mtp_response_set_code(response, MTP_RESP_OK);
}
