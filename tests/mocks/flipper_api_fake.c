#include "flipper_api_fake.h"
#include <stdlib.h>
#include <string.h>

#define MAX_ENTRIES   64
#define MAX_PATH      256
#define MAX_FILE_SIZE (256 * 1024)

typedef struct {
    bool in_use;
    bool is_dir;
    char path[MAX_PATH];
    uint8_t* contents;
    size_t size;
    size_t capacity;
} FakeEntry;

typedef struct {
    FakeEntry entries[MAX_ENTRIES];
    bool fail_open;
    bool fail_write;
} FakeFs;

struct Storage {
    int dummy;
};

static FakeFs g_fs;
static struct Storage g_storage_token; // address only, never dereferenced

struct File {
    bool open;
    bool is_dir;
    int entry_idx; // for files
    size_t pos; // file read/write cursor
    int dir_cursor; // for directory iteration
    char dir_path[MAX_PATH]; // for directory iteration
};

// --- Helpers ---------------------------------------------------------------

static FakeEntry* find_entry(const char* path) {
    if(!path) return NULL;
    for(int i = 0; i < MAX_ENTRIES; i++) {
        if(g_fs.entries[i].in_use && strcmp(g_fs.entries[i].path, path) == 0) {
            return &g_fs.entries[i];
        }
    }
    return NULL;
}

static int find_entry_index(const char* path) {
    if(!path) return -1;
    for(int i = 0; i < MAX_ENTRIES; i++) {
        if(g_fs.entries[i].in_use && strcmp(g_fs.entries[i].path, path) == 0) {
            return i;
        }
    }
    return -1;
}

static FakeEntry* alloc_entry(const char* path, bool is_dir) {
    for(int i = 0; i < MAX_ENTRIES; i++) {
        if(!g_fs.entries[i].in_use) {
            FakeEntry* e = &g_fs.entries[i];
            e->in_use = true;
            e->is_dir = is_dir;
            strncpy(e->path, path, MAX_PATH - 1);
            e->path[MAX_PATH - 1] = '\0';
            e->contents = NULL;
            e->size = 0;
            e->capacity = 0;
            return e;
        }
    }
    return NULL;
}

static void free_entry(FakeEntry* e) {
    if(!e) return;
    free(e->contents);
    memset(e, 0, sizeof(*e));
}

// Returns true if `parent_path` is the immediate parent of `child_path`.
static bool is_immediate_child(const char* parent_path, const char* child_path) {
    size_t plen = strlen(parent_path);

    // Special-case "/" — every absolute path that has exactly one '/'
    // (the leading one) is its immediate child.
    if(strcmp(parent_path, "/") == 0) {
        if(child_path[0] != '/') return false;
        const char* second_slash = strchr(child_path + 1, '/');
        return second_slash == NULL;
    }

    if(strncmp(parent_path, child_path, plen) != 0) return false;
    if(child_path[plen] != '/') return false;
    const char* rest = child_path + plen + 1;
    if(*rest == '\0') return false; // child equals parent + "/"
    return strchr(rest, '/') == NULL;
}

static const char* basename_of(const char* path) {
    const char* last = strrchr(path, '/');
    return last ? last + 1 : path;
}

// --- Record API ------------------------------------------------------------

void* furi_record_open(const char* record_name) {
    (void)record_name;
    return &g_storage_token;
}

void furi_record_close(const char* record_name) {
    (void)record_name;
}

// --- File alloc / free ----------------------------------------------------

File* storage_file_alloc(Storage* storage) {
    (void)storage;
    File* f = calloc(1, sizeof(File));
    return f;
}

void storage_file_free(File* file) {
    free(file);
}

// --- File ops -------------------------------------------------------------

