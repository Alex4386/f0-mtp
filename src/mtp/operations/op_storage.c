#include "op_storage.h"
#include "storage_info.h"
#include "../core/context.h"
#include "../storage/storage_paths.h"

// Pull in Flipper's storage API on-device, or the fake declarations during
// host-side test builds. The Makefile adds `-Imocks` so the fake header
// resolves transparently.
#ifdef FLIPPER_ZERO
#include <storage/storage.h>
#else
#include "flipper_api_fake.h"
#endif

#include <stdlib.h>
#include <string.h>

// --- GetStorageIDs -------------------------------------------------------

MTP_OPERATION_HANDLER(get_storage_ids) {
    (void)request;

    // payload = [count: u32][ids: u32 ...]
    uint32_t ids[2];
    uint32_t count = 0;

    SDInfo sd_info;
    bool sd_ok = (storage_sd_info(ctx->storage, &sd_info) == FSE_OK);
    if(sd_ok) {
        ids[count++] = MTP_STORAGE_ID_EXTERNAL;
    }

    // We always advertise internal storage as available.
    ids[count++] = MTP_STORAGE_ID_INTERNAL;

    size_t payload_size = sizeof(uint32_t) * (1 + count);
    uint8_t* buf = malloc(payload_size);
    if(!buf) {
        mtp_response_set_code(response, MTP_RESP_GENERAL_ERROR);
        return;
    }

    buf[0] = (uint8_t)(count & 0xFF);
    buf[1] = (uint8_t)((count >> 8) & 0xFF);
    buf[2] = (uint8_t)((count >> 16) & 0xFF);
    buf[3] = (uint8_t)((count >> 24) & 0xFF);

    for(uint32_t i = 0; i < count; i++) {
        uint32_t id = ids[i];
        size_t off = 4 + i * 4;
        buf[off + 0] = (uint8_t)(id & 0xFF);
        buf[off + 1] = (uint8_t)((id >> 8) & 0xFF);
        buf[off + 2] = (uint8_t)((id >> 16) & 0xFF);
        buf[off + 3] = (uint8_t)((id >> 24) & 0xFF);
    }

    mtp_response_set_data(response, buf, payload_size);
    mtp_response_set_code(response, MTP_RESP_OK);
}

// --- GetStorageInfo -------------------------------------------------------

MTP_OPERATION_HANDLER(get_storage_info) {
    uint32_t storage_id = request->params[0];

    MTPStorageInfoInput in = {
        .filesystem_type = MTP_FS_TYPE_GENERIC_HIERARCHICAL,
        .access_capability = MTP_ACCESS_READWRITE,
        .free_space_in_objects = 0,
    };

    if(storage_id == MTP_STORAGE_ID_INTERNAL) {
        in.storage_type = MTP_STORAGE_TYPE_FIXED_RAM;
        in.storage_description = "Internal Storage";
        in.volume_identifier = "INT_STORAGE";

        // No common_fs_info in fake API; advertise large capacity so the
        // host doesn't think we're full.
        in.max_capacity_blocks = mtp_kb_to_blocks(1024 * 1024); // 1 GB
        in.free_space_blocks = mtp_kb_to_blocks(512 * 1024); // 512 MB
    } else if(storage_id == MTP_STORAGE_ID_EXTERNAL) {
        in.storage_type = MTP_STORAGE_TYPE_REMOVABLE_RAM;
        in.storage_description = "SD Card";
        in.volume_identifier = "SD_CARD";

        SDInfo sd_info;
        if(storage_sd_info(ctx->storage, &sd_info) != FSE_OK) {
            in.max_capacity_blocks = 0;
            in.free_space_blocks = 0;
        } else {
            in.max_capacity_blocks = mtp_kb_to_blocks(sd_info.kb_total);
            in.free_space_blocks = mtp_kb_to_blocks(sd_info.kb_free);
        }
    } else {
        mtp_response_set_code(response, MTP_RESP_INVALID_STORAGE_ID);
        return;
    }

    uint8_t* buf = malloc(MTP_BUFFER_SIZE);
    if(!buf) {
        mtp_response_set_code(response, MTP_RESP_GENERAL_ERROR);
        return;
    }

    size_t n = mtp_build_storage_info(&in, buf, MTP_BUFFER_SIZE);
    if(n == 0) {
        free(buf);
        mtp_response_set_code(response, MTP_RESP_GENERAL_ERROR);
        return;
    }

    mtp_response_set_data(response, buf, n);
    mtp_response_set_code(response, MTP_RESP_OK);
}
