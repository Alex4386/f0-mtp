#include "storage_ops.h"
#include "utils.h"

static uint32_t hash_path(const char* path) {
    // FNV-1a
    uint32_t hash = 2166136261u;
    while(*path) {
        hash ^= (uint8_t)*path++;
        hash *= 16777619u;
    }
    return hash;
}

// true if `path` is `base` itself or lies below it
static bool path_is_under(const char* path, const char* base, size_t base_len) {
    return strncmp(path, base, base_len) == 0 && (path[base_len] == '\0' || path[base_len] == '/');
}

char* get_base_path_from_storage_id(uint32_t storage_id) {
    if(storage_id == INTERNAL_STORAGE_ID) {
        return STORAGE_INT_PATH_PREFIX;
    } else if(storage_id == EXTERNAL_STORAGE_ID) {
        return STORAGE_EXT_PATH_PREFIX;
    }
    return NULL;
}

char* get_path_from_handle(AppMTP* mtp, uint32_t handle) {
    FileHandle* current = mtp->handles;
    while(current != NULL) {
        if(current->handle == handle) {
            return current->path;
        }
        current = current->next;
    }
    return NULL;
}

uint32_t issue_object_handle(AppMTP* mtp, const char* path) {
    uint32_t hash = hash_path(path);

    for(FileHandle* current = mtp->handles; current != NULL; current = current->next) {
        if(current->hash == hash && strcmp(current->path, path) == 0) {
            return current->handle;
        }
    }

    FileHandle* entry = malloc(sizeof(FileHandle));
    entry->handle = ++mtp->last_handle;
    entry->hash = hash;
    entry->path = strdup(path);
    entry->next = mtp->handles;
    mtp->handles = entry;

    return entry->handle;
}

void rename_object_handles(AppMTP* mtp, const char* old_path, const char* new_path) {
    // old_path may point into one of the entries being rewritten
    char* old_copy = strdup(old_path);
    size_t old_len = strlen(old_copy);

    for(FileHandle* current = mtp->handles; current != NULL; current = current->next) {
        if(!path_is_under(current->path, old_copy, old_len)) continue;

        const char* rest = current->path + old_len;
        size_t len = strlen(new_path) + strlen(rest) + 1;
        char* path = malloc(len);
        snprintf(path, len, "%s%s", new_path, rest);

        free(current->path);
        current->path = path;
        current->hash = hash_path(path);
    }

    free(old_copy);
}

void remove_object_handles(AppMTP* mtp, const char* path) {
    // path may point into one of the entries being removed
    char* path_copy = strdup(path);
    size_t path_len = strlen(path_copy);

    FileHandle** link = &mtp->handles;
    while(*link != NULL) {
        FileHandle* current = *link;
        if(path_is_under(current->path, path_copy, path_len)) {
            *link = current->next;
            free(current->path);
            free(current);
        } else {
            link = &current->next;
        }
    }

    free(path_copy);
}

void free_object_handles(AppMTP* mtp) {
    FileHandle* handle = mtp->handles;
    mtp->handles = NULL;

    while(handle != NULL) {
        FileHandle* next = handle->next;
        free(handle->path);
        free(handle);
        handle = next;
    }
}

int list_and_issue_handles(
    AppMTP* mtp,
    uint32_t storage_id,
    uint32_t association,
    uint32_t** handles) {
    const char* base_path;
    if(association == 0xffffffff) {
        // root of the storage. "all storages" means the only one we expose.
        base_path = get_base_path_from_storage_id(
            storage_id == 0xffffffff ? EXTERNAL_STORAGE_ID : storage_id);
    } else {
        base_path = get_path_from_handle(mtp, association);
    }

    int count = 0;
    uint32_t capacity = 0;
    uint32_t* list = NULL;

    if(base_path != NULL) {
        File* dir = storage_file_alloc(mtp->storage);
        if(storage_dir_open(dir, base_path)) {
            FileInfo fileinfo;
            char* file_name = malloc(sizeof(char) * MTP_NAME_SIZE);
            char* full_path = malloc(sizeof(char) * MTP_PATH_SIZE);

            while(storage_dir_read(dir, &fileinfo, file_name, MTP_NAME_SIZE)) {
                if(!merge_path(full_path, MTP_PATH_SIZE, base_path, file_name)) {
                    FURI_LOG_W("MTP", "Path too long, skipped: %s", file_name);
                    continue;
                }

                uint32_t handle = issue_object_handle(mtp, full_path);
                if(handles != NULL) {
                    // slot 0 is reserved for the count
                    if((uint32_t)count + 2 > capacity) {
                        capacity = capacity ? capacity * 2 : 32;
                        list = realloc(list, sizeof(uint32_t) * capacity);
                    }
                    list[count + 1] = handle;
                }
                count++;
            }

            free(file_name);
            free(full_path);
        } else {
            FURI_LOG_E("MTP", "Failed to open directory: %s", base_path);
        }

        storage_dir_close(dir);
        storage_file_free(dir);
    }

    FURI_LOG_I("MTP", "Objects in %s: %d", base_path ? base_path : "(invalid)", count);

    if(handles != NULL) {
        if(list == NULL) list = malloc(sizeof(uint32_t));
        list[0] = count;
        *handles = list;
    }

    return count;
}