bool storage_file_open(File* file, const char* path, FS_AccessMode am, FS_OpenMode om) {
    if(!file || !path) return false;
    if(g_fs.fail_open) return false;

    int idx = find_entry_index(path);
    FakeEntry* e = (idx >= 0) ? &g_fs.entries[idx] : NULL;

    if(om == FSOM_OPEN_EXISTING) {
        if(!e || e->is_dir) return false;
    } else if(om == FSOM_CREATE_ALWAYS) {
        if(e) {
            free(e->contents);
            e->contents = NULL;
            e->size = 0;
            e->capacity = 0;
        } else {
            e = alloc_entry(path, false);
            if(!e) return false;
            idx = (int)(e - g_fs.entries);
        }
    } else if(om == FSOM_OPEN_ALWAYS || om == FSOM_CREATE_NEW) {
        if(!e) {
            e = alloc_entry(path, false);
            if(!e) return false;
            idx = (int)(e - g_fs.entries);
        } else if(om == FSOM_CREATE_NEW) {
            return false;
        }
    } else if(om == FSOM_OPEN_APPEND) {
        if(!e) {
            e = alloc_entry(path, false);
            if(!e) return false;
            idx = (int)(e - g_fs.entries);
        }
    } else {
        return false;
    }

    (void)am;
    file->open = true;
    file->is_dir = false;
    file->entry_idx = idx;
    file->pos = (om == FSOM_OPEN_APPEND) ? e->size : 0;
    return true;
}

bool storage_file_close(File* file) {
    if(!file) return false;
    file->open = false;
    return true;
}

uint16_t storage_file_read(File* file, void* buffer, uint16_t size) {
    if(!file || !file->open || file->is_dir || !buffer) return 0;
    FakeEntry* e = &g_fs.entries[file->entry_idx];
    if(!e->in_use) return 0;
    size_t available = (file->pos < e->size) ? (e->size - file->pos) : 0;
    size_t to_read = (size < available) ? size : available;
    if(to_read > 0) {
        memcpy(buffer, e->contents + file->pos, to_read);
        file->pos += to_read;
    }
    return (uint16_t)to_read;
}

uint16_t storage_file_write(File* file, const void* buffer, uint16_t size) {
    if(!file || !file->open || file->is_dir || !buffer) return 0;
    if(g_fs.fail_write) return 0;
    FakeEntry* e = &g_fs.entries[file->entry_idx];
    if(!e->in_use) return 0;

    size_t needed = file->pos + size;
    if(needed > MAX_FILE_SIZE) return 0;

    if(needed > e->capacity) {
        size_t new_cap = e->capacity == 0 ? 64 : e->capacity * 2;
        while(new_cap < needed)
            new_cap *= 2;
        uint8_t* new_buf = realloc(e->contents, new_cap);
        if(!new_buf) return 0;
        e->contents = new_buf;
        e->capacity = new_cap;
    }

    memcpy(e->contents + file->pos, buffer, size);
    file->pos += size;
    if(file->pos > e->size) e->size = file->pos;
    return size;
}

bool storage_file_sync(File* file) {
    (void)file;
    return true;
}

uint64_t storage_file_size(File* file) {
    if(!file || !file->open || file->is_dir) return 0;
    FakeEntry* e = &g_fs.entries[file->entry_idx];
    return e->in_use ? e->size : 0;
}

// --- Directory ops --------------------------------------------------------

bool storage_dir_open(File* file, const char* path) {
    if(!file || !path) return false;

    // Root "/" is always a valid directory.
    if(strcmp(path, "/") != 0) {
        FakeEntry* e = find_entry(path);
        if(!e || !e->is_dir) return false;
    }

    file->open = true;
    file->is_dir = true;
    file->dir_cursor = 0;
    strncpy(file->dir_path, path, MAX_PATH - 1);
    file->dir_path[MAX_PATH - 1] = '\0';
    return true;
}

bool storage_dir_close(File* file) {
    if(!file) return false;
    file->open = false;
    return true;
}

bool storage_dir_read(File* file, FileInfo* info, char* name, uint16_t name_max) {
    if(!file || !file->open || !file->is_dir || !info || !name) return false;

    while(file->dir_cursor < MAX_ENTRIES) {
        int i = file->dir_cursor++;
        if(!g_fs.entries[i].in_use) continue;
        if(!is_immediate_child(file->dir_path, g_fs.entries[i].path)) continue;

        const char* bname = basename_of(g_fs.entries[i].path);
        strncpy(name, bname, name_max - 1);
        name[name_max - 1] = '\0';
        info->flags = g_fs.entries[i].is_dir ? FSF_DIRECTORY : 0;
        info->size = g_fs.entries[i].size;
        return true;
    }
    return false;
}

// --- Common ---------------------------------------------------------------

