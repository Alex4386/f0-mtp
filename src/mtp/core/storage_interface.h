#pragma once

#include "mtp_types.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Forward declarations
typedef struct MTPStorage MTPStorage;
typedef struct MTPFile MTPFile;
typedef struct MTPDirectory MTPDirectory;

// Storage access modes
typedef enum {
    MTP_STORAGE_MODE_READ = 1,
    MTP_STORAGE_MODE_WRITE = 2,
    MTP_STORAGE_MODE_RW = 3,
} MTPStorageMode;

// File/directory information
typedef struct {
    uint64_t size;
    bool is_directory;
    // Could add timestamps, permissions, etc.
} MTPFileInfo;

// Storage interface (vtable pattern)
typedef struct {
    // File operations
    MTPFile* (*file_open)(MTPStorage* storage, const char* path, MTPStorageMode mode);
    int (*file_read)(MTPFile* file, void* buffer, size_t size);
    int (*file_write)(MTPFile* file, const void* buffer, size_t size);
    int (*file_sync)(MTPFile* file);
    void (*file_close)(MTPFile* file);
    uint64_t (*file_size)(MTPFile* file);

    // Directory operations
    MTPDirectory* (*dir_open)(MTPStorage* storage, const char* path);
    bool (*dir_read)(MTPDirectory* dir, MTPFileInfo* info, char* name, size_t name_size);
    void (*dir_close)(MTPDirectory* dir);

    // File system operations
    bool (*exists)(MTPStorage* storage, const char* path);
    bool (*is_dir)(MTPStorage* storage, const char* path);
    bool (*mkdir)(MTPStorage* storage, const char* path);
    bool (*remove)(MTPStorage* storage, const char* path);
    bool (*rename)(MTPStorage* storage, const char* old_path, const char* new_path);
    bool (*stat)(MTPStorage* storage, const char* path, MTPFileInfo* info);

    // Storage info
    bool (*get_info)(MTPStorage* storage, uint64_t* total, uint64_t* free);

    // Lifecycle
    void (*destroy)(MTPStorage* storage);
} MTPStorageVTable;

// Storage structure
struct MTPStorage {
    const MTPStorageVTable* vtable;
    void* context;  // Implementation-specific data
};

// Inline helper functions for cleaner API usage
static inline MTPFile* mtp_storage_file_open(
    MTPStorage* storage,
    const char* path,
    MTPStorageMode mode
) {
    return storage->vtable->file_open(storage, path, mode);
}

static inline int mtp_storage_file_read(MTPFile* file, void* buffer, size_t size) {
    return file ? ((MTPStorage*)file)->vtable->file_read(file, buffer, size) : -1;
}

static inline int mtp_storage_file_write(MTPFile* file, const void* buffer, size_t size) {
    return file ? ((MTPStorage*)file)->vtable->file_write(file, buffer, size) : -1;
}

static inline int mtp_storage_file_sync(MTPFile* file) {
    return file ? ((MTPStorage*)file)->vtable->file_sync(file) : -1;
}

static inline void mtp_storage_file_close(MTPFile* file) {
    if(file) {
        ((MTPStorage*)file)->vtable->file_close(file);
    }
}

static inline uint64_t mtp_storage_file_size(MTPFile* file) {
    return file ? ((MTPStorage*)file)->vtable->file_size(file) : 0;
}

static inline MTPDirectory* mtp_storage_dir_open(MTPStorage* storage, const char* path) {
    return storage->vtable->dir_open(storage, path);
}

static inline bool mtp_storage_dir_read(
    MTPDirectory* dir,
    MTPFileInfo* info,
    char* name,
    size_t name_size
) {
    return dir ? ((MTPStorage*)dir)->vtable->dir_read(dir, info, name, name_size) : false;
}

static inline void mtp_storage_dir_close(MTPDirectory* dir) {
    if(dir) {
        ((MTPStorage*)dir)->vtable->dir_close(dir);
    }
}

static inline bool mtp_storage_exists(MTPStorage* storage, const char* path) {
    return storage->vtable->exists(storage, path);
}

static inline bool mtp_storage_is_dir(MTPStorage* storage, const char* path) {
    return storage->vtable->is_dir(storage, path);
}

static inline bool mtp_storage_mkdir(MTPStorage* storage, const char* path) {
    return storage->vtable->mkdir(storage, path);
}

static inline bool mtp_storage_remove(MTPStorage* storage, const char* path) {
    return storage->vtable->remove(storage, path);
}

static inline bool mtp_storage_rename(MTPStorage* storage, const char* old_path, const char* new_path) {
    return storage->vtable->rename(storage, old_path, new_path);
}

static inline bool mtp_storage_stat(MTPStorage* storage, const char* path, MTPFileInfo* info) {
    return storage->vtable->stat(storage, path, info);
}

static inline bool mtp_storage_get_info(MTPStorage* storage, uint64_t* total, uint64_t* free) {
    return storage->vtable->get_info(storage, total, free);
}

static inline void mtp_storage_destroy(MTPStorage* storage) {
    if(storage && storage->vtable->destroy) {
        storage->vtable->destroy(storage);
    }
}
