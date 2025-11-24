# MTP Control Flow Analysis & SOLID Refactoring Plan

## Executive Summary

This document analyzes the current MTP (Media Transfer Protocol) implementation control flow and provides recommendations for refactoring towards SOLID principles. The current implementation exhibits monolithic functions with mixed responsibilities that violate several SOLID principles.

---

## Current Architecture Overview

### Main Components

```
┌─────────────────────────────────────────────────────────────┐
│                         USB Layer                           │
│                    (usb.c, usb_desc.c)                      │
└────────────────────────────┬────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────┐
│                    MTP Protocol Handler                      │
│                         (mtp.c)                              │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  mtp_handle_bulk() - Entry Point                     │  │
│  │  ├─ handle_mtp_command()                             │  │
│  │  ├─ handle_mtp_data_packet()                         │  │
│  │  └─ handle_mtp_response()                            │  │
│  └──────────────────────────────────────────────────────┘  │
└────────────────────────────┬────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────┐
│                   Operation Handlers                         │
│  ┌────────────────┬──────────────────┬──────────────────┐  │
│  │   MTP Ops      │  Storage Ops     │  Device Props    │  │
│  │  (mtp_ops.c)   │ (storage_ops.c)  │ (device_props.c) │  │
│  └────────────────┴──────────────────┴──────────────────┘  │
└────────────────────────────┬────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────┐
│                    Storage Abstraction                       │
│                   (Flipper Storage API)                      │
└─────────────────────────────────────────────────────────────┘
```

---

## Control Flow Analysis

### 1. **Entry Point: `mtp_handle_bulk()`**

