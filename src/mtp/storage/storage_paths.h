#pragma once

#include "../core/mtp_types.h"

// Flipper Zero filesystem path prefixes.
// Defined locally so tests can use them without depending on <storage/storage.h>.
// On a real Flipper build these match the SDK's STORAGE_INT_PATH_PREFIX /
// STORAGE_EXT_PATH_PREFIX values.
#define MTP_PATH_PREFIX_INTERNAL "/int"
#define MTP_PATH_PREFIX_EXTERNAL "/ext"

// Resolve a storage ID to its base mount path.
// Returns NULL for unknown storage IDs.
const char* mtp_storage_path_from_id(uint32_t storage_id);

// Resolve a base path to its storage ID. Returns 0 if the path does not
// belong to a known storage.
uint32_t mtp_storage_id_from_path(const char* path);

// Join `base` and `name` into `out` with a single '/' separator, writing at
// most `out_size` bytes (including the terminator). Returns the number of
// bytes written excluding the terminator, or 0 on overflow / bad input.
//
// Examples:
//   merge("/ext", "file.txt", out) -> "/ext/file.txt"
//   merge("/ext/", "file.txt", out) -> "/ext/file.txt"  (no double slash)
//   merge("/",   "x",        out) -> "/x"
size_t mtp_storage_merge_path(const char* base, const char* name, char* out, size_t out_size);

// Return a pointer to the basename portion of `path` (substring after the
// last '/'). If `path` has no '/', returns `path` itself. Never returns NULL
// for a non-NULL input.
const char* mtp_storage_basename(const char* path);
