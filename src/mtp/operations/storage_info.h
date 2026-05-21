#pragma once

#include "../core/mtp_types.h"

// MTP StorageType values (PIMA 15740 / MTP 1.1 §5.2)
#define MTP_STORAGE_TYPE_FIXED_RAM     0x0003
#define MTP_STORAGE_TYPE_REMOVABLE_RAM 0x0004

// MTP FileSystemType
#define MTP_FS_TYPE_GENERIC_HIERARCHICAL 0x0002

// AccessCapability
#define MTP_ACCESS_READWRITE 0x0000
#define MTP_ACCESS_READONLY  0x0001

// Inputs for building a GetStorageInfo dataset.
typedef struct {
    uint16_t storage_type; // e.g. MTP_STORAGE_TYPE_REMOVABLE_RAM
    uint16_t filesystem_type;
    uint16_t access_capability;

    // Capacity / free space already expressed in *blocks of MTP_BLOCK_SIZE*,
    // matching what the original implementation reported. Use the helpers
    // below for unit conversion.
    uint64_t max_capacity_blocks;
    uint64_t free_space_blocks;
    uint32_t free_space_in_objects;

    const char* storage_description; // human-readable, e.g. "SD Card"
    const char* volume_identifier; // short id, e.g. "SD_CARD"
} MTPStorageInfoInput;

// Serialize a GetStorageInfo response payload into `buffer`.
// Returns bytes written, or 0 on failure / overflow.
size_t mtp_build_storage_info(
    const MTPStorageInfoInput* in,
    uint8_t* buffer,
    size_t buffer_size);

// Convenience: convert raw byte counts (e.g. from storage_sd_info kb fields)
// into MTP_BLOCK_SIZE block counts.
static inline uint64_t mtp_bytes_to_blocks(uint64_t bytes) {
    return bytes / MTP_BLOCK_SIZE;
}

static inline uint64_t mtp_kb_to_blocks(uint64_t kb) {
    return (kb * 1024ull) / MTP_BLOCK_SIZE;
}
