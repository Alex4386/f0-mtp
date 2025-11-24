#pragma once

#include "mtp_types.h"

// Object index (handle ↔ path mapping)
typedef struct MTPObjectIndex MTPObjectIndex;

// Create/destroy
MTPObjectIndex* mtp_object_index_create(void);
void mtp_object_index_destroy(MTPObjectIndex* index);

// Handle operations (O(1) average case with hash table)
uint32_t mtp_object_index_add(MTPObjectIndex* index, const char* path);
const char* mtp_object_index_get_path(MTPObjectIndex* index, uint32_t handle);
bool mtp_object_index_update_path(MTPObjectIndex* index, uint32_t handle, const char* new_path);
bool mtp_object_index_remove(MTPObjectIndex* index, uint32_t handle);
bool mtp_object_index_contains(MTPObjectIndex* index, uint32_t handle);

// Batch operations
void mtp_object_index_clear(MTPObjectIndex* index);
size_t mtp_object_index_count(MTPObjectIndex* index);

// Iteration (for debugging/stats)
typedef void (*MTPObjectIndexIterator)(uint32_t handle, const char* path, void* user_data);
void mtp_object_index_foreach(
    MTPObjectIndex* index,
    MTPObjectIndexIterator callback,
    void* user_data
);
