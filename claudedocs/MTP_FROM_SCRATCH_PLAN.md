# MTP Implementation from Scratch - Architecture & Design Plan

## Executive Summary

This document outlines a complete architecture for implementing MTP (Media Transfer Protocol) from scratch using SOLID principles, modern C design patterns, and lessons learned from the current implementation analysis.

**Goal**: Build a maintainable, testable, and extensible MTP implementation that:
- ✅ Follows SOLID principles
- ✅ Supports concurrent operations
- ✅ Has clear separation of concerns
- ✅ Is testable in isolation
- ✅ Maintains high performance
- ✅ Works within Flipper Zero constraints

**Related Documents**:
- [MTP_CONTROL_FLOW_ANALYSIS.md](MTP_CONTROL_FLOW_ANALYSIS.md) - Current implementation control flow
- [MTP_IMPLEMENTATION_ANALYSIS.md](MTP_IMPLEMENTATION_ANALYSIS.md) - Current implementation deep dive

---

## Table of Contents

1. [Design Principles](#design-principles)
2. [System Architecture](#system-architecture)
3. [Layer Breakdown](#layer-breakdown)
4. [Core Abstractions](#core-abstractions)
5. [Data Structures](#data-structures)
6. [Operation Handler System](#operation-handler-system)
7. [Transfer State Management](#transfer-state-management)
8. [Storage Abstraction](#storage-abstraction)
9. [Memory Management Strategy](#memory-management-strategy)
10. [Error Handling Framework](#error-handling-framework)
11. [Testing Strategy](#testing-strategy)
12. [Implementation Roadmap](#implementation-roadmap)
13. [File Organization](#file-organization)

---

## Design Principles

### SOLID Compliance

**Single Responsibility Principle (SRP)**:
- Each module has ONE reason to change
- Protocol parsing ≠ business logic ≠ storage access
- Clear boundaries between layers

**Open/Closed Principle (OCP)**:
- New MTP operations added via registration, not modification
- New storage backends via interface implementation
- Extension points designed upfront

**Liskov Substitution Principle (LSP)**:
- All operation handlers interchangeable through interface
- All storage implementations interchangeable through interface
- Polymorphism without surprises

**Interface Segregation Principle (ISP)**:
- Small, focused interfaces
- Operations grouped by capability (read, write, control)
- Clients depend only on what they use

**Dependency Inversion Principle (DIP)**:
- High-level modules depend on abstractions
- Storage, USB, operations are interfaces
- Concrete implementations injected at initialization

---

### Additional Principles

**Clear Ownership**:
- Every allocated resource has one owner
- Ownership transfer is explicit
- RAII-style patterns where possible

**Fail-Fast**:
- Validate inputs at boundaries
- Assertions for invariants
- Early error detection

**Progressive Enhancement**:
- Core functionality first
- Optional features layered on top
- Graceful degradation

**Testability First**:
- Interfaces designed for mocking
- Side effects isolated
- State externalized

---

## System Architecture

### Layered Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                         │
│                  (UI, Session Management)                    │
└────────────────────────────┬────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────┐
│                   MTP Protocol Layer                         │
│  ┌───────────────┬──────────────────┬────────────────────┐  │
│  │   Dispatcher  │  State Machine   │  Response Builder  │  │
│  └───────────────┴──────────────────┴────────────────────┘  │
└────────────────────────────┬────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────┐
│                 Operation Handler Layer                      │
│  ┌──────────┬──────────┬──────────┬──────────┬──────────┐  │
│  │  Device  │ Storage  │  Object  │ Property │ Transfer │  │
│  │   Ops    │   Ops    │   Ops    │   Ops    │   Ops    │  │
│  └──────────┴──────────┴──────────┴──────────┴──────────┘  │
└────────────────────────────┬────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────┐
│                 Abstraction Layer                            │
│  ┌────────────────┬────────────────┬────────────────────┐  │
│  │    Storage     │  Object Index  │  String Encoding   │  │
│  │   Interface    │   (Handles)    │     Utilities      │  │
│  └────────────────┴────────────────┴────────────────────┘  │
└────────────────────────────┬────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────┐
│                  Platform Layer                              │
│  ┌────────────────┬────────────────┬────────────────────┐  │
│  │  Flipper USB   │ Flipper Storage│  System Services   │  │
│  └────────────────┴────────────────┴────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

### Component Interaction Flow

```
[USB Packet]
    ↓
[USB Handler] → [Packet Parser]
    ↓
[Dispatcher] → [Operation Registry] → [Operation Handler]
    ↓                                        ↓
[State Manager]                    [Storage Interface]
    ↓                                        ↓
[Response Builder] ← [Operation Handler] ← [Storage Backend]
    ↓
[USB Transmit]
```

---

## Layer Breakdown

### Layer 1: Platform Layer (Adapter)

**Responsibility**: Interface with Flipper Zero hardware and OS

**Components**:

```c
// platform/usb_platform.h
typedef struct USBPlatform USBPlatform;

typedef struct {
    void (*init)(USBPlatform* platform, void* usb_dev);
    void (*deinit)(USBPlatform* platform);
    int (*ep_write)(USBPlatform* platform, uint8_t ep_addr, const void* buf, size_t len);
    int (*ep_read)(USBPlatform* platform, uint8_t ep_addr, void* buf, size_t len);
} USBPlatformOps;

// platform/storage_platform.h
typedef struct StoragePlatform StoragePlatform;

typedef struct {
    void* (*file_open)(StoragePlatform* platform, const char* path, int mode);
    int (*file_read)(void* file, void* buf, size_t size);
    int (*file_write)(void* file, const void* buf, size_t size);
    void (*file_close)(void* file);
    // ... other operations
} StoragePlatformOps;
```

**Benefits**:
- ✅ Platform-specific code isolated
- ✅ Easy to mock for testing
- ✅ Can support multiple platforms (not just Flipper)

---

### Layer 2: Abstraction Layer

**Responsibility**: Platform-agnostic interfaces for storage, indexing, utilities

#### Storage Interface

```c
// core/storage_interface.h
typedef struct MTPStorage MTPStorage;

typedef enum {
    MTP_STORAGE_MODE_READ = 1,
    MTP_STORAGE_MODE_WRITE = 2,
    MTP_STORAGE_MODE_RW = 3,
} MTPStorageMode;

typedef struct MTPFile MTPFile;
typedef struct MTPDirectory MTPDirectory;
typedef struct MTPFileInfo MTPFileInfo;

// Storage interface (abstract)
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

    // Storage info
    bool (*get_info)(MTPStorage* storage, uint64_t* total, uint64_t* free);

    // Lifecycle
    void (*destroy)(MTPStorage* storage);
} MTPStorageVTable;

struct MTPStorage {
    const MTPStorageVTable* vtable;
    void* context;  // Implementation-specific data
};
```

**Concrete Implementation**:

```c
// platform/flipper_storage.h
MTPStorage* mtp_storage_create_flipper(Storage* flipper_storage);
```

---

#### Object Index (Handle Manager)

```c
// core/object_index.h
typedef struct MTPObjectIndex MTPObjectIndex;

// Create/destroy
MTPObjectIndex* mtp_object_index_create(void);
void mtp_object_index_destroy(MTPObjectIndex* index);

// Handle operations
uint32_t mtp_object_index_add(MTPObjectIndex* index, const char* path);
const char* mtp_object_index_get_path(MTPObjectIndex* index, uint32_t handle);
bool mtp_object_index_update_path(MTPObjectIndex* index, uint32_t handle, const char* new_path);
bool mtp_object_index_remove(MTPObjectIndex* index, uint32_t handle);

// Batch operations
void mtp_object_index_clear(MTPObjectIndex* index);
size_t mtp_object_index_count(MTPObjectIndex* index);

// Iteration (for debugging/stats)
typedef void (*MTPObjectIndexIterator)(uint32_t handle, const char* path, void* user_data);
void mtp_object_index_foreach(MTPObjectIndex* index, MTPObjectIndexIterator callback, void* user_data);
```

**Implementation** (hash table):

```c
// core/object_index.c
struct MTPObjectIndex {
    // Hash table: handle → path
    // Could use uthash, or simple custom implementation
    size_t capacity;
    size_t count;
    struct IndexEntry {
        uint32_t handle;
        char* path;
        struct IndexEntry* next;  // Collision chain
    }** buckets;

    uint32_t next_handle;  // Auto-increment
};

// O(1) average case lookup/insert/update/remove
```

---

#### String Utilities

```c
// core/mtp_string.h

// MTP string encoding/decoding
typedef struct {
    uint16_t length;  // Length including null terminator
    uint16_t* data;   // UTF-16LE characters
} MTPString;

// Encode: ASCII/UTF-8 → MTP string (UTF-16LE)
MTPString* mtp_string_encode(const char* utf8_str);

// Decode: MTP string → UTF-8
char* mtp_string_decode(const uint8_t* buffer, size_t buffer_size);

// Check if contains non-ASCII
bool mtp_string_has_unicode(const uint8_t* buffer);

// Write to buffer (for response construction)
size_t mtp_string_write(uint8_t* buffer, const char* utf8_str);

// Destroy
void mtp_string_destroy(MTPString* str);
```

---

### Layer 3: Operation Handler Layer

**Responsibility**: Implement MTP operations

#### Operation Interface

```c
// core/operation.h
typedef struct MTPContext MTPContext;
typedef struct MTPRequest MTPRequest;
typedef struct MTPResponse MTPResponse;

// Operation handler signature
typedef void (*MTPOperationHandler)(
    MTPContext* ctx,
    const MTPRequest* request,
    MTPResponse* response
);

// Operation registration entry
typedef struct {
    uint16_t op_code;
    const char* name;
    MTPOperationHandler handler;
} MTPOperationEntry;

// Registry
typedef struct MTPOperationRegistry MTPOperationRegistry;

MTPOperationRegistry* mtp_operation_registry_create(void);
void mtp_operation_registry_destroy(MTPOperationRegistry* registry);

void mtp_operation_registry_add(
    MTPOperationRegistry* registry,
    const MTPOperationEntry* entry
);

MTPOperationHandler mtp_operation_registry_find(
    MTPOperationRegistry* registry,
    uint16_t op_code
);
```

---

#### Request/Response Structures

```c
// core/operation.h

// Request structure
struct MTPRequest {
    uint16_t op_code;
    uint32_t transaction_id;
    uint32_t params[5];

    // For data phase operations
    const uint8_t* data;
    size_t data_size;
};

// Response structure
struct MTPResponse {
    uint16_t response_code;  // MTP_RESP_OK, etc.
    uint32_t params[5];

    // Data response (owned by response, will be freed)
    uint8_t* data;
    size_t data_size;

    // Streaming response (callback-based)
    bool is_streaming;
    void* stream_context;
    int (*stream_callback)(void* ctx, uint8_t* buffer, size_t size);
    size_t stream_total_size;
};

// Response builders
MTPResponse* mtp_response_create(void);
void mtp_response_destroy(MTPResponse* response);

void mtp_response_set_code(MTPResponse* response, uint16_t code);
void mtp_response_add_param(MTPResponse* response, uint32_t param);
void mtp_response_set_data(MTPResponse* response, uint8_t* data, size_t size);

void mtp_response_set_stream(
    MTPResponse* response,
    void* context,
    int (*callback)(void* ctx, uint8_t* buffer, size_t size),
    size_t total_size
);
```

---

#### MTP Context (Dependency Injection)

```c
// core/context.h
struct MTPContext {
    // Storage interface
    MTPStorage* storage;

    // Object index
    MTPObjectIndex* object_index;

    // Session state
    struct {
        uint32_t session_id;
        bool is_open;
    } session;

    // Transfer state manager
    MTPTransferStateManager* transfer_manager;

    // Configuration
    struct {
        const char* device_name;
        const char* manufacturer;
        const char* model;
        const char* serial;
        const char* firmware_version;
    } device_info;

    // User data (for platform-specific extensions)
    void* user_data;
};

MTPContext* mtp_context_create(MTPStorage* storage);
void mtp_context_destroy(MTPContext* ctx);
```

---

#### Example Operation Handler

```c
// operations/get_device_info.c
void mtp_op_get_device_info(
    MTPContext* ctx,
    const MTPRequest* request,
    MTPResponse* response
) {
    // Allocate buffer
    uint8_t* buffer = malloc(MTP_BUFFER_SIZE);
    uint8_t* ptr = buffer;

    // Build device info structure
    *(uint16_t*)ptr = MTP_STANDARD_VERSION;
    ptr += 2;

    *(uint32_t*)ptr = MTP_VENDOR_EXTENSION_ID;
    ptr += 4;

    // ... (build rest of structure)

    // Write strings
    ptr += mtp_string_write(ptr, ctx->device_info.manufacturer);
    ptr += mtp_string_write(ptr, ctx->device_info.model);
    ptr += mtp_string_write(ptr, ctx->device_info.firmware_version);
    ptr += mtp_string_write(ptr, ctx->device_info.serial);

    // Set response
    size_t total_size = ptr - buffer;
    mtp_response_set_code(response, MTP_RESP_OK);
    mtp_response_set_data(response, buffer, total_size);

    // Buffer ownership transferred to response, will be freed by response_destroy()
}

// Registration
static const MTPOperationEntry entry = {
    .op_code = MTP_OP_GET_DEVICE_INFO,
    .name = "GetDeviceInfo",
    .handler = mtp_op_get_device_info
};
```

---

### Layer 4: MTP Protocol Layer

**Responsibility**: MTP protocol state machine, packet parsing, response transmission

#### Packet Parser

```c
// protocol/packet.h
typedef struct {
    uint32_t length;
    uint16_t type;
    uint16_t code;  // Operation or response code
    uint32_t transaction_id;
} MTPPacketHeader;

typedef struct {
    MTPPacketHeader header;
    uint32_t params[5];
    const uint8_t* data;  // Points to data after header
    size_t data_size;
} MTPPacket;

// Parse packet from buffer
bool mtp_packet_parse(const uint8_t* buffer, size_t size, MTPPacket* packet);

// Validate packet
bool mtp_packet_validate(const MTPPacket* packet);
```

---

#### Protocol State Machine

```c
// protocol/state_machine.h
typedef enum {
    MTP_PROTO_STATE_IDLE,
    MTP_PROTO_STATE_COMMAND_RECEIVED,
    MTP_PROTO_STATE_AWAITING_DATA,
    MTP_PROTO_STATE_PROCESSING,
    MTP_PROTO_STATE_SENDING_RESPONSE,
    MTP_PROTO_STATE_ERROR
} MTPProtocolState;

typedef struct MTPProtocolStateMachine MTPProtocolStateMachine;

MTPProtocolStateMachine* mtp_protocol_sm_create(MTPContext* ctx);
void mtp_protocol_sm_destroy(MTPProtocolStateMachine* sm);

// Event handlers
void mtp_protocol_sm_handle_packet(
    MTPProtocolStateMachine* sm,
    const uint8_t* buffer,
    size_t size
);

// State queries
MTPProtocolState mtp_protocol_sm_get_state(MTPProtocolStateMachine* sm);
bool mtp_protocol_sm_is_busy(MTPProtocolStateMachine* sm);
```

---

#### Dispatcher

```c
// protocol/dispatcher.h
typedef struct MTPDispatcher MTPDispatcher;

MTPDispatcher* mtp_dispatcher_create(
    MTPContext* ctx,
    MTPOperationRegistry* registry
);

void mtp_dispatcher_destroy(MTPDispatcher* dispatcher);

// Dispatch operation
void mtp_dispatcher_execute(
    MTPDispatcher* dispatcher,
    const MTPRequest* request,
    MTPResponse* response
);
```

**Implementation**:

```c
void mtp_dispatcher_execute(
    MTPDispatcher* dispatcher,
    const MTPRequest* request,
    MTPResponse* response
) {
    // Find handler
    MTPOperationHandler handler = mtp_operation_registry_find(
        dispatcher->registry,
        request->op_code
    );

    if(!handler) {
        mtp_response_set_code(response, MTP_RESP_OPERATION_NOT_SUPPORTED);
        return;
    }

    // Session validation
    if(request->op_code != MTP_OP_GET_DEVICE_INFO &&
       request->op_code != MTP_OP_OPEN_SESSION) {
        if(!dispatcher->ctx->session.is_open) {
            mtp_response_set_code(response, MTP_RESP_SESSION_NOT_OPEN);
            return;
        }
    }

    // Execute handler
    handler(dispatcher->ctx, request, response);
}
```

---

#### Response Transmitter

```c
// protocol/transmitter.h
typedef struct MTPTransmitter MTPTransmitter;

MTPTransmitter* mtp_transmitter_create(USBPlatform* usb);
void mtp_transmitter_destroy(MTPTransmitter* transmitter);

// Send response
void mtp_transmitter_send(
    MTPTransmitter* transmitter,
    uint16_t type,  // MTP_TYPE_DATA or MTP_TYPE_RESPONSE
    uint16_t code,
    uint32_t transaction_id,
    const MTPResponse* response
);
```

**Implementation** (streaming support):

```c
void mtp_transmitter_send(
    MTPTransmitter* tx,
    uint16_t type,
    uint16_t code,
    uint32_t transaction_id,
    const MTPResponse* response
) {
    if(response->is_streaming) {
        // Streaming response
        mtp_transmitter_send_stream(
            tx, type, code, transaction_id,
            response->stream_context,
            response->stream_callback,
            response->stream_total_size
        );
    } else {
        // Buffer response
        mtp_transmitter_send_buffer(
            tx, type, code, transaction_id,
            response->data,
            response->data_size
        );
    }
}
```

---

## Core Abstractions

### Transfer State Management

**Problem**: Multi-packet operations (SendObjectInfo, SendObject) need to maintain state across packets.

**Solution**: Transfer state objects managed by state manager

```c
// core/transfer_state.h

typedef enum {
    MTP_TRANSFER_TYPE_SEND_OBJECT_INFO,
    MTP_TRANSFER_TYPE_SEND_OBJECT,
    MTP_TRANSFER_TYPE_SET_OBJECT_PROP
} MTPTransferType;

// Abstract transfer state
typedef struct MTPTransferState MTPTransferState;

typedef struct {
    // Lifecycle
    void (*destroy)(MTPTransferState* state);

    // Data handling
    bool (*handle_data)(MTPTransferState* state, const uint8_t* data, size_t size);
    bool (*is_complete)(const MTPTransferState* state);

    // Finalization
    uint16_t (*finalize)(MTPTransferState* state, MTPContext* ctx, MTPResponse* response);
} MTPTransferStateVTable;

struct MTPTransferState {
    const MTPTransferStateVTable* vtable;
    MTPTransferType type;
    uint32_t transaction_id;
    void* impl;  // Implementation-specific data
};
```

---

#### Transfer State Manager

```c
// core/transfer_state_manager.h
typedef struct MTPTransferStateManager MTPTransferStateManager;

MTPTransferStateManager* mtp_transfer_state_manager_create(void);
void mtp_transfer_state_manager_destroy(MTPTransferStateManager* manager);

// Create transfer state
MTPTransferState* mtp_transfer_state_manager_create_transfer(
    MTPTransferStateManager* manager,
    MTPTransferType type,
    uint32_t transaction_id,
    const uint32_t params[5]
);

// Get active transfer
MTPTransferState* mtp_transfer_state_manager_get_active(
    MTPTransferStateManager* manager
);

// Complete and destroy transfer
void mtp_transfer_state_manager_complete(
    MTPTransferStateManager* manager,
    MTPTransferState* state
);
```

---

#### Concrete Transfer State: SendObject

```c
// operations/transfer_states/send_object_state.c

typedef struct {
    MTPTransferState base;

    uint32_t object_handle;
    MTPFile* file;
    uint64_t bytes_written;
    uint64_t total_bytes;
    uint64_t last_sync_offset;
} SendObjectState;

static bool send_object_handle_data(MTPTransferState* base, const uint8_t* data, size_t size) {
    SendObjectState* state = (SendObjectState*)base->impl;

    // Write to file
    int written = state->file->vtable->file_write(state->file, data, size);
    if(written != size) {
        return false;  // Write error
    }

    state->bytes_written += written;

    // Periodic sync
    if(state->bytes_written - state->last_sync_offset >= MTP_FILE_SYNC_INTERVAL) {
        state->file->vtable->file_sync(state->file);
        state->last_sync_offset = state->bytes_written;
    }

    return true;
}

static bool send_object_is_complete(const MTPTransferState* base) {
    SendObjectState* state = (SendObjectState*)base->impl;
    return state->bytes_written >= state->total_bytes;
}

static uint16_t send_object_finalize(MTPTransferState* base, MTPContext* ctx, MTPResponse* response) {
    SendObjectState* state = (SendObjectState*)base->impl;

    // Final sync
    state->file->vtable->file_sync(state->file);
    state->file->vtable->file_close(state->file);

    // Success
    mtp_response_set_code(response, MTP_RESP_OK);
    return MTP_RESP_OK;
}

static void send_object_destroy(MTPTransferState* base) {
    SendObjectState* state = (SendObjectState*)base->impl;
    if(state->file) {
        state->file->vtable->file_close(state->file);
    }
    free(state);
}

static const MTPTransferStateVTable send_object_vtable = {
    .destroy = send_object_destroy,
    .handle_data = send_object_handle_data,
    .is_complete = send_object_is_complete,
    .finalize = send_object_finalize
};

// Factory
MTPTransferState* mtp_transfer_state_create_send_object(
    uint32_t transaction_id,
    uint32_t handle,
    MTPFile* file,
    uint64_t total_bytes
) {
    SendObjectState* state = malloc(sizeof(SendObjectState));

    state->base.vtable = &send_object_vtable;
    state->base.type = MTP_TRANSFER_TYPE_SEND_OBJECT;
    state->base.transaction_id = transaction_id;
    state->base.impl = state;

    state->object_handle = handle;
    state->file = file;
    state->bytes_written = 0;
    state->total_bytes = total_bytes;
    state->last_sync_offset = 0;

    return &state->base;
}
```

---

## Data Structures

### Object Index Implementation (Hash Table)

```c
// core/object_index.c

#define INITIAL_CAPACITY 64
#define LOAD_FACTOR 0.75

typedef struct IndexEntry {
    uint32_t handle;
    char* path;
    struct IndexEntry* next;  // Collision chain
} IndexEntry;

struct MTPObjectIndex {
    IndexEntry** buckets;
    size_t capacity;
    size_t count;
    uint32_t next_handle;
};

static uint32_t hash_handle(uint32_t handle, size_t capacity) {
    // Simple hash function
    return handle % capacity;
}

static void resize_if_needed(MTPObjectIndex* index) {
    if((double)index->count / index->capacity > LOAD_FACTOR) {
        // Resize and rehash
        size_t new_capacity = index->capacity * 2;
        IndexEntry** new_buckets = calloc(new_capacity, sizeof(IndexEntry*));

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
}

uint32_t mtp_object_index_add(MTPObjectIndex* index, const char* path) {
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
    entry->handle = handle;
    entry->path = strdup(path);

    // Insert into bucket
    uint32_t hash = hash_handle(handle, index->capacity);
    entry->next = index->buckets[hash];
    index->buckets[hash] = entry;

    index->count++;
    resize_if_needed(index);

    return handle;
}

const char* mtp_object_index_get_path(MTPObjectIndex* index, uint32_t handle) {
    uint32_t hash = hash_handle(handle, index->capacity);
    IndexEntry* entry = index->buckets[hash];

    while(entry) {
        if(entry->handle == handle) {
            return entry->path;
        }
        entry = entry->next;
    }

    return NULL;  // Not found
}

// O(1) average case for all operations!
```

**Performance**:
- ✅ **Insert**: O(1) average
- ✅ **Lookup**: O(1) average
- ✅ **Update**: O(1) average
- ✅ **Remove**: O(1) average

vs. Current implementation: O(n) for all operations

---

### Memory Pool for Common Allocations

```c
// core/memory_pool.h
typedef struct MTPMemoryPool MTPMemoryPool;

// Create pool for fixed-size allocations
MTPMemoryPool* mtp_memory_pool_create(size_t block_size, size_t initial_blocks);
void mtp_memory_pool_destroy(MTPMemoryPool* pool);

// Allocate/free from pool
void* mtp_memory_pool_alloc(MTPMemoryPool* pool);
void mtp_memory_pool_free(MTPMemoryPool* pool, void* ptr);

// Stats
size_t mtp_memory_pool_allocated(MTPMemoryPool* pool);
size_t mtp_memory_pool_available(MTPMemoryPool* pool);
```

**Use Cases**:
- Response structures
- Transfer state objects
- Small buffers

**Benefits**:
- ✅ Faster allocation (no malloc overhead)
- ✅ Reduced fragmentation
- ✅ Predictable memory usage

---

## Operation Handler System

### Operation Categories

**Device Operations**:
```c
// operations/device/
- get_device_info.c
- open_session.c
- close_session.c
```

**Storage Operations**:
```c
// operations/storage/
- get_storage_ids.c
- get_storage_info.c
```

**Object Operations**:
```c
// operations/objects/
- get_object_handles.c
- get_object_info.c
- get_object.c
- send_object_info.c
- send_object.c
- delete_object.c
- move_object.c
```

**Property Operations**:
```c
// operations/properties/
- get_object_props_supported.c
- get_object_prop_value.c
- set_object_prop_value.c
- get_device_prop_value.c
- get_device_prop_desc.c
```

---

### Operation Registration Pattern

```c
// operations/registry_init.c
void mtp_operation_registry_init_default(MTPOperationRegistry* registry) {
    // Device operations
    extern const MTPOperationEntry mtp_op_entry_get_device_info;
    extern const MTPOperationEntry mtp_op_entry_open_session;
    extern const MTPOperationEntry mtp_op_entry_close_session;

    mtp_operation_registry_add(registry, &mtp_op_entry_get_device_info);
    mtp_operation_registry_add(registry, &mtp_op_entry_open_session);
    mtp_operation_registry_add(registry, &mtp_op_entry_close_session);

    // Storage operations
    extern const MTPOperationEntry mtp_op_entry_get_storage_ids;
    extern const MTPOperationEntry mtp_op_entry_get_storage_info;

    mtp_operation_registry_add(registry, &mtp_op_entry_get_storage_ids);
    mtp_operation_registry_add(registry, &mtp_op_entry_get_storage_info);

    // ... register all operations
}
```

**Benefits**:
- ✅ OCP: Adding operation = adding file + registration line
- ✅ Testable: Can create registry with subset of operations
- ✅ Discoverable: All operations in one init function

---

### Helper Macros for Operation Handlers

```c
// core/operation_helpers.h

// Simplified operation handler declaration
#define MTP_OPERATION_HANDLER(name) \
    void mtp_op_##name(MTPContext* ctx, const MTPRequest* request, MTPResponse* response)

// Operation entry definition
#define MTP_OPERATION_ENTRY(op_code, name) \
    const MTPOperationEntry mtp_op_entry_##name = { \
        .op_code = op_code, \
        .name = #name, \
        .handler = mtp_op_##name \
    }

// Session validation
#define MTP_REQUIRE_SESSION(ctx, response) \
    if(!ctx->session.is_open) { \
        mtp_response_set_code(response, MTP_RESP_SESSION_NOT_OPEN); \
        return; \
    }

// Handle validation
#define MTP_REQUIRE_VALID_HANDLE(ctx, handle, response) \
    if(!mtp_object_index_get_path(ctx->object_index, handle)) { \
        mtp_response_set_code(response, MTP_RESP_INVALID_OBJECT_HANDLE); \
        return; \
    }
```

**Usage**:

```c
// operations/objects/delete_object.c
#include "core/operation_helpers.h"

MTP_OPERATION_HANDLER(delete_object) {
    MTP_REQUIRE_SESSION(ctx, response);

    uint32_t handle = request->params[0];
    MTP_REQUIRE_VALID_HANDLE(ctx, handle, response);

    const char* path = mtp_object_index_get_path(ctx->object_index, handle);

    if(!ctx->storage->vtable->remove(ctx->storage, path)) {
        mtp_response_set_code(response, MTP_RESP_GENERAL_ERROR);
        return;
    }

    mtp_object_index_remove(ctx->object_index, handle);
    mtp_response_set_code(response, MTP_RESP_OK);
}

MTP_OPERATION_ENTRY(MTP_OP_DELETE_OBJECT, delete_object);
```

---

## Error Handling Framework

### Error Code System

```c
// core/error.h
typedef enum {
    MTP_ERROR_NONE = 0,

    // General errors
    MTP_ERROR_INVALID_ARGUMENT,
    MTP_ERROR_OUT_OF_MEMORY,
    MTP_ERROR_INTERNAL,

    // MTP protocol errors
    MTP_ERROR_INVALID_HANDLE,
    MTP_ERROR_INVALID_STORAGE_ID,
    MTP_ERROR_INVALID_PARENT,
    MTP_ERROR_SESSION_NOT_OPEN,

    // Storage errors
    MTP_ERROR_STORAGE_FULL,
    MTP_ERROR_STORAGE_READ_ONLY,
    MTP_ERROR_FILE_NOT_FOUND,
    MTP_ERROR_FILE_EXISTS,
    MTP_ERROR_ACCESS_DENIED,
    MTP_ERROR_IO_ERROR,

    // Transfer errors
    MTP_ERROR_TRANSFER_CANCELLED,
    MTP_ERROR_TRANSFER_CORRUPT,

} MTPError;

// Error to MTP response code mapping
uint16_t mtp_error_to_response_code(MTPError error);

// Error description
const char* mtp_error_to_string(MTPError error);
```

---

### Error Context

```c
// core/error.h
typedef struct {
    MTPError code;
    char message[256];
    const char* file;
    int line;
    const char* function;
} MTPErrorContext;

// Set error with context
#define MTP_SET_ERROR(ctx, error_code, fmt, ...) \
    mtp_error_context_set(&(ctx)->last_error, error_code, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

// Check and return on error
#define MTP_RETURN_IF_ERROR(ctx, response) \
    if((ctx)->last_error.code != MTP_ERROR_NONE) { \
        mtp_response_set_code(response, mtp_error_to_response_code((ctx)->last_error.code)); \
        return; \
    }
```

**Usage**:

```c
MTP_OPERATION_HANDLER(get_object) {
    uint32_t handle = request->params[0];

    const char* path = mtp_object_index_get_path(ctx->object_index, handle);
    if(!path) {
        MTP_SET_ERROR(ctx, MTP_ERROR_INVALID_HANDLE, "Handle %u not found", handle);
        MTP_RETURN_IF_ERROR(ctx, response);
    }

    MTPFile* file = ctx->storage->vtable->file_open(ctx->storage, path, MTP_STORAGE_MODE_READ);
    if(!file) {
        MTP_SET_ERROR(ctx, MTP_ERROR_FILE_NOT_FOUND, "Failed to open %s", path);
        MTP_RETURN_IF_ERROR(ctx, response);
    }

    // ... success path
}
```

---

## Memory Management Strategy

### Ownership Rules

**Clear Ownership Model**:

1. **Context owns**:
   - Storage interface
   - Object index
   - Transfer state manager
   - Device info strings

2. **Operation handler owns**:
   - Response data (transferred to response structure)
   - Temporary buffers (freed before return)

3. **Response owns**:
   - Response data buffer
   - Stream context (cleanup callback provided)

4. **Transfer state owns**:
   - Open files
   - Accumulation buffers
   - State-specific resources

---

### RAII-Style Patterns

```c
// core/raii_helpers.h

// Auto-cleanup file handle
#define MTP_AUTO_FILE __attribute__((cleanup(mtp_file_cleanup)))
static inline void mtp_file_cleanup(MTPFile** file_ptr) {
    if(*file_ptr) {
        (*file_ptr)->vtable->file_close(*file_ptr);
    }
}

// Auto-cleanup buffer
#define MTP_AUTO_BUFFER __attribute__((cleanup(mtp_buffer_cleanup)))
static inline void mtp_buffer_cleanup(void** buffer_ptr) {
    free(*buffer_ptr);
}

// Usage:
MTP_OPERATION_HANDLER(get_object) {
    MTP_AUTO_FILE MTPFile* file = ctx->storage->vtable->file_open(...);
    MTP_AUTO_BUFFER uint8_t* buffer = malloc(MTP_BUFFER_SIZE);

    // Use file and buffer

    // Automatic cleanup on return (even on early return/error)
}
```

---

### Memory Allocation Strategy

**Size-Based Strategy**:

```c
// Small allocations (<= 256 bytes): Memory pool
MTPMemoryPool* small_pool = mtp_memory_pool_create(256, 32);
void* ptr = mtp_memory_pool_alloc(small_pool);

// Medium allocations (256 - 4KB): malloc/free
uint8_t* buffer = malloc(MTP_BUFFER_SIZE);

// Large allocations (> 4KB): malloc with alignment
void* large = aligned_alloc(4096, size);

// Streaming data: No allocation, use callbacks
```

---

## Testing Strategy

### Unit Testing Framework

```c
// tests/test_framework.h
#define TEST(name) void test_##name(void)
#define RUN_TEST(name) run_test(#name, test_##name)

#define ASSERT(condition) \
    if(!(condition)) { \
        test_fail(__FILE__, __LINE__, #condition); \
    }

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))
#define ASSERT_NULL(ptr) ASSERT((ptr) == NULL)
#define ASSERT_NOT_NULL(ptr) ASSERT((ptr) != NULL)
#define ASSERT_STR_EQ(a, b) ASSERT(strcmp((a), (b)) == 0)
```

---

### Mock Storage Implementation

```c
// tests/mocks/mock_storage.h
typedef struct {
    // Track operations
    int open_count;
    int read_count;
    int write_count;
    int close_count;

    // Configurable behavior
    bool should_fail_open;
    bool should_fail_write;
    size_t disk_space_available;

    // In-memory file system
    // (simple hash table of path → file content)
} MockStorage;

MTPStorage* mock_storage_create(void);
void mock_storage_destroy(MTPStorage* storage);
void mock_storage_reset(MockStorage* mock);
void mock_storage_set_fail_mode(MockStorage* mock, bool fail_open, bool fail_write);
```

---

### Test Categories

**1. Unit Tests** (no external dependencies):

```c
// tests/unit/test_object_index.c
TEST(object_index_add_returns_handle) {
    MTPObjectIndex* index = mtp_object_index_create();

    uint32_t handle = mtp_object_index_add(index, "/ext/test.txt");
    ASSERT_NE(handle, 0);

    mtp_object_index_destroy(index);
}

TEST(object_index_get_path_returns_correct_path) {
    MTPObjectIndex* index = mtp_object_index_create();

    uint32_t handle = mtp_object_index_add(index, "/ext/test.txt");
    const char* path = mtp_object_index_get_path(index, handle);

    ASSERT_STR_EQ(path, "/ext/test.txt");

    mtp_object_index_destroy(index);
}

TEST(object_index_deduplicates_paths) {
    MTPObjectIndex* index = mtp_object_index_create();

    uint32_t handle1 = mtp_object_index_add(index, "/ext/test.txt");
    uint32_t handle2 = mtp_object_index_add(index, "/ext/test.txt");

    ASSERT_EQ(handle1, handle2);
    ASSERT_EQ(mtp_object_index_count(index), 1);

    mtp_object_index_destroy(index);
}
```

**2. Integration Tests** (with mocks):

```c
// tests/integration/test_delete_object.c
TEST(delete_object_removes_file_and_handle) {
    // Setup
    MTPStorage* storage = mock_storage_create();
    MTPContext* ctx = mtp_context_create(storage);

    // Add file to mock storage and index
    mock_storage_add_file(storage, "/ext/test.txt", "content");
    uint32_t handle = mtp_object_index_add(ctx->object_index, "/ext/test.txt");

    // Create request
    MTPRequest request = {
        .op_code = MTP_OP_DELETE_OBJECT,
        .params[0] = handle
    };
    MTPResponse* response = mtp_response_create();

    // Execute
    mtp_op_delete_object(ctx, &request, response);

    // Verify
    ASSERT_EQ(response->response_code, MTP_RESP_OK);
    ASSERT(!mock_storage_file_exists(storage, "/ext/test.txt"));
    ASSERT_NULL(mtp_object_index_get_path(ctx->object_index, handle));

    // Cleanup
    mtp_response_destroy(response);
    mtp_context_destroy(ctx);
    mock_storage_destroy(storage);
}
```

**3. End-to-End Tests** (with real Flipper storage):

```c
// tests/e2e/test_file_transfer.c
TEST(file_transfer_round_trip) {
    // Create real context with Flipper storage
    Storage* flipper_storage = furi_record_open(RECORD_STORAGE);
    MTPStorage* storage = mtp_storage_create_flipper(flipper_storage);
    MTPContext* ctx = mtp_context_create(storage);

    // Test complete file transfer workflow:
    // 1. SendObjectInfo
    // 2. SendObject (stream data)
    // 3. GetObject (verify data)
    // 4. DeleteObject (cleanup)

    // ... test implementation
}
```

---

## Implementation Roadmap

### Phase 0: Foundation (Week 1)

**Goal**: Core infrastructure and abstractions

- [ ] Create directory structure
- [ ] Define all interfaces (storage, operation, transfer state)
- [ ] Implement object index (hash table)
- [ ] Implement MTP string utilities
- [ ] Set up test framework
- [ ] Write unit tests for utilities

**Deliverable**: Core abstractions with tests

---

### Phase 1: Platform Adapters (Week 2)

**Goal**: Platform integration layer

- [ ] Implement Flipper storage adapter
- [ ] Implement Flipper USB adapter
- [ ] Create mock implementations for testing
- [ ] Write integration tests with mocks

**Deliverable**: Working platform adapters

---

### Phase 2: Protocol Layer (Week 3)

**Goal**: MTP protocol handling

- [ ] Implement packet parser
- [ ] Implement protocol state machine
- [ ] Implement dispatcher
- [ ] Implement response transmitter
- [ ] Write protocol layer tests

**Deliverable**: Complete protocol layer

---

### Phase 3: Basic Operations (Week 4)

**Goal**: Read-only MTP operations

- [ ] Implement operation registry
- [ ] Implement device operations (GetDeviceInfo, Open/CloseSession)
- [ ] Implement storage operations (GetStorageIDs, GetStorageInfo)
- [ ] Implement object read operations (GetObjectHandles, GetObjectInfo, GetObject)
- [ ] Write operation tests

**Deliverable**: Read-only MTP functionality

---

### Phase 4: Write Operations (Week 5)

**Goal**: File manipulation operations

- [ ] Implement transfer state manager
- [ ] Implement SendObjectInfo transfer state
- [ ] Implement SendObject transfer state
- [ ] Implement DeleteObject operation
- [ ] Implement MoveObject operation
- [ ] Write transfer state tests

**Deliverable**: Full MTP read/write functionality

---

### Phase 5: Property Operations (Week 6)

**Goal**: MTP property support

- [ ] Implement GetObjectPropsSupported
- [ ] Implement Get/SetObjectPropValue
- [ ] Implement GetDevicePropValue/Desc
- [ ] Write property operation tests

**Deliverable**: Complete MTP property support

---

### Phase 6: Integration & Polish (Week 7)

**Goal**: System integration and optimization

- [ ] Integrate with Flipper UI
- [ ] Implement memory pool
- [ ] Performance profiling and optimization
- [ ] Memory leak detection and fixes
- [ ] End-to-end testing on device

**Deliverable**: Production-ready implementation

---

### Phase 7: Advanced Features (Week 8+)

**Goal**: Optional enhancements

- [ ] Enhanced Unicode filename support
- [ ] Event notifications
- [ ] Thumbnail support
- [ ] Additional device properties
- [ ] Performance monitoring/metrics

**Deliverable**: Enhanced MTP implementation

---

## File Organization

```
src/mtp/
├── core/                          # Core abstractions
│   ├── context.h                  # MTP context (DI container)
│   ├── context.c
│   ├── error.h                    # Error handling
│   ├── error.c
│   ├── object_index.h             # Handle ↔ path mapping
│   ├── object_index.c
│   ├── mtp_string.h               # UTF-16LE encoding
│   ├── mtp_string.c
│   ├── memory_pool.h              # Memory pool allocator
│   ├── memory_pool.c
│   ├── operation.h                # Operation handler interface
│   ├── operation_registry.h       # Operation registry
│   ├── operation_registry.c
│   ├── operation_helpers.h        # Helper macros
│   ├── storage_interface.h        # Storage abstraction
│   ├── transfer_state.h           # Transfer state interface
│   ├── transfer_state_manager.h
│   └── transfer_state_manager.c
│
├── protocol/                      # MTP protocol layer
│   ├── packet.h                   # Packet parsing
│   ├── packet.c
│   ├── state_machine.h            # Protocol state machine
│   ├── state_machine.c
│   ├── dispatcher.h               # Operation dispatcher
│   ├── dispatcher.c
│   ├── transmitter.h              # Response transmitter
│   └── transmitter.c
│
├── operations/                    # MTP operation handlers
│   ├── registry_init.h            # Registry initialization
│   ├── registry_init.c
│   │
│   ├── device/                    # Device operations
│   │   ├── get_device_info.c
│   │   ├── open_session.c
│   │   └── close_session.c
│   │
│   ├── storage/                   # Storage operations
│   │   ├── get_storage_ids.c
│   │   └── get_storage_info.c
│   │
│   ├── objects/                   # Object operations
│   │   ├── get_object_handles.c
│   │   ├── get_object_info.c
│   │   ├── get_object.c
│   │   ├── send_object_info.c
│   │   ├── send_object.c
│   │   ├── delete_object.c
│   │   └── move_object.c
│   │
│   ├── properties/                # Property operations
│   │   ├── get_object_props_supported.c
│   │   ├── get_object_prop_value.c
│   │   ├── set_object_prop_value.c
│   │   ├── get_device_prop_value.c
│   │   └── get_device_prop_desc.c
│   │
│   └── transfer_states/           # Transfer state implementations
│       ├── send_object_info_state.c
│       ├── send_object_state.c
│       └── set_object_prop_state.c
│
├── platform/                      # Platform-specific adapters
│   ├── usb_platform.h             # USB platform interface
│   ├── flipper_usb.h              # Flipper USB implementation
│   ├── flipper_usb.c
│   ├── storage_platform.h         # Storage platform interface
│   ├── flipper_storage.h          # Flipper storage implementation
│   └── flipper_storage.c
│
├── app/                           # Application entry point
│   ├── mtp_app.h                  # Application interface
│   ├── mtp_app.c                  # Application main
│   └── ui.c                       # Flipper UI integration
│
└── mtp.h                          # Public API header

tests/
├── framework/                     # Test framework
│   ├── test.h
│   └── test.c
│
├── mocks/                         # Mock implementations
│   ├── mock_storage.h
│   ├── mock_storage.c
│   ├── mock_usb.h
│   └── mock_usb.c
│
├── unit/                          # Unit tests
│   ├── test_object_index.c
│   ├── test_mtp_string.c
│   ├── test_memory_pool.c
│   ├── test_packet_parser.c
│   └── ...
│
├── integration/                   # Integration tests
│   ├── test_operations.c
│   ├── test_transfer_states.c
│   └── ...
│
└── e2e/                          # End-to-end tests
    ├── test_file_transfer.c
    └── ...
```

---

## Performance Considerations

### Memory Footprint

**Estimated Memory Usage**:

```
Core Structures:
- MTPContext: ~200 bytes
- MTPObjectIndex (64 buckets): ~2KB + (handle count × ~280 bytes)
- MTPOperationRegistry: ~800 bytes (20 ops × 40 bytes/entry)
- Protocol State Machine: ~500 bytes
- Transfer State (active): ~200 bytes

Buffers:
- USB packet buffer: 512 bytes
- Response construction buffer: 1KB
- Memory pool (32 blocks × 256 bytes): 8KB

Total Base: ~13KB
Per-File Overhead: ~280 bytes (handle entry)

Example: 100 files = 13KB + (100 × 280) = 41KB
```

**Current Implementation**: ~2KB base + (file count × ~280 bytes)

**Comparison**: Slightly higher base, same per-file overhead

---

### CPU Performance

**Hash Table vs Linked List**:

| Operation | Linked List | Hash Table | Improvement |
|-----------|-------------|------------|-------------|
| Insert | O(n) | O(1) | 100× faster (100 files) |
| Lookup | O(n) | O(1) | 100× faster (100 files) |
| Update | O(n) | O(1) | 100× faster (100 files) |
| Remove | O(n) | O(1) | 100× faster (100 files) |

**Directory Scanning**:
- Current: 2 passes
- Proposed: 1 pass (if using dynamic array)
- Improvement: 2× faster

---

### Throughput

**File Transfer Performance**:

```
Current Implementation:
- Streaming write: Direct to storage
- Sync every 1MB
- Throughput: Limited by SD card (~5-20 MB/s)

Proposed Implementation:
- Same streaming approach
- Same sync strategy
- Throughput: Same (limited by hardware)
```

**No performance regression expected**

---

## Migration from Current Implementation

### Compatibility Layer

```c
// compat/legacy_adapter.h

// Wrapper for existing code to use new implementation
void handle_mtp_command_legacy(AppMTP* legacy_mtp, struct MTPContainer* container) {
    // Convert legacy structures to new API
    MTPContext* ctx = get_context_from_legacy(legacy_mtp);

    MTPRequest request = {
        .op_code = container->header.op,
        .transaction_id = container->header.transaction_id,
    };
    memcpy(request.params, container->params, sizeof(request.params));

    MTPResponse* response = mtp_response_create();

    // Dispatch using new system
    MTPDispatcher* dispatcher = get_global_dispatcher();
    mtp_dispatcher_execute(dispatcher, &request, response);

    // Convert response back to legacy format
    send_mtp_response_from_new(legacy_mtp, response);

    mtp_response_destroy(response);
}
```

**Benefits**:
- ✅ Gradual migration possible
- ✅ Can run both implementations side-by-side
- ✅ A/B testing in production

---

## Conclusion

This from-scratch design achieves all SOLID principles while maintaining the performance characteristics of the current implementation:

### ✅ **SOLID Compliance**

**Single Responsibility**:
- Each module has one clear purpose
- Protocol ≠ Operations ≠ Storage ≠ Platform

**Open/Closed**:
- New operations via registration
- New storage backends via interface implementation
- No modification of existing code

**Liskov Substitution**:
- All operation handlers interchangeable
- All storage implementations interchangeable

**Interface Segregation**:
- Small, focused interfaces
- Clients depend only on what they use

**Dependency Inversion**:
- High-level depends on abstractions
- Concrete implementations injected

---

### ✅ **Quality Improvements**

**Maintainability**:
- Clear module boundaries
- No God objects
- Functions < 100 lines

**Testability**:
- All components mockable
- Unit tests for all modules
- Integration and E2E tests

**Performance**:
- Hash table: O(1) handle operations (vs O(n))
- Single-pass directory scan (vs 2-pass)
- Same streaming I/O performance

**Memory**:
- Slightly higher base (~13KB vs ~2KB)
- Same per-file overhead (~280 bytes)
- Acceptable for Flipper Zero

---

### ✅ **Extensibility**

**Easy to Add**:
- New MTP operations
- New storage backends
- New platform adapters
- New transfer types

**Future-Proof**:
- Unicode support ready
- Event notifications ready
- Thumbnail support ready
- Multi-session ready

---

### 📋 **Next Steps**

1. **Review this plan** with team/stakeholders
2. **Set up development environment** and build system
3. **Implement Phase 0** (foundation)
4. **Establish testing pipeline**
5. **Iterate through phases** with continuous testing

---

**Document Version**: 1.0
**Last Updated**: 2025-11-24
**Author**: Claude Code Architecture
**Status**: Complete Architecture Plan
**Estimated Effort**: 8 weeks (1 developer)