bool storage_file_exists(Storage* storage, const char* path) {
    (void)storage;
    FakeEntry* e = find_entry(path);
    return e && !e->is_dir;
}

bool storage_dir_exists(Storage* storage, const char* path) {
    (void)storage;
    if(strcmp(path, "/") == 0) return true;
    FakeEntry* e = find_entry(path);
    return e && e->is_dir;
}

bool storage_simply_mkdir(Storage* storage, const char* path) {
    (void)storage;
    if(find_entry(path)) return true; // idempotent
    FakeEntry* e = alloc_entry(path, true);
    return e != NULL;
}

FS_Error storage_common_remove(Storage* storage, const char* path) {
    (void)storage;
    FakeEntry* e = find_entry(path);
    if(!e) return FSE_NOT_EXIST;
    free_entry(e);
    return FSE_OK;
}

FS_Error storage_common_rename(Storage* storage, const char* old_path, const char* new_path) {
    (void)storage;
    FakeEntry* e = find_entry(old_path);
    if(!e) return FSE_NOT_EXIST;
    if(find_entry(new_path)) return FSE_EXIST;
    strncpy(e->path, new_path, MAX_PATH - 1);
    e->path[MAX_PATH - 1] = '\0';
    return FSE_OK;
}

FS_Error storage_common_stat(Storage* storage, const char* path, FileInfo* info) {
    (void)storage;
    if(!info) return FSE_INVALID_PARAMETER;
    FakeEntry* e = find_entry(path);
    if(!e) return FSE_NOT_EXIST;
    info->flags = e->is_dir ? FSF_DIRECTORY : 0;
    info->size = e->size;
    return FSE_OK;
}

FS_Error storage_sd_info(Storage* storage, SDInfo* info) {
    (void)storage;
    if(!info) return FSE_INVALID_PARAMETER;
    strncpy(info->fs_type, "FAT32", sizeof(info->fs_type) - 1);
    info->fs_type[sizeof(info->fs_type) - 1] = '\0';
    info->kb_total = 1024 * 1024; // 1 GB
    info->kb_free = 512 * 1024; // 512 MB
    info->cluster_size = 8;
    info->sector_size = 512;
    strncpy(info->label, "TEST_SD", sizeof(info->label) - 1);
    info->label[sizeof(info->label) - 1] = '\0';
    info->error = FSE_OK;
    return FSE_OK;
}

bool storage_simply_remove_recursive(Storage* storage, const char* path) {
    (void)storage;
    size_t plen = strlen(path);
    bool any = false;
    for(int i = 0; i < MAX_ENTRIES; i++) {
        FakeEntry* e = &g_fs.entries[i];
        if(!e->in_use) continue;
        bool match = strcmp(e->path, path) == 0 ||
                     (strncmp(e->path, path, plen) == 0 && e->path[plen] == '/');
        if(match) {
            free_entry(e);
            any = true;
        }
    }
    return any;
}

// --- Test hooks -----------------------------------------------------------

void flipper_fake_reset(void) {
    for(int i = 0; i < MAX_ENTRIES; i++) {
        if(g_fs.entries[i].in_use) {
            free(g_fs.entries[i].contents);
        }
    }
    memset(&g_fs, 0, sizeof(g_fs));
}

void flipper_fake_add_file(const char* path, const char* contents) {
    FakeEntry* e = alloc_entry(path, false);
    if(!e) return;
    size_t len = contents ? strlen(contents) : 0;
    if(len > 0) {
        e->contents = malloc(len);
        memcpy(e->contents, contents, len);
        e->size = len;
        e->capacity = len;
    }
}

void flipper_fake_add_dir(const char* path) {
    alloc_entry(path, true);
}

void flipper_fake_set_fail_open(bool fail) {
    g_fs.fail_open = fail;
}

void flipper_fake_set_fail_write(bool fail) {
    g_fs.fail_write = fail;
}

const char* flipper_fake_get_file_contents(const char* path) {
    FakeEntry* e = find_entry(path);
    if(!e || e->is_dir) return NULL;
    // contents is not null-terminated in storage; we add a sentinel for tests.
    static char buf[MAX_FILE_SIZE + 1];
    size_t n = e->size < MAX_FILE_SIZE ? e->size : MAX_FILE_SIZE;
    memcpy(buf, e->contents, n);
    buf[n] = '\0';
    return buf;
}
