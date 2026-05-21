#pragma once

#include "../core/mtp_types.h"

// Inputs for building a GetObjectInfo dataset (the bytes-on-wire
// representation of one MTP ObjectInfo dataset).
typedef struct {
    uint32_t storage_id;
    uint16_t object_format; // MTP_FORMAT_*
    uint16_t protection_status; // 0 = read-write
    uint32_t object_compressed_size; // file size (or 0 for directories)
    uint16_t association_type; // 1 (GenericFolder) for dirs, 0 otherwise
    uint32_t association_desc;
    uint32_t parent_object;

    const char* filename;
    const char* date_created; // MTP date string, e.g. "20240608T010702"
    const char* date_modified;
    const char* keywords;
} MTPObjectInfoInput;

// Serialize an ObjectInfo dataset into `buffer`. Returns bytes written, or 0
// on overflow / bad input.
size_t mtp_build_object_info(
    const MTPObjectInfoInput* in,
    uint8_t* buffer,
    size_t buffer_size);
