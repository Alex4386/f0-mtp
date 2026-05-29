#pragma once

#include "operation.h"

// Operation registry (replaces switch statements!)
typedef struct MTPOperationRegistry MTPOperationRegistry;

// Create/destroy
MTPOperationRegistry* mtp_operation_registry_create(void);
void mtp_operation_registry_destroy(MTPOperationRegistry* registry);

// Register an operation handler
bool mtp_operation_registry_add(MTPOperationRegistry* registry, const MTPOperationEntry* entry);

// Find operation handler by operation code
// Returns NULL if operation not found
MTPOperationHandler mtp_operation_registry_find(MTPOperationRegistry* registry, uint16_t op_code);

// Get operation name (for logging/debugging)
const char* mtp_operation_registry_get_name(MTPOperationRegistry* registry, uint16_t op_code);

// Check if operation is registered
bool mtp_operation_registry_contains(MTPOperationRegistry* registry, uint16_t op_code);

// Get count of registered operations
size_t mtp_operation_registry_count(MTPOperationRegistry* registry);

// Iteration (for debugging)
typedef void (*MTPOperationIterator)(const MTPOperationEntry* entry, void* user_data);
void mtp_operation_registry_foreach(
    MTPOperationRegistry* registry,
    MTPOperationIterator callback,
    void* user_data);
