#pragma once

#include "../core/mtp_types.h"

// Build a single ObjectPropValue dataset for a given property code.
// The payload format follows the response style used in src.old:
//   [prop_code:u32][type:u32][flag:u8][value...][trailer:u8]
//
// Supported prop codes:
//   MTP_PROP_STORAGE_ID       -> writes storage_id (u32)
//   MTP_PROP_OBJECT_FORMAT    -> writes MTP_FORMAT_UNDEFINED (u16)
//   MTP_PROP_OBJECT_FILE_NAME -> writes filename as MTP string
//
// Returns bytes written, or 0 on unsupported prop code or overflow.
size_t mtp_build_object_prop_value(
    uint16_t prop_code,
    uint32_t storage_id,
    const char* filename,
    uint8_t* buffer,
    size_t buffer_size);
