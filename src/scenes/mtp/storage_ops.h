#pragma once

#include <storage/storage.h>
#include "main.h"
#include "mtp.h"

// Storage utility functions
char* get_base_path_from_storage_id(uint32_t storage_id);
char* get_path_from_handle(AppMTP* mtp, uint32_t handle);
uint32_t issue_object_handle(AppMTP* mtp, const char* path);

// Repoints the handles of `old_path` (and everything below it) to `new_path`
void rename_object_handles(AppMTP* mtp, const char* old_path, const char* new_path);
// Drops the handles of `path` and everything below it
void remove_object_handles(AppMTP* mtp, const char* path);
void free_object_handles(AppMTP* mtp);

/**
 * Lists the objects under `association` and issues handles for them.
 * If `handles` is not NULL, it receives a malloc'd MTP array dataset:
 * element 0 is the count, followed by the handles.
 */
int list_and_issue_handles(
    AppMTP* mtp,
    uint32_t storage_id,
    uint32_t association,
    uint32_t** handles);
