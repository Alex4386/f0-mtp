#include "object_index.h"
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 64
#define LOAD_FACTOR 0.75

// Hash table entry
typedef struct IndexEntry {
    uint32_t handle;
    char* path;
    struct IndexEntry* next;  // Collision chain
} IndexEntry;

// Object index structure
struct MTPObjectIndex {
    IndexEntry** buckets;
    size_t capacity;
    size_t count;
    uint32_t next_handle;
};

// Hash function
static uint32_t hash_handle(uint32_t handle, size_t capacity) {
    return handle % capacity;
}

// Resize and rehash
static void resize_if_needed(MTPObjectIndex* index) {
    if((double)index->count / index->capacity <= LOAD_FACTOR) {
        return;
    }

    // Double capacity
    size_t new_capacity = index->capacity * 2;
    IndexEntry** new_buckets = calloc(new_capacity, sizeof(IndexEntry*));
    if(!new_buckets) {
        return;  // Allocation failed, continue with current capacity
    }

    // Rehash all entries
    for(size_t i = 0; i < index->capacity; i++) {
        IndexEntry* entry = index->buckets[i];
        while(entry) {
            IndexEntry* next = entry->next;

            uint32_t new_hash = hash_handle(entry->handle, new_capacity);
            entry->next = new_buckets[new_hash];
            new_buckets[new_hash] = entry;

            entry = next;
        }
    }

    free(index->buckets);
    index->buckets = new_buckets;
    index->capacity = new_capacity;
}

MTPObjectIndex* mtp_object_index_create(void) {
    MTPObjectIndex* index = malloc(sizeof(MTPObjectIndex));
    if(!index) {
        return NULL;
    }

    index->capacity = INITIAL_CAPACITY;
    index->count = 0;
    index->next_handle = 1;  // Start handles at 1

    index->buckets = calloc(index->capacity, sizeof(IndexEntry*));
    if(!index->buckets) {
        free(index);
        return NULL;
    }

    return index;
}

void mtp_object_index_destroy(MTPObjectIndex* index) {
    if(!index) {
        return;
    }

    // Free all entries
    for(size_t i = 0; i < index->capacity; i++) {
        IndexEntry* entry = index->buckets[i];
        while(entry) {
            IndexEntry* next = entry->next;
            free(entry->path);
            free(entry);
            entry = next;
        }
    }

    free(index->buckets);
    free(index);
}

uint32_t mtp_object_index_add(MTPObjectIndex* index, const char* path) {
    if(!index || !path) {
        return 0;
    }

    // Check if path already exists (deduplication)
    for(size_t i = 0; i < index->capacity; i++) {
        IndexEntry* entry = index->buckets[i];
        while(entry) {
            if(strcmp(entry->path, path) == 0) {
                return entry->handle;  // Return existing handle
            }
            entry = entry->next;
        }
    }

    // Allocate new handle
    uint32_t handle = index->next_handle++;

    // Create entry
    IndexEntry* entry = malloc(sizeof(IndexEntry));
    if(!entry) {
        return 0;
    }

    entry->handle = handle;
    entry->path = strdup(path);
    if(!entry->path) {
        free(entry);
        return 0;
    }

    // Insert into bucket
    uint32_t hash = hash_handle(handle, index->capacity);
    entry->next = index->buckets[hash];
    index->buckets[hash] = entry;

    index->count++;
    resize_if_needed(index);

    return handle;
}

const char* mtp_object_index_get_path(MTPObjectIndex* index, uint32_t handle) {
    if(!index || handle == 0) {
        return NULL;
    }

    uint32_t hash = hash_handle(handle, index->capacity);
    IndexEntry* entry = index->buckets[hash];

    while(entry) {
        if(entry->handle == handle) {
            return entry->path;
        }
        entry = entry->next;
    }

    return NULL;
}

bool mtp_object_index_update_path(MTPObjectIndex* index, uint32_t handle, const char* new_path) {
    if(!index || handle == 0 || !new_path) {
        return false;
    }

    uint32_t hash = hash_handle(handle, index->capacity);
    IndexEntry* entry = index->buckets[hash];

    while(entry) {
        if(entry->handle == handle) {
            char* new_path_copy = strdup(new_path);
            if(!new_path_copy) {
                return false;
            }

            free(entry->path);
            entry->path = new_path_copy;
            return true;
        }
        entry = entry->next;
    }

    return false;
}

bool mtp_object_index_remove(MTPObjectIndex* index, uint32_t handle) {
    if(!index || handle == 0) {
        return false;
    }

    uint32_t hash = hash_handle(handle, index->capacity);
    IndexEntry* entry = index->buckets[hash];
    IndexEntry* prev = NULL;

    while(entry) {
        if(entry->handle == handle) {
            // Remove from chain
            if(prev) {
                prev->next = entry->next;
            } else {
                index->buckets[hash] = entry->next;
            }

            free(entry->path);
            free(entry);
            index->count--;
            return true;
        }

        prev = entry;
        entry = entry->next;
    }

    return false;
}

bool mtp_object_index_contains(MTPObjectIndex* index, uint32_t handle) {
    return mtp_object_index_get_path(index, handle) != NULL;
}

void mtp_object_index_clear(MTPObjectIndex* index) {
    if(!index) {
        return;
    }

    // Free all entries
    for(size_t i = 0; i < index->capacity; i++) {
        IndexEntry* entry = index->buckets[i];
        while(entry) {
            IndexEntry* next = entry->next;
            free(entry->path);
            free(entry);
            entry = next;
        }
        index->buckets[i] = NULL;
    }

    index->count = 0;
    // Note: next_handle is NOT reset, to avoid handle reuse
}

size_t mtp_object_index_count(MTPObjectIndex* index) {
    return index ? index->count : 0;
}

void mtp_object_index_foreach(
    MTPObjectIndex* index,
    MTPObjectIndexIterator callback,
    void* user_data
) {
    if(!index || !callback) {
        return;
    }

    for(size_t i = 0; i < index->capacity; i++) {
        IndexEntry* entry = index->buckets[i];
        while(entry) {
            callback(entry->handle, entry->path, user_data);
            entry = entry->next;
        }
    }
}
