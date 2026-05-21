#include "storage_paths.h"
#include <string.h>

const char* mtp_storage_path_from_id(uint32_t storage_id) {
    switch(storage_id) {
    case MTP_STORAGE_ID_INTERNAL:
        return MTP_PATH_PREFIX_INTERNAL;
    case MTP_STORAGE_ID_EXTERNAL:
        return MTP_PATH_PREFIX_EXTERNAL;
    default:
        return NULL;
    }
}

uint32_t mtp_storage_id_from_path(const char* path) {
    if(!path) return 0;

    size_t int_len = strlen(MTP_PATH_PREFIX_INTERNAL);
    if(strncmp(path, MTP_PATH_PREFIX_INTERNAL, int_len) == 0) {
        // Match must be either the prefix itself or followed by '/'.
        if(path[int_len] == '\0' || path[int_len] == '/') {
            return MTP_STORAGE_ID_INTERNAL;
        }
    }

    size_t ext_len = strlen(MTP_PATH_PREFIX_EXTERNAL);
    if(strncmp(path, MTP_PATH_PREFIX_EXTERNAL, ext_len) == 0) {
        if(path[ext_len] == '\0' || path[ext_len] == '/') {
            return MTP_STORAGE_ID_EXTERNAL;
        }
    }

    return 0;
}

size_t mtp_storage_merge_path(const char* base, const char* name, char* out, size_t out_size) {
    if(!base || !name || !out || out_size == 0) return 0;

    size_t base_len = strlen(base);
    size_t name_len = strlen(name);

    // Strip trailing '/' from base (except when base is exactly "/").
    size_t base_copy_len = base_len;
    if(base_copy_len > 1 && base[base_copy_len - 1] == '/') {
        base_copy_len--;
    }

    // Strip leading '/' from name to avoid double slash.
    const char* name_p = name;
    while(*name_p == '/') {
        name_p++;
        name_len--;
    }

    // If base already ends with '/' (only possible when base == "/"), skip the
    // separator we'd otherwise insert — otherwise we'd produce "//x".
    bool need_sep = !(base_copy_len > 0 && base[base_copy_len - 1] == '/');

    size_t required = base_copy_len + (need_sep ? 1 : 0) + name_len + 1;
    if(required > out_size) return 0;

    memcpy(out, base, base_copy_len);
    size_t pos = base_copy_len;
    if(need_sep) {
        out[pos++] = '/';
    }
    memcpy(out + pos, name_p, name_len);
    pos += name_len;
    out[pos] = '\0';

    return pos;
}

const char* mtp_storage_basename(const char* path) {
    if(!path) return NULL;

    const char* last_slash = strrchr(path, '/');
    if(!last_slash) return path;
    return last_slash + 1;
}