**Location**: [mtp.c:16-45](mtp.c#L16-L45)

**Responsibility**: Packet routing and initial parsing

**Flow**:
```
mtp_handle_bulk(buffer, length)
│
├─ Check if continuation packet (persistence.left_bytes > 0)
│  └─ Yes → handle_mtp_data_packet(buffer, length, cont=1)
│
├─ Validate packet length (>= 12 bytes)
│
├─ Parse MTPHeader
│
└─ Route by packet type:
   ├─ MTP_TYPE_COMMAND   → handle_mtp_command()
   ├─ MTP_TYPE_DATA      → handle_mtp_data_packet()
   └─ MTP_TYPE_RESPONSE  → handle_mtp_response()
```

**SOLID Violations**:
- **Single Responsibility**: Mixes packet validation, parsing, and routing
- **Open/Closed**: Hard-coded type checks require modification to add new types

---

### 2. **Command Handler: `handle_mtp_command()`**

**Location**: [mtp.c:328-511](mtp.c#L328-L511)

**Responsibility**: MTP operation dispatch

**Flow**:
```
handle_mtp_command(container)
│
└─ Switch on operation code (mtp_op):
   │
   ├─ MTP_OP_GET_DEVICE_INFO     → send_device_info()
   ├─ MTP_OP_OPEN/CLOSE_SESSION  → send_mtp_response(OK)
   ├─ MTP_OP_GET_STORAGE_IDS     → send_storage_ids()
   ├─ MTP_OP_GET_STORAGE_INFO    → GetStorageInfo() + send response
   ├─ MTP_OP_GET_OBJECT_HANDLES  → GetObjectHandles() + send response
   ├─ MTP_OP_GET_OBJECT_INFO     → GetObjectInfo() + send response
   ├─ MTP_OP_GET_OBJECT_PROPS_*  → Build props + send response
   ├─ MTP_OP_DELETE_OBJECT       → DeleteObject() + send response
   ├─ MTP_OP_GET_DEVICE_PROP_*   → GetDeviceProp*()
   ├─ MTP_OP_SEND_OBJECT_INFO    → setup_persistence()
   ├─ MTP_OP_SEND_OBJECT         → setup_persistence()
   ├─ MTP_OP_GET_OBJECT          → GetObject()
   ├─ MTP_OP_MOVE_OBJECT         → MoveObject()
   ├─ MTP_OP_SET_OBJECT_PROP_*   → setup_persistence()
   ├─ MTP_OP_POWER_DOWN          → send_mtp_response() + power_off()
   └─ default                    → send_mtp_response(UNKNOWN)
```

**SOLID Violations**:
- **Single Responsibility**: 183-line function handling 15+ different operations
- **Open/Closed**: Requires modification for every new MTP operation
- **Dependency Inversion**: Directly calls concrete implementations
- **Interface Segregation**: Clients forced to depend on entire switch statement

---

### 3. **Data Packet Handler: `handle_mtp_data_packet()`**

**Location**: [mtp.c:52-205](mtp.c#L52-L205)

**Responsibility**: Handles incoming data for bulk operations

**Flow**:
```
handle_mtp_data_packet(buffer, length, cont)
│
├─ If NOT continuation packet (cont == 0):
│  ├─ Parse MTPHeader
│  ├─ Validate transaction_id
│  ├─ Extract operation code
│  └─ Initialize operation-specific state:
│     ├─ MTP_OP_SEND_OBJECT_INFO → Allocate global_buffer
│     ├─ MTP_OP_SET_OBJECT_PROP  → Allocate global_buffer
│     └─ MTP_OP_SEND_OBJECT      → Set buffer_offset, left_bytes
│
├─ Handle MTP_OP_SEND_OBJECT (File Write):
│  ├─ Get handle from persistence.prev_handle
│  ├─ Get path from handle
│  ├─ Open file if not already open
│  ├─ Write data chunk to file
│  ├─ Update buffer_offset and left_bytes
│  ├─ Sync file periodically (MTP_FILE_SYNC_INTERVAL)
│  └─ If complete:
│     ├─ Final sync
│     ├─ Send MTP_RESP_OK
│     └─ Close and cleanup file
│
├─ Handle MTP_OP_SET_OBJECT_PROP_VALUE (Property Update):
│  ├─ Extract handle and prop_code from params
│  ├─ Get path from handle
│  └─ If prop_code == MTP_PROP_OBJECT_FILE_NAME:
│     ├─ Read MTP string from buffer
│     ├─ Merge path with base path
│     ├─ Update object handle path (commented: actual rename)
│     └─ Send MTP_RESP_OK
│
└─ If global_buffer allocated:
   ├─ Copy data to global_buffer
   ├─ Update buffer_offset and left_bytes
   └─ If complete (left_bytes == 0):
      └─ handle_mtp_data_complete()
```

**SOLID Violations**:
- **Single Responsibility**: Handles parsing, state management, file I/O, and property updates
- **Open/Closed**: Hard-coded operation handling requires modification for new operations
- **Dependency Inversion**: Direct file system access without abstraction

---

### 4. **Data Complete Handler: `handle_mtp_data_complete()`**

**Location**: [mtp.c:207-318](mtp.c#L207-L318)

**Responsibility**: Process completed multi-packet data transfers

**Flow**:
```
handle_mtp_data_complete()
│
└─ Switch on persistence.op:
   │
   └─ MTP_OP_SEND_OBJECT_INFO (Create Object):
      ├─ Parse ObjectInfoHeader from global_buffer
      ├─ Extract filename from MTP string
      ├─ Handle Unicode (fallback to "New Folder"/"New File")
      ├─ Generate random name if blank
      ├─ Extract storage_id and parent from params
      ├─ Get base path from storage_id or parent handle
      ├─ Merge full path
      ├─ If directory (format == MTP_FORMAT_ASSOCIATION):
      │  └─ Create directory with storage_simply_mkdir()
      └─ If file:
         ├─ Create empty file
         ├─ Issue object handle
         └─ Store handle in persistence.prev_handle
```

**SOLID Violations**:
- **Single Responsibility**: Mixes parsing, validation, path manipulation, and file creation
- **Open/Closed**: Requires modification for new bulk operations
- **Dependency Inversion**: Direct storage API calls

---

### 5. **Response Handlers**

**Location**: [mtp.c:513-646](mtp.c#L513-L646)

**Functions**:
- `send_storage_ids()` - Simple storage ID response
- `send_device_info()` - Device info response
- `send_mtp_response()` - Generic response with params
- `send_mtp_response_buffer()` - Buffer-based response
- `send_mtp_response_stream()` - Streaming response with callback

**Flow**:
```
send_mtp_response_stream(resp_type, resp_code, transaction_id, ctx, callback, length)
│
└─ Loop until all data sent:
   ├─ Allocate packet buffer (MTP_MAX_PACKET_SIZE)
   ├─ If first chunk:
   │  └─ Write MTPHeader
   ├─ Call callback to fill buffer
   ├─ Send packet via usbd_ep_write()
   └─ Update sent_length counter
```

**SOLID Compliance**: These functions are relatively well-designed with clear responsibilities.

---

## State Management Analysis

### Global State: `MTPDataPersistence persistence`

**Location**: [mtp.c:14](mtp.c#L14)

**Structure**:
```c
typedef struct MTPDataPersistence {
    uint32_t left_bytes;         // Remaining bytes in multi-packet transfer
    uint8_t* global_buffer;      // Temporary buffer for accumulating data
    uint32_t buffer_offset;      // Current offset in global_buffer

    uint16_t op;                 // Current operation code
    uint32_t transaction_id;     // Current transaction ID
    uint32_t params[5];          // Operation parameters

    uint32_t prev_handle;        // Previous object handle (for SEND_OBJECT)
    File* current_file;          // Open file handle
} MTPDataPersistence;
```

**Issues**:
- **Global mutable state** shared across all functions
- **No encapsulation** - direct access from multiple functions
- **No lifecycle management** - unclear ownership
- **Thread safety concerns** - no synchronization

---

## Data Flow Patterns

### 1. **Query Operations (Read-Only)**

```
USB Bulk IN
    ↓
mtp_handle_bulk()
    ↓
handle_mtp_command()
    ↓
Operation Function (e.g., GetObjectInfo)
    ↓
send_mtp_response_buffer()
    ↓
send_mtp_response_stream()
    ↓
USB EP Write
```

**Characteristics**:
- Synchronous execution
- Single-packet request/response
- No state mutation

---

### 2. **Creation Operations (Write - Multi-Packet)**

```
USB Bulk OUT (SEND_OBJECT_INFO command)
    ↓
mtp_handle_bulk()
    ↓
handle_mtp_command()
    ↓
setup_persistence()
    ↓
[Wait for data packets...]
    ↓
USB Bulk OUT (DATA packets)
    ↓
mtp_handle_bulk()
    ↓
handle_mtp_data_packet() [accumulate in global_buffer]
    ↓
handle_mtp_data_complete()
    ↓
Create file/directory
    ↓
send_mtp_response()
```

**Characteristics**:
- Asynchronous (multi-packet)
- State maintained in global `persistence`
- Complex error handling needed

---

### 3. **File Transfer Operations (Streaming Write)**

```
USB Bulk OUT (SEND_OBJECT_INFO command)
    ↓
handle_mtp_command() → setup_persistence()
    ↓
[Object created, handle stored in persistence.prev_handle]
    ↓
USB Bulk OUT (SEND_OBJECT command)
    ↓
handle_mtp_command() → setup_persistence()
    ↓
USB Bulk OUT (DATA packets - file content)
    ↓
handle_mtp_data_packet()
    ├─ Open file on first packet
    ├─ Write chunk to file
    ├─ Periodic sync (every MTP_FILE_SYNC_INTERVAL)
    └─ On last packet:
       ├─ Final sync
       ├─ Close file
       └─ Send MTP_RESP_OK
```

**Characteristics**:
- Streaming I/O (no buffering in memory)
- Direct write to storage
- Progressive persistence with periodic syncs
- File handle maintained in `persistence.current_file`

---

## SOLID Violations Summary

### ❌ Single Responsibility Principle (SRP)

**Violations**:

1. **`handle_mtp_command()`**:
   - Packet parsing ✗
   - Operation dispatch ✗
   - Response formatting ✗
   - Business logic execution ✗

2. **`handle_mtp_data_packet()`**:
   - Packet parsing ✗
   - State management ✗
   - File I/O operations ✗
   - Property updates ✗
   - Buffer management ✗

3. **`handle_mtp_data_complete()`**:
   - Data parsing ✗
   - Path manipulation ✗
   - File system operations ✗
   - Handle management ✗

---

### ❌ Open/Closed Principle (OCP)

**Violations**:

1. **Large switch statements** requiring modification for new operations:
   - `handle_mtp_command()` - 15+ cases
   - `handle_mtp_data_packet()` - 3 operation-specific branches
   - `handle_mtp_data_complete()` - 1 case (will grow)

2. **Hard-coded type checking**:
   - Packet type routing in `mtp_handle_bulk()`
   - Operation-specific logic in data handlers

**Impact**: Every new MTP operation requires modifying multiple functions.

---

### ❌ Liskov Substitution Principle (LSP)

**Current State**: Not applicable - no inheritance hierarchy exists.

**Future Concern**: When refactoring to polymorphism, must ensure operation handlers are properly substitutable.

---

### ❌ Interface Segregation Principle (ISP)

**Violations**:

1. **Monolithic operation handling**: All operations handled by single `handle_mtp_command()` function.
2. **No segregation by capability**: Read, write, and control operations mixed together.
3. **Large parameter lists**: Functions like `send_mtp_response_stream()` take 6+ parameters.

---

### ❌ Dependency Inversion Principle (DIP)

**Violations**:

1. **Direct storage API calls**:
   ```c
   storage_file_open(persistence.current_file, path, ...)
   storage_simply_mkdir(mtp->storage, full_path)
   ```

2. **Direct USB API calls**:
   ```c
   usbd_ep_write(mtp->dev, MTP_EP_IN_ADDR, buffer, usb_bytes)
   ```

3. **No abstractions**: High-level logic directly depends on low-level implementations.

---

## Refactoring Recommendations

### Phase 1: Extract Operation Handlers (Command Pattern)

**Goal**: Replace switch statements with polymorphic operation handlers.

**Approach**:

1. **Define Operation Handler Interface**:
```c
// Operation handler function pointer type
typedef void (*MTPOperationHandler)(
    AppMTP* mtp,
    const struct MTPContainer* request,
    MTPResponse* response
);

// Operation registry entry
typedef struct {
    uint16_t op_code;
    MTPOperationHandler handler;
} MTPOperationEntry;
```

2. **Implement Individual Handlers**:
```c
// Example: mtp_ops/get_device_info.c
void mtp_op_get_device_info(
    AppMTP* mtp,
    const struct MTPContainer* request,
    MTPResponse* response
) {
    uint8_t* buffer = malloc(MTP_BUFFER_SIZE);
    int length = BuildDeviceInfo(buffer);

    mtp_response_set_data(response, buffer, length);
    mtp_response_set_code(response, MTP_RESP_OK);

    free(buffer);
}
```

3. **Create Operation Registry**:
```c
static const MTPOperationEntry operation_registry[] = {
    {MTP_OP_GET_DEVICE_INFO, mtp_op_get_device_info},
    {MTP_OP_GET_STORAGE_IDS, mtp_op_get_storage_ids},
    {MTP_OP_GET_STORAGE_INFO, mtp_op_get_storage_info},
    // ... all operations
};
```

4. **Replace Switch with Registry Lookup**:
```c
void handle_mtp_command(AppMTP* mtp, struct MTPContainer* container) {
    MTPResponse response = {0};

    MTPOperationHandler handler = find_operation_handler(container->header.op);
    if (handler) {
        handler(mtp, container, &response);
        send_mtp_response_from_struct(mtp, container->header.transaction_id, &response);
    } else {
        send_mtp_response(mtp, MTP_TYPE_RESPONSE, MTP_RESP_UNKNOWN,
                         container->header.transaction_id, NULL);
    }
}
```

**Benefits**:
- ✅ **OCP**: Add new operations without modifying core dispatcher
- ✅ **SRP**: Each operation handler has single responsibility
- ✅ **ISP**: Operations only depend on what they need

---

### Phase 2: Extract State Management (State Pattern)

**Goal**: Encapsulate multi-packet transfer state management.

**Approach**:

1. **Define Transfer State Interface**:
```c
typedef struct MTPTransferState MTPTransferState;

typedef struct {
    // Lifecycle
    MTPTransferState* (*create)(uint16_t op_code, uint32_t transaction_id);
    void (*destroy)(MTPTransferState* state);

    // Data handling
    bool (*handle_packet)(MTPTransferState* state, const uint8_t* data, uint32_t length);
    bool (*is_complete)(const MTPTransferState* state);

    // Completion
    MTPResponseCode (*finalize)(MTPTransferState* state, AppMTP* mtp);
} MTPTransferStateVTable;
```

2. **Implement State Objects**:
```c
// Example: SendObjectTransferState
typedef struct {
    MTPTransferStateVTable* vtable;
    uint32_t transaction_id;
    uint32_t handle;
    File* file;
    uint32_t bytes_written;
    uint32_t total_bytes;
} SendObjectTransferState;

bool send_object_handle_packet(MTPTransferState* state, const uint8_t* data, uint32_t length) {
    SendObjectTransferState* obj_state = (SendObjectTransferState*)state;

    // Open file on first packet
    if (!obj_state->file) {
        char* path = get_path_from_handle(mtp, obj_state->handle);
        obj_state->file = storage_file_alloc(mtp->storage);
        storage_file_open(obj_state->file, path, FSAM_WRITE, FSOM_OPEN_EXISTING);
    }

    // Write chunk
    uint16_t written = storage_file_write(obj_state->file, data, length);
    obj_state->bytes_written += written;

    // Periodic sync
    if (obj_state->bytes_written % MTP_FILE_SYNC_INTERVAL == 0) {
        storage_file_sync(obj_state->file);
    }

    return written == length;
}
```

3. **Replace Global Persistence**:
```c
// In AppMTP structure
typedef struct AppMTP {
    // ... existing fields
    MTPTransferState* active_transfer;  // Replace global persistence
} AppMTP;
```

**Benefits**:
- ✅ **SRP**: State management separated from packet handling
- ✅ **OCP**: New transfer types without modifying existing code
- ✅ **Encapsulation**: State lifecycle properly managed

---

### Phase 3: Introduce Storage Abstraction (DIP)

**Goal**: Decouple MTP logic from concrete storage implementation.

**Approach**:

1. **Define Storage Interface**:
```c
typedef struct {
    bool (*file_exists)(void* ctx, const char* path);
    bool (*dir_exists)(void* ctx, const char* path);
    bool (*mkdir)(void* ctx, const char* path);
    File* (*file_open)(void* ctx, const char* path, int mode);
    bool (*file_delete)(void* ctx, const char* path);
    bool (*file_rename)(void* ctx, const char* old_path, const char* new_path);
    // ... other operations
} StorageInterface;
```

2. **Create Adapter for Flipper Storage**:
```c
typedef struct {
    StorageInterface interface;
    Storage* flipper_storage;
} FlipperStorageAdapter;

bool flipper_storage_file_exists(void* ctx, const char* path) {
    FlipperStorageAdapter* adapter = (FlipperStorageAdapter*)ctx;
    return storage_file_exists(adapter->flipper_storage, path);
}

StorageInterface* create_flipper_storage_adapter(Storage* storage) {
    FlipperStorageAdapter* adapter = malloc(sizeof(FlipperStorageAdapter));
    adapter->flipper_storage = storage;
    adapter->interface.file_exists = flipper_storage_file_exists;
    // ... wire up other functions
    return &adapter->interface;
}
```

3. **Update AppMTP Structure**:
```c
typedef struct AppMTP {
    // ... existing fields
    Storage* storage;              // Low-level API (for compatibility)
    StorageInterface* storage_if;  // High-level abstraction
} AppMTP;
```

**Benefits**:
- ✅ **DIP**: High-level MTP logic depends on abstraction
- ✅ **Testability**: Can inject mock storage for testing
- ✅ **Flexibility**: Easy to add new storage backends

---

### Phase 4: Separate Response Building (Builder Pattern)

**Goal**: Simplify response construction and transmission.

**Approach**:

1. **Define Response Builder**:
```c
typedef struct MTPResponseBuilder MTPResponseBuilder;

MTPResponseBuilder* mtp_response_builder_create(void);
void mtp_response_builder_set_code(MTPResponseBuilder* builder, uint16_t code);
void mtp_response_builder_add_param(MTPResponseBuilder* builder, uint32_t param);
void mtp_response_builder_set_data(MTPResponseBuilder* builder, uint8_t* data, uint32_t length);
void mtp_response_builder_send(MTPResponseBuilder* builder, AppMTP* mtp, uint32_t transaction_id);
void mtp_response_builder_destroy(MTPResponseBuilder* builder);
```

2. **Simplify Operation Handlers**:
```c
void mtp_op_get_storage_ids(AppMTP* mtp, const struct MTPContainer* request, MTPResponse* response) {
    MTPResponseBuilder* builder = mtp_response_builder_create();

    uint32_t storage_ids[3];
    uint32_t count;
    GetStorageIDs(mtp, storage_ids, &count);

    mtp_response_builder_set_code(builder, MTP_RESP_OK);
    mtp_response_builder_add_param(builder, count);
    for (uint32_t i = 0; i < count; i++) {
        mtp_response_builder_add_param(builder, storage_ids[i]);
    }

    mtp_response_builder_send(builder, mtp, request->header.transaction_id);
    mtp_response_builder_destroy(builder);
}
```

**Benefits**:
- ✅ **SRP**: Response building separated from business logic
- ✅ **Readability**: Clearer operation handler code
- ✅ **Maintainability**: Centralized response formatting

---

### Phase 5: Error Handling Strategy

**Goal**: Consistent error handling across all operations.

**Approach**:

1. **Define Error Types**:
```c
typedef enum {
    MTP_ERROR_NONE = 0,
    MTP_ERROR_INVALID_HANDLE,
    MTP_ERROR_INVALID_STORAGE_ID,
    MTP_ERROR_STORAGE_FULL,
    MTP_ERROR_IO_ERROR,
    MTP_ERROR_ACCESS_DENIED,
    // ... other errors
} MTPErrorType;

typedef struct {
    MTPErrorType type;
    uint16_t response_code;  // MTP response code to send
    char message[256];       // Debug message
} MTPError;
```

2. **Error Propagation Pattern**:
```c
typedef struct {
    bool success;
    MTPError error;
    void* result;  // Operation-specific result
} MTPOperationResult;

MTPOperationResult mtp_op_delete_object_impl(AppMTP* mtp, uint32_t handle) {
    MTPOperationResult result = {.success = true};

    char* path = get_path_from_handle(mtp, handle);
    if (!path) {
        result.success = false;
        result.error.type = MTP_ERROR_INVALID_HANDLE;
        result.error.response_code = MTP_RESP_INVALID_OBJECT_HANDLE;
        snprintf(result.error.message, sizeof(result.error.message),
                 "Invalid handle: %lu", handle);
        return result;
    }

    if (!storage_file_delete(mtp->storage, path)) {
        result.success = false;
        result.error.type = MTP_ERROR_IO_ERROR;
        result.error.response_code = MTP_RESP_GENERAL_ERROR;
        snprintf(result.error.message, sizeof(result.error.message),
                 "Failed to delete: %s", path);
        return result;
    }

    return result;
}
```

3. **Centralized Error Handling**:
```c
void mtp_op_delete_object(AppMTP* mtp, const struct MTPContainer* request, MTPResponse* response) {
    MTPOperationResult result = mtp_op_delete_object_impl(mtp, request->params[0]);

    if (result.success) {
        mtp_response_set_code(response, MTP_RESP_OK);
    } else {
        FURI_LOG_E("MTP", "DeleteObject failed: %s", result.error.message);
        mtp_response_set_code(response, result.error.response_code);
    }
}
```

**Benefits**:
- ✅ **Consistency**: Uniform error handling pattern
- ✅ **Debuggability**: Structured error information
- ✅ **Separation**: Error handling separated from business logic

---

## Refactoring Roadmap

### Step 1: Non-Breaking Preparation
- [ ] Create new directory structure for operation handlers
- [ ] Define operation handler interface and registry
- [ ] Define transfer state interface
- [ ] Define storage interface
- [ ] Define response builder interface

### Step 2: Extract Operations (Incremental)
- [ ] Extract simple operations first (GET_DEVICE_INFO, GET_STORAGE_IDS)
- [ ] Extract medium complexity (GET_OBJECT_INFO, DELETE_OBJECT)
- [ ] Extract complex operations (SEND_OBJECT_INFO, SEND_OBJECT)
- [ ] Update `handle_mtp_command()` to use registry

### Step 3: Extract State Management
- [ ] Implement SendObjectTransferState
- [ ] Implement SendObjectInfoTransferState
- [ ] Implement SetObjectPropTransferState
- [ ] Replace global `persistence` with state objects

### Step 4: Introduce Abstractions
- [ ] Implement storage interface adapter
- [ ] Implement response builder
- [ ] Update operation handlers to use abstractions

### Step 5: Cleanup & Testing
- [ ] Remove dead code
- [ ] Add comprehensive error handling
- [ ] Add unit tests for operation handlers
- [ ] Add integration tests
- [ ] Performance validation

---

## File Organization Proposal

```
src/scenes/mtp/
├── core/
│   ├── mtp_protocol.h          # Core protocol definitions
│   ├── mtp_protocol.c
│   ├── mtp_dispatcher.h        # Operation dispatcher
│   └── mtp_dispatcher.c
├── operations/
│   ├── mtp_operation.h         # Operation interface
│   ├── mtp_operation_registry.h
│   ├── mtp_operation_registry.c
│   ├── info/
│   │   ├── get_device_info.c
│   │   ├── get_storage_ids.c
│   │   └── get_storage_info.c
│   ├── objects/
│   │   ├── get_object_handles.c
│   │   ├── get_object_info.c
│   │   ├── get_object.c
│   │   ├── send_object_info.c
│   │   ├── send_object.c
│   │   ├── delete_object.c
│   │   └── move_object.c
│   └── properties/
│       ├── get_object_props_supported.c
│       ├── get_object_prop_value.c
│       └── set_object_prop_value.c
├── transfer/
│   ├── transfer_state.h        # Transfer state interface
│   ├── send_object_state.c
│   ├── send_object_info_state.c
│   └── set_prop_state.c
├── storage/
│   ├── storage_interface.h     # Storage abstraction
│   ├── flipper_storage_adapter.c
│   └── handle_manager.c        # Object handle management
├── response/
│   ├── response_builder.h
│   └── response_builder.c
└── legacy/
    ├── mtp.c                   # Original (to be removed)
    └── mtp.h
```

---

## Migration Strategy

### Backward Compatibility

During migration, maintain dual implementation:

```c
// In handle_mtp_command()
void handle_mtp_command(AppMTP* mtp, struct MTPContainer* container) {
    // Try new registry-based approach first
    MTPOperationHandler handler = find_operation_handler(container->header.op);
    if (handler) {
        MTPResponse response = {0};
        handler(mtp, container, &response);
        send_mtp_response_from_struct(mtp, container->header.transaction_id, &response);
        return;
    }

    // Fallback to legacy switch statement
    switch (container->header.op) {
        // ... existing implementation
    }
}
```

This allows gradual migration without breaking existing functionality.

---

## Testing Strategy

### Unit Tests (New)

Test individual operation handlers in isolation:

```c
void test_get_device_info(void) {
    // Arrange
    AppMTP* mtp = create_test_mtp();
    struct MTPContainer request = {
        .header = {.op = MTP_OP_GET_DEVICE_INFO, .transaction_id = 1}
    };
    MTPResponse response = {0};

    // Act
    mtp_op_get_device_info(mtp, &request, &response);

    // Assert
    assert(response.code == MTP_RESP_OK);
    assert(response.data != NULL);
    assert(response.data_length > 0);

    cleanup_test_mtp(mtp);
}
```

### Integration Tests (New)

Test complete operation flows:

```c
void test_send_object_flow(void) {
    // Simulate complete file transfer:
    // 1. SEND_OBJECT_INFO command
    // 2. Multiple DATA packets
    // 3. SEND_OBJECT command
    // 4. File DATA packets
    // 5. Verify file created correctly
}
```

### Regression Tests (Critical)

Maintain existing functionality during refactoring:

```c
void test_all_existing_operations(void) {
    // Run comprehensive test suite covering all current MTP operations
    // Compare behavior before and after refactoring
}
```

---

## Performance Considerations

### Memory Impact

**Current**:
- Global `persistence` structure: ~60 bytes + dynamic allocations
- Large switch statements: Minimal memory, but code size impact

**Proposed**:
- Operation registry: ~400 bytes (20 operations × ~20 bytes/entry)
- Transfer state objects: ~100-200 bytes per active transfer (only one at a time)
- Storage interface vtable: ~100 bytes

**Net Impact**: Minimal increase (~500 bytes), acceptable for Flipper Zero

### CPU Impact

**Current**:
- Switch statement: O(n) linear scan (though typically optimized by compiler)
- Direct function calls: Minimal overhead

**Proposed**:
- Registry lookup: O(log n) binary search or O(1) hash table
- Virtual function calls: Single pointer indirection (~5-10 CPU cycles)

**Net Impact**: Negligible performance difference, likely within measurement noise

### Code Size Impact

**Current**: ~2KB for mtp.c

**Proposed**: ~3-4KB total (modular files)
- Core dispatcher: ~500 bytes
- Operation handlers: ~2KB
- Infrastructure: ~1KB

**Net Impact**: Slight increase, but improved maintainability justifies it

---

## Risk Assessment

### High Risk
- **State Management Changes**: Global to encapsulated state
  - *Mitigation*: Thorough testing, phased rollout

### Medium Risk
- **Operation Handler Extraction**: Large switch to registry
  - *Mitigation*: Maintain backward compatibility during transition

### Low Risk
- **Storage Abstraction**: Additional indirection layer
  - *Mitigation*: Simple adapter pattern, no behavior changes

- **Response Builder**: New utility, doesn't replace existing
  - *Mitigation*: Optional use, gradual adoption

---

## Success Criteria

### Functional Requirements
- ✅ All existing MTP operations work identically
- ✅ No performance degradation
- ✅ No memory footprint increase >10%

### Code Quality Metrics
- ✅ No function >100 lines
- ✅ No file >500 lines
- ✅ Cyclomatic complexity <10 per function
- ✅ 90%+ unit test coverage for new code

### SOLID Compliance
- ✅ Each operation handler has single responsibility
- ✅ New operations addable without modifying existing code
- ✅ Dependencies on abstractions, not concretions
- ✅ No God objects or monolithic functions

---

## Conclusion

The current MTP implementation is functional but violates multiple SOLID principles, making it difficult to maintain and extend. The proposed refactoring approach:

1. **Preserves existing functionality** through careful incremental changes
2. **Improves maintainability** by separating concerns
3. **Enables extensibility** through operation registry pattern
4. **Reduces coupling** via storage and response abstractions
5. **Maintains performance** through minimal overhead abstractions

The refactoring should be approached as a **gradual, test-driven migration** rather than a big-bang rewrite, ensuring production stability throughout the transition.

---

## Appendix A: Current Call Graph

```
mtp_handle_bulk()
├── handle_mtp_command()
│   ├── send_device_info()
│   │   ├── BuildDeviceInfo()
│   │   └── send_mtp_response_buffer()
│   ├── send_storage_ids()
│   │   ├── GetStorageIDs()
│   │   └── send_mtp_response_buffer()
│   ├── GetStorageInfo()
│   ├── GetObjectHandles()
│   ├── GetObjectInfo()
│   ├── GetObject()
│   ├── DeleteObject()
│   ├── MoveObject()
│   ├── GetDevicePropValue()
│   ├── GetDevicePropDesc()
│   ├── GetObjectPropValue()
│   └── setup_persistence()
├── handle_mtp_data_packet()
│   ├── get_path_from_handle()
│   ├── storage_file_open()
│   ├── storage_file_write()
│   ├── storage_file_sync()
│   ├── storage_file_close()
│   ├── ReadMTPString()
│   ├── update_object_handle_path()
│   └── handle_mtp_data_complete()
│       ├── CheckMTPStringHasUnicode()
│       ├── ReadMTPString()
│       ├── get_base_path_from_storage_id()
│       ├── get_path_from_handle()
│       ├── merge_path()
│       ├── storage_dir_exists()
│       ├── storage_simply_mkdir()
│       ├── storage_file_exists()
│       ├── storage_file_open()
│       ├── issue_object_handle()
│       └── send_mtp_response()
└── handle_mtp_response()
```

---

## Appendix B: State Machine Diagram

```
[Idle State]
     │
     │ Receive COMMAND packet
     ▼
[Command Processing]
     │
     ├─ Simple Operation (GET_*)
     │  └─► [Response Sent] ──► [Idle State]
     │
     └─ Multi-Packet Operation (SEND_*)
        └─► [Awaiting Data]
             │
             │ Receive DATA packet
             ▼
        [Accumulating Data]
             │
             ├─ More data expected
             │  └─► [Awaiting Data]
             │
             └─ Data complete
                └─► [Processing Complete]
                     └─► [Response Sent] ──► [Idle State]
```

---

## Appendix C: Recommended Reading

### Design Patterns
- **Command Pattern**: Gang of Four - for operation handlers
- **State Pattern**: Gang of Four - for transfer state management
- **Adapter Pattern**: Gang of Four - for storage abstraction
- **Builder Pattern**: Gang of Four - for response construction

### SOLID Principles
- *"Agile Software Development, Principles, Patterns, and Practices"* - Robert C. Martin
- *"Clean Architecture"* - Robert C. Martin

### Refactoring Techniques
- *"Refactoring: Improving the Design of Existing Code"* - Martin Fowler
- *"Working Effectively with Legacy Code"* - Michael Feathers

---

**Document Version**: 1.0
**Last Updated**: 2025-11-24
**Author**: Claude Code Analysis
**Status**: Draft for Review
