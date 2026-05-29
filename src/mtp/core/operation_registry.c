#include "operation_registry.h"
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 32

// Registry entry (stores copy of operation entry)
typedef struct RegistryEntry {
    MTPOperationEntry entry;
    struct RegistryEntry* next;
} RegistryEntry;

// Operation registry structure
struct MTPOperationRegistry {
    RegistryEntry** buckets;
    size_t capacity;
    size_t count;
};

// Hash function for operation codes
static uint32_t hash_op_code(uint16_t op_code, size_t capacity) {
    return op_code % capacity;
}

MTPOperationRegistry* mtp_operation_registry_create(void) {
    MTPOperationRegistry* registry = malloc(sizeof(MTPOperationRegistry));
    if(!registry) {
        return NULL;
    }

    registry->capacity = INITIAL_CAPACITY;
    registry->count = 0;

    registry->buckets = calloc(registry->capacity, sizeof(RegistryEntry*));
    if(!registry->buckets) {
        free(registry);
        return NULL;
    }

    return registry;
}

void mtp_operation_registry_destroy(MTPOperationRegistry* registry) {
    if(!registry) {
        return;
    }

    // Free all entries
    for(size_t i = 0; i < registry->capacity; i++) {
        RegistryEntry* entry = registry->buckets[i];
        while(entry) {
            RegistryEntry* next = entry->next;
            free(entry);
            entry = next;
        }
    }

    free(registry->buckets);
    free(registry);
}

bool mtp_operation_registry_add(MTPOperationRegistry* registry, const MTPOperationEntry* entry) {
    if(!registry || !entry || !entry->handler) {
        return false;
    }

    // Check if operation already registered
    uint32_t hash = hash_op_code(entry->op_code, registry->capacity);
    RegistryEntry* existing = registry->buckets[hash];

    while(existing) {
        if(existing->entry.op_code == entry->op_code) {
            // Already registered - update handler
            existing->entry.handler = entry->handler;
            existing->entry.name = entry->name;
            return true;
        }
        existing = existing->next;
    }

    // Create new entry
    RegistryEntry* new_entry = malloc(sizeof(RegistryEntry));
    if(!new_entry) {
        return false;
    }

    new_entry->entry = *entry;
    new_entry->next = registry->buckets[hash];
    registry->buckets[hash] = new_entry;
    registry->count++;

    return true;
}

MTPOperationHandler mtp_operation_registry_find(MTPOperationRegistry* registry, uint16_t op_code) {
    if(!registry) {
        return NULL;
    }

    uint32_t hash = hash_op_code(op_code, registry->capacity);
    RegistryEntry* entry = registry->buckets[hash];

    while(entry) {
        if(entry->entry.op_code == op_code) {
            return entry->entry.handler;
        }
        entry = entry->next;
    }

    return NULL;
}

const char* mtp_operation_registry_get_name(MTPOperationRegistry* registry, uint16_t op_code) {
    if(!registry) {
        return NULL;
    }

    uint32_t hash = hash_op_code(op_code, registry->capacity);
    RegistryEntry* entry = registry->buckets[hash];

    while(entry) {
        if(entry->entry.op_code == op_code) {
            return entry->entry.name;
        }
        entry = entry->next;
    }

    return NULL;
}

bool mtp_operation_registry_contains(MTPOperationRegistry* registry, uint16_t op_code) {
    return mtp_operation_registry_find(registry, op_code) != NULL;
}

size_t mtp_operation_registry_count(MTPOperationRegistry* registry) {
    return registry ? registry->count : 0;
}

void mtp_operation_registry_foreach(
    MTPOperationRegistry* registry,
    MTPOperationIterator callback,
    void* user_data) {
    if(!registry || !callback) {
        return;
    }

    for(size_t i = 0; i < registry->capacity; i++) {
        RegistryEntry* entry = registry->buckets[i];
        while(entry) {
            callback(&entry->entry, user_data);
            entry = entry->next;
        }
    }
}
