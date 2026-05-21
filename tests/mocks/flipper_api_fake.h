#pragma once

// Fake Flipper Storage API for host-side testing. Mirrors the names and
// signatures used by the Flipper SDK closely enough that production code
// written against `<storage/storage.h>` compiles unchanged.
//
// In production builds (FLIPPER_ZERO defined) the real SDK headers are used
// instead — see src/mtp/core/context.h.

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

// --- Types -----------------------------------------------------------------

typedef struct Storage Storage;
typedef struct File File;

typedef enum {
    FSE_OK = 0,
    FSE_NOT_READY,
    FSE_EXIST,
    FSE_NOT_EXIST,
    FSE_INVALID_PARAMETER,
    FSE_DENIED,
    FSE_INVALID_NAME,
    FSE_INTERNAL,
    FSE_NOT_IMPLEMENTED,
    FSE_ALREADY_OPEN,
} FS_Error;

typedef enum {
    FSAM_READ = 1,
    FSAM_WRITE = 2,
    FSAM_READ_WRITE = FSAM_READ | FSAM_WRITE,
} FS_AccessMode;

typedef enum {
    FSOM_OPEN_EXISTING = 1,
    FSOM_OPEN_ALWAYS,
    FSOM_OPEN_APPEND,
    FSOM_CREATE_NEW,
    FSOM_CREATE_ALWAYS,
} FS_OpenMode;

typedef struct {
    uint8_t flags; // 1 = directory
    uint64_t size;
} FileInfo;

#define FSF_DIRECTORY 0x01

static inline bool file_info_is_dir(const FileInfo* info) {
    return info && (info->flags & FSF_DIRECTORY);
}

typedef struct {
    char fs_type[16];
    uint64_t kb_total;
    uint64_t kb_free;
    uint16_t cluster_size;
    uint16_t sector_size;
    char label[16];
    FS_Error error;
} SDInfo;

// --- Record access ---------------------------------------------------------

void* furi_record_open(const char* record_name);
void furi_record_close(const char* record_name);

// --- File API --------------------------------------------------------------

File* storage_file_alloc(Storage* storage);
void storage_file_free(File* file);

bool storage_file_open(File* file, const char* path, FS_AccessMode am, FS_OpenMode om);
bool storage_file_close(File* file);

uint16_t storage_file_read(File* file, void* buffer, uint16_t size);
uint16_t storage_file_write(File* file, const void* buffer, uint16_t size);
bool storage_file_sync(File* file);

uint64_t storage_file_size(File* file);

// --- Directory API ---------------------------------------------------------

bool storage_dir_open(File* file, const char* path);
bool storage_dir_close(File* file);
bool storage_dir_read(File* file, FileInfo* info, char* name, uint16_t name_max);

// --- Common API ------------------------------------------------------------

bool storage_file_exists(Storage* storage, const char* path);
bool storage_dir_exists(Storage* storage, const char* path);

bool storage_simply_mkdir(Storage* storage, const char* path);

FS_Error storage_common_remove(Storage* storage, const char* path);
FS_Error storage_common_rename(Storage* storage, const char* old_path, const char* new_path);
FS_Error storage_common_stat(Storage* storage, const char* path, FileInfo* info);
FS_Error storage_sd_info(Storage* storage, SDInfo* info);

bool storage_simply_remove_recursive(Storage* storage, const char* path);

// --- Test-only control knobs ----------------------------------------------

// Reset all in-memory filesystem and failure injection state.
void flipper_fake_reset(void);

// Seed an entry in the fake filesystem.
void flipper_fake_add_file(const char* path, const char* contents);
void flipper_fake_add_dir(const char* path);

// Force the next file open to fail.
void flipper_fake_set_fail_open(bool fail);

// Force subsequent writes to return 0 bytes written.
void flipper_fake_set_fail_write(bool fail);

// Inspect file contents (NULL if not present).
const char* flipper_fake_get_file_contents(const char* path);
