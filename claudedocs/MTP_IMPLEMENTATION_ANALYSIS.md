# MTP Implementation Analysis - Current Architecture

## Document Overview

This document provides a detailed analysis of how the current MTP implementation works, examining the actual code patterns, data structures, and implementation strategies used in the Flipper Zero MTP application.

**Companion Document**: [MTP_CONTROL_FLOW_ANALYSIS.md](MTP_CONTROL_FLOW_ANALYSIS.md) - Contains control flow diagrams and SOLID refactoring recommendations.

---

## Table of Contents

1. [Architecture Layers](#architecture-layers)
2. [Data Structures](#data-structures)
3. [Implementation Patterns](#implementation-patterns)
4. [Object Handle Management](#object-handle-management)
5. [MTP String Encoding](#mtp-string-encoding)
6. [File Transfer Implementation](#file-transfer-implementation)
7. [Storage Abstraction](#storage-abstraction)
8. [Response Construction](#response-construction)
9. [Memory Management](#memory-management)
10. [Error Handling](#error-handling)
11. [Performance Optimizations](#performance-optimizations)

---

## Architecture Layers

### Layer 1: USB Physical Layer
**Files**: `usb.c`, `usb.h`, `usb_desc.c`, `usb_desc.h`

```
USB Hardware (Flipper Zero STM32)
         ↓
FuriHal USB Interface
         ↓
Custom MTP USB Descriptors
         ↓
MTP Endpoints (Bulk IN/OUT, Interrupt)
```

**Key Components**:
- USB device initialization and configuration
- Endpoint management (Bulk IN, Bulk OUT, Interrupt)
- USB class-specific control requests
- Descriptor definitions (device, configuration, interface, endpoint)

---

### Layer 2: MTP Protocol Layer
**Files**: `mtp.c`, `mtp.h`

```
USB Bulk Packets
         ↓
mtp_handle_bulk() - Packet Router
         ↓
    ┌────┴────┬────────────────┐
    ▼         ▼                ▼
COMMAND     DATA           RESPONSE
    ↓         ↓                ↓
handle_mtp_ handle_mtp_   handle_mtp_
command()   data_packet() response()
```

**Responsibilities**:
- MTP packet parsing and validation
- Operation dispatch
- Multi-packet state management
- Response formatting and transmission

---

### Layer 3: Operation Implementation Layer
**Files**: `mtp_ops.c`, `storage_ops.c`, `device_props.c`

```
MTP Operations (divided by domain)
│
├─ Storage Operations (mtp_ops.c)
│  ├─ GetStorageIDs()
│  ├─ GetStorageInfo()
│  ├─ GetObjectHandles()
│  ├─ GetObjectInfo()
│  ├─ GetObject()
│  ├─ DeleteObject()
│  └─ MoveObject()
│
├─ Handle Management (storage_ops.c)
│  ├─ issue_object_handle()
│  ├─ get_path_from_handle()
│  ├─ update_object_handle_path()
│  └─ list_and_issue_handles()
│
└─ Device Properties (device_props.c)
   ├─ BuildDeviceInfo()
   ├─ GetDevicePropValue()
   └─ GetDevicePropDesc()
```

**Domain Separation**:
- **Storage ops**: File/directory operations
- **Handle ops**: Object handle ↔ path mapping
- **Device props**: Device metadata and capabilities

---

### Layer 4: Utility Layer
**Files**: `utils.c`, `utils.h`

```
Utility Functions
├─ MTP String Encoding/Decoding
│  ├─ WriteMTPString() - ASCII to UTF-16LE
│  ├─ ReadMTPString() - UTF-16LE to ASCII
│  └─ CheckMTPStringHasUnicode()
├─ Path Manipulation
│  └─ merge_path() - Combine base and filename
└─ Debug Utilities
   ├─ print_bytes() - Hex dump
   └─ byte_to_hex() - Hex conversion
```

---

## Data Structures

### Core Application State: `AppMTP`

**Location**: [main.h:28-47](main.h#L28-L47)

```c
typedef struct AppMTP {
    Submenu* menu;              // UI menu
    View* view;                 // UI view

    bool usb_connected;         // USB connection status

    // USB infrastructure
    FuriHalUsbInterface* old_usb;  // Previous USB mode (for restore)
    FuriThread* worker_thread;     // USB worker thread
    usbd_device* dev;              // USB device handle

    MTPState state;             // Device state (Offline/Ready/Busy/etc)
    bool is_working;            // Processing flag
    Storage* storage;           // Flipper storage API handle
    MTPSession session;         // MTP session info

    bool write_pending;         // Write operation in progress

    FileHandle* handles;        // Object handle linked list (HEAD)
} AppMTP;
```

**Key Observations**:
- Single global state structure
- Direct dependency on Flipper APIs (Storage, USB)
- Object handles stored as linked list
- No separation between UI and protocol state

---

### MTP Session: `MTPSession`

**Location**: [main.h:17-20](main.h#L17-L20)

```c
typedef struct {
    uint32_t session_id;
    bool session_open;
} MTPSession;
```

**Current Usage**:
- Defined but **minimally used** in practice
- Session operations (OpenSession, CloseSession) are **stubs** that return OK
- No actual session validation enforced

**MTP Spec Requirement**: Sessions should gate most operations (except GetDeviceInfo)

**Gap**: Current implementation doesn't enforce session state

---

### Multi-Packet Transfer State: `MTPDataPersistence`

**Location**: [mtp.h:147-158](mtp.h#L147-L158), [mtp.c:14](mtp.c#L14)

```c
typedef struct MTPDataPersistence {
    uint32_t left_bytes;        // Remaining bytes to receive
    uint8_t* global_buffer;     // Accumulation buffer (for SEND_OBJECT_INFO)
    uint32_t buffer_offset;     // Current position in global_buffer

    uint16_t op;                // Current operation code
    uint32_t transaction_id;    // Current transaction ID
    uint32_t params[5];         // Operation parameters (from command)

    uint32_t prev_handle;       // Handle from previous operation (SEND_OBJECT_INFO)
    File* current_file;         // Open file for streaming write (SEND_OBJECT)
} MTPDataPersistence;

// Global instance
MTPDataPersistence persistence;
```

**Purpose**: State machine for multi-packet operations

**Lifecycle**:
1. **Setup**: `setup_persistence()` called from command handler
2. **Accumulation**: Data packets update state
3. **Completion**: `handle_mtp_data_complete()` finalizes operation
4. **Cleanup**: Memory freed, state reset

**Global Scope Issue**:
- No concurrency support (only one transfer at a time)
- No protection against state corruption
- Lifetime not clearly managed

---

### Object Handle Mapping: `FileHandle`

**Location**: [main.h:22-26](main.h#L22-L26)

```c
typedef struct FileHandle {
    uint32_t handle;        // Numeric object handle (1, 2, 3, ...)
    char* path;             // Full file system path
    struct FileHandle* next; // Singly-linked list pointer
} FileHandle;
```

**Implementation**: Singly-linked list

**Handle Allocation Strategy**:
```c
uint32_t issue_object_handle(AppMTP* mtp, char* path) {
    // 1. Check if path already has handle
    // 2. If exists, return existing handle
    // 3. If not, allocate new handle = (list length + 1)
    // 4. Append to linked list
    // 5. Return new handle
}
```

**Performance**:
- Lookup: **O(n)** - Linear scan through linked list
- Insert: **O(n)** - Must scan to end of list
- Update: **O(n)** - Linear scan to find handle

**Memory**:
- Each handle stores **duplicated path string** (malloc'd copy)
- No garbage collection - handles persist for application lifetime
- Memory grows with number of objects accessed

**Potential Improvements**:
- Hash table: O(1) lookup
- Binary search tree: O(log n) operations
- Handle reuse after delete operations

---

### MTP Protocol Structures

#### MTPHeader
**Location**: [mtp.h:140-145](mtp.h#L140-L145)

```c
struct MTPHeader {
    uint32_t len;           // Total packet length (including header)
    uint16_t type;          // MTP_TYPE_COMMAND/DATA/RESPONSE
    uint16_t op;            // Operation code or response code
    uint32_t transaction_id; // Transaction identifier
};
// Size: 12 bytes
```

**Wire Format** (little-endian):
```
Offset  Size  Field
0       4     Length
4       2     Type
6       2     Operation Code
8       4     Transaction ID
```

---

#### MTPContainer
**Location**: [mtp.h:160-163](mtp.h#L160-L163)

```c
struct MTPContainer {
    struct MTPHeader header;  // 12 bytes
    uint32_t params[5];       // 20 bytes (up to 5 parameters)
};
// Size: 32 bytes
```

**Usage**: Command packets with parameters

**Example** - DeleteObject:
```
header.op = MTP_OP_DELETE_OBJECT (0x100B)
params[0] = object_handle
params[1-4] = unused (0x00000000)
```

---

#### ObjectInfoHeader
**Location**: [mtp.h:123-138](mtp.h#L123-L138)

```c
typedef struct ObjectInfoHeader {
    uint32_t storage_id;           // Storage where object resides
    uint16_t format;               // MTP_FORMAT_* code
    uint16_t protection_status;    // 0=no protection, 1=read-only
    uint32_t compressed_size;      // File size in bytes

    // Thumbnail info (unused for Flipper)
    uint16_t thumb_format;
    uint32_t thumb_compressed_size;
    uint32_t thumb_pix_width;
    uint32_t thumb_pix_height;

    // Image info (unused for Flipper)
    uint32_t image_pix_width;
    uint32_t image_pix_height;
    uint32_t image_bit_depth;

    uint32_t parent_object;        // Parent directory handle
    uint16_t association_type;     // 0x0001 for directories
    uint32_t association_desc;     // Unused
} ObjectInfoHeader;
// Followed by: filename (MTP string), date_created, date_modified, keywords
```

**Variable-Length Data**: MTP strings follow the fixed header

---

## Implementation Patterns

### Pattern 1: Operation Handler Convention

**Standard Pattern** (used in mtp_ops.c):

```c
// Pattern A: Return data in buffer, return length
int GetOperation(AppMTP* mtp, uint32_t handle, uint8_t* buffer) {
    // 1. Validate parameters
    // 2. Allocate/use provided buffer
    // 3. Build response data
    // 4. Return length (or -1 for error)
}

// Pattern B: Send response directly
void GetOperation(AppMTP* mtp, uint32_t transaction_id, uint32_t param) {
    // 1. Validate parameters
    // 2. Allocate buffer
    // 3. Build response data
    // 4. Send response via send_mtp_response_buffer()
    // 5. Free buffer
}

// Pattern C: Streaming response
void GetOperation(AppMTP* mtp, uint32_t transaction_id, uint32_t handle) {
    // 1. Prepare data source (e.g., open file)
    // 2. Create callback context
    // 3. Send via send_mtp_response_stream()
    // 4. Send OK response
    // 5. Cleanup
}
```

**Examples**:

**Pattern A**: `GetObjectInfo()`, `GetStorageInfo()`
```c
int GetObjectInfo(AppMTP* mtp, uint32_t handle, uint8_t* buffer) {
    ObjectInfoHeader* header = (ObjectInfoHeader*)buffer;
    uint8_t* ptr = buffer + sizeof(ObjectInfoHeader);

    // Fill header fields
    header->storage_id = EXTERNAL_STORAGE_ID;
    header->format = MTP_FORMAT_UNDEFINED;

    // Append variable-length data
    WriteMTPString(ptr, filename, &length);
    ptr += length;

    return ptr - buffer;  // Return total length
}
```

**Pattern B**: `GetDevicePropValue()`
```c
void GetDevicePropValue(AppMTP* mtp, uint32_t transaction_id, uint32_t prop_code) {
    uint8_t* response = malloc(sizeof(uint8_t) * MTP_BUFFER_SIZE);
    int length = GetDevicePropValueInternal(prop_code, response);

    send_mtp_response_buffer(mtp, MTP_TYPE_DATA, MTP_OP_GET_DEVICE_PROP_VALUE,
                             transaction_id, response, length);
    send_mtp_response_buffer(mtp, MTP_TYPE_RESPONSE, MTP_RESP_OK,
                             transaction_id, NULL, 0);
    free(response);
}
```

**Pattern C**: `GetObject()` (file streaming)
```c
void GetObject(AppMTP* mtp, uint32_t transaction_id, uint32_t handle) {
    File* file = storage_file_alloc(storage);
    storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING);

    struct GetObjectContext ctx = { .file = file };

    send_mtp_response_stream(mtp, MTP_TYPE_DATA, MTP_OP_GET_OBJECT,
                            transaction_id, &ctx, GetObject_callback, size);
    send_mtp_response(mtp, MTP_TYPE_RESPONSE, MTP_RESP_OK, transaction_id, NULL);

    storage_file_close(file);
    storage_file_free(file);
}
```

---

### Pattern 2: Buffer Building with Pointer Arithmetic

**Ubiquitous Pattern** throughout codebase:

```c
int BuildResponse(uint8_t* buffer) {
    uint8_t* ptr = buffer;  // Working pointer
    uint16_t length;

    // Write fixed-size field
    *(uint32_t*)ptr = value;
    ptr += sizeof(uint32_t);

    // Write variable-size field
    WriteMTPString(ptr, "string", &length);
    ptr += length;

    // Return total length
    return ptr - buffer;
}
```

**Why This Pattern?**:
- **Efficient**: No copying, direct buffer construction
- **Flexible**: Easy to add variable-length fields
- **MTP-Native**: Matches MTP's variable-length structure format

**Risks**:
- Buffer overflow if `buffer` is too small
- No bounds checking
- Pointer arithmetic errors are silent until corruption

**Current Mitigation**:
- Large fixed buffer sizes (`MTP_BUFFER_SIZE = 1024`)
- Careful manual calculation

---

### Pattern 3: Internal vs External Function Split

**Common Pattern**: Split functions for reusability

```c
// Internal function: Build data, return length
int GetDevicePropValueInternal(uint32_t prop_code, uint8_t* buffer) {
    // Build response in buffer
    return ptr - buffer;
}

// External function: Handle MTP protocol details
void GetDevicePropValue(AppMTP* mtp, uint32_t transaction_id, uint32_t prop_code) {
    uint8_t* buffer = malloc(MTP_BUFFER_SIZE);
    int length = GetDevicePropValueInternal(prop_code, buffer);

    send_mtp_response_buffer(mtp, MTP_TYPE_DATA, op_code, transaction_id, buffer, length);
    send_mtp_response(mtp, MTP_TYPE_RESPONSE, MTP_RESP_OK, transaction_id, NULL);

    free(buffer);
}
```

**Benefits**:
- **Testability**: Internal function can be tested without MTP infrastructure
- **Reusability**: `GetDevicePropDescInternal()` calls `GetDevicePropValueInternal()`
- **Separation**: Protocol handling vs. data construction

**Example**: [device_props.c:7-46](device_props.c#L7-L46) and [device_props.c:108-115](device_props.c#L108-L115)

---

## Object Handle Management

### Handle Lifecycle

```
1. File System Scan
   └─ list_and_issue_handles()
      └─ For each file/directory:
         └─ issue_object_handle(path)

2. Handle Allocation
   └─ issue_object_handle()
      ├─ Check if path exists in list
      ├─ If exists: return existing handle
      └─ If new:
         ├─ Allocate FileHandle node
         ├─ Copy path string
         ├─ Assign handle = (list_length + 1)
         └─ Append to linked list

3. Handle Lookup
   └─ get_path_from_handle()
      └─ Linear scan through list

4. Handle Update (for renames/moves)
   └─ update_object_handle_path()
      ├─ Find handle in list
      ├─ Free old path
      └─ Store new path

5. No Explicit Cleanup
   (Handles persist until app exit)
```

---

### Implementation Details

**File**: [storage_ops.c:24-59](storage_ops.c#L24-L59)

```c
uint32_t issue_object_handle(AppMTP* mtp, char* path) {
    int handle = 1;
    int length = strlen(path);
    char* path_store = malloc(sizeof(char) * (length + 1));
    strcpy(path_store, path);  // Duplicate path

    // Empty list case
    if(mtp->handles == NULL) {
        mtp->handles = malloc(sizeof(FileHandle));
        mtp->handles->handle = handle;
        mtp->handles->path = path_store;
        mtp->handles->next = NULL;
        return handle;
    }

    // Check for existing path
    FileHandle* current = mtp->handles;
    if(strcmp(current->path, path) == 0) {
        free(path_store);  // Don't need duplicate
        return current->handle;
    }

    // Traverse to end, checking for duplicates
    while(current->next != NULL) {
        if(strcmp(current->path, path) == 0) {
            free(path_store);
            return current->handle;
        }
        current = current->next;
        handle++;
    }

    // Append new handle
    current->next = malloc(sizeof(FileHandle));
    current = current->next;
    handle++;

    current->handle = handle;
    current->path = path_store;
    current->next = NULL;

    return handle;
}
```

**Key Behaviors**:
- ✅ **Deduplication**: Same path always gets same handle
- ✅ **Sequential handles**: 1, 2, 3, ... (no gaps)
- ❌ **No reuse**: Deleted file handles never reclaimed
- ❌ **Memory leak potential**: Handles never freed

---

### Handle Scanning Algorithm

**File**: [storage_ops.c:74-130](storage_ops.c#L74-L130)

```c
int list_and_issue_handles(AppMTP* mtp, uint32_t storage_id, uint32_t association,
                           uint32_t* handles) {
    char* base_path = get_base_path_from_storage_id(storage_id);

    File* dir = storage_file_alloc(storage);

    // Open directory
    if(association == 0xffffffff) {
        storage_dir_open(dir, base_path);  // Root directory
    } else {
        char* path = get_path_from_handle(mtp, association);
        storage_dir_open(dir, path);       // Subdirectory
        base_path = path;
    }

    // Scan directory
    int count = 0;
    FileInfo fileinfo;
    char* file_name = malloc(256);
    char* full_path = malloc(256);

    while(storage_dir_read(dir, &fileinfo, file_name, 256)) {
        merge_path(full_path, base_path, file_name);

        uint32_t handle = issue_object_handle(mtp, full_path);

        if(handles != NULL) {
            handles[count] = handle;  // Store handle in output array
        }
        count++;
    }

    storage_dir_close(dir);
    storage_file_free(dir);
    free(file_name);
    free(full_path);

    return count;
}
```

**Dual-Mode Operation**:
1. **Count Mode** (`handles == NULL`): Just return count
2. **Populate Mode** (`handles != NULL`): Fill array with handles

**Usage Pattern**:
```c
// 1. Get count
int count = list_and_issue_handles(mtp, storage_id, association, NULL);

// 2. Allocate array
uint32_t* handles = malloc(sizeof(uint32_t) * count);

// 3. Populate array
list_and_issue_handles(mtp, storage_id, association, handles);
```

**Efficiency Issue**: **Scans directory twice** for every GetObjectHandles operation

---

## MTP String Encoding

### MTP String Format (UTF-16LE)

**Specification**:
```
Byte 0:     String length (number of characters INCLUDING null terminator)
Byte 1-N:   UTF-16LE encoded characters
Last 2:     NULL terminator (0x0000)
```

**Example**: "Hello"
```
Offset  Value   Meaning
0       0x06    Length (5 chars + 1 null = 6)
1       0x48    'H'
2       0x00    (high byte of 'H')
3       0x65    'e'
4       0x00    (high byte of 'e')
5       0x6C    'l'
6       0x00    (high byte of 'l')
7       0x6C    'l'
8       0x00    (high byte of 'l')
9       0x6F    'o'
10      0x00    (high byte of 'o')
11      0x00    Null terminator (low byte)
12      0x00    Null terminator (high byte)

Total: 13 bytes
```

---

### Write Implementation

**File**: [utils.c:86-113](utils.c#L86-L113)

```c
void WriteMTPString(uint8_t* buffer, const char* str, uint16_t* length) {
    uint8_t* ptr = buffer;
    uint8_t str_len = strlen(str);

    // Empty string special case
    if(str_len == 0) {
        *ptr = 0x00;
        *length = 1;
        return;
    }

    // Write length byte (chars + null terminator)
    *ptr = str_len + 1;
    ptr++;

    // Write characters in UTF-16LE
    while(*str) {
        *ptr++ = *str++;        // Low byte (ASCII)
        *ptr++ = 0x00;          // High byte (always 0 for ASCII)
    }

    // Write null terminator
    *ptr++ = 0x00;
    *ptr++ = 0x00;

    *length = ptr - buffer;
}
```

**Assumptions**:
- ✅ Input is **ASCII-only**
- ✅ No characters > 0x7F
- ❌ No support for actual Unicode (multi-byte characters)
- ❌ No validation of input encoding

---

### Read Implementation

**File**: [utils.c:68-83](utils.c#L68-L83)

```c
char* ReadMTPString(uint8_t* buffer) {
    int len16 = *(uint8_t*)buffer;
    if(len16 == 0) {
        return "";  // Empty string (NOTE: not allocated!)
    }

    char* str = malloc(sizeof(char) * len16);

    uint8_t* base = buffer + 1;
    uint16_t* ptr = (uint16_t*)base;

    // Extract low bytes from UTF-16LE
    for(int i = 0; i < len16; i++) {
        str[i] = *ptr++;  // Implicit truncation to char (low byte)
    }

    return str;
}
```

**Issues**:
- ✅ Handles ASCII strings correctly
- ❌ **Memory management inconsistency**: Returns static "" for empty, malloc'd for non-empty
- ❌ **No null termination**: Relies on MTP string having null terminator
- ❌ **Unicode truncation**: Characters > 0xFF are silently truncated

---

### Unicode Detection

**File**: [utils.c:53-66](utils.c#L53-L66)

```c
bool CheckMTPStringHasUnicode(uint8_t* buffer) {
    uint8_t* base = buffer;
    int len = *base;

    uint16_t* ptr = (uint16_t*)(base + 1);

    for(int i = 0; i < len; i++) {
        if(ptr[i] > 0x7F) {  // Non-ASCII
            return true;
        }
    }

    return false;
}
```

**Usage**: [mtp.c:225-230](mtp.c#L225-L230)
```c
if(CheckMTPStringHasUnicode(ptr)) {
    // Fallback to generic names
    name = is_dir ? "New Folder" : "New File";
} else {
    name = ReadMTPString(ptr);
}
```

**Purpose**: Avoid dealing with Unicode filenames (Flipper doesn't support them well)

---

## File Transfer Implementation

### Outbound Transfer (Device → Host): `GetObject`

**Complete Flow**:

```c
// 1. Command received
handle_mtp_command(container)
  └─ case MTP_OP_GET_OBJECT:
     └─ GetObject(mtp, transaction_id, handle)

// 2. File opened
void GetObject(AppMTP* mtp, uint32_t transaction_id, uint32_t handle) {
    char* path = get_path_from_handle(mtp, handle);

    File* file = storage_file_alloc(storage);
    storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING);
    uint32_t size = storage_file_size(file);

    // 3. Stream via callback
    struct GetObjectContext ctx = { .file = file };
    send_mtp_response_stream(
        mtp, MTP_TYPE_DATA, MTP_OP_GET_OBJECT,
        transaction_id, &ctx, GetObject_callback, size
    );

    // 4. Send OK response
    send_mtp_response(mtp, MTP_TYPE_RESPONSE, MTP_RESP_OK, transaction_id, NULL);

    // 5. Cleanup
    storage_file_close(file);
    storage_file_free(file);
}

// 6. Callback reads file chunks
int GetObject_callback(void* ctx, uint8_t* buffer, int length) {
    struct GetObjectContext* obj_ctx = ctx;
    return storage_file_read(obj_ctx->file, buffer, length);
}
```

**Streaming Architecture**:

```
File on SD Card
       ↓
storage_file_read() [Chunk: 512 bytes]
       ↓
GetObject_callback()
       ↓
send_mtp_response_stream() [Builds USB packets]
       ↓
usbd_ep_write() [USB endpoint]
       ↓
Host Computer
```

**Chunk Size**: Determined by `MTP_MAX_PACKET_SIZE` (USB packet size)

**Benefits**:
- ✅ **Memory efficient**: No full file buffering
- ✅ **Scalable**: Works with large files
- ✅ **Low latency**: Streaming starts immediately

---

### Inbound Transfer (Host → Device): `SendObject`

**Phase 1: Object Creation** (`SendObjectInfo`)

```c
// Command: SEND_OBJECT_INFO
handle_mtp_command()
  └─ setup_persistence(container)

// Data packets arrive
handle_mtp_data_packet()
  └─ Accumulate in persistence.global_buffer

// All data received
handle_mtp_data_complete()
  └─ Parse ObjectInfoHeader
  └─ Extract filename
  └─ Create empty file
  └─ Issue handle → store in persistence.prev_handle
  └─ Send response with handle
```

**Phase 2: File Write** (`SendObject`)

```c
// Command: SEND_OBJECT
handle_mtp_command()
  └─ setup_persistence(container)

// Data packets arrive (streaming)
handle_mtp_data_packet() {
    handle = persistence.prev_handle;  // From SendObjectInfo
    path = get_path_from_handle(mtp, handle);

    // Open file on first packet
    if(persistence.current_file == NULL) {
        persistence.current_file = storage_file_alloc(storage);
        storage_file_open(persistence.current_file, path, FSAM_WRITE, FSOM_OPEN_EXISTING);
    }

    // Write chunk
    bytes_written = storage_file_write(persistence.current_file, ptr, bytes_length);
    persistence.buffer_offset += bytes_written;
    persistence.left_bytes -= bytes_written;

    // Periodic sync (every 1MB)
    if(persistence.buffer_offset % MTP_FILE_SYNC_INTERVAL == 0) {
        storage_file_sync(persistence.current_file);
    }

    // Last packet
    if(persistence.left_bytes <= 0) {
        storage_file_sync(persistence.current_file);
        send_mtp_response(mtp, MTP_RESP_OK);
        storage_file_close(persistence.current_file);
        storage_file_free(persistence.current_file);
        persistence.current_file = NULL;
    }
}
```

**Streaming Architecture**:

```
Host Computer
       ↓
USB Bulk OUT
       ↓
mtp_handle_bulk() [Receive packet]
       ↓
handle_mtp_data_packet() [Extract data]
       ↓
storage_file_write() [Write to SD]
       ↓
Periodic storage_file_sync() [Flush to disk]
```

**Key Features**:

1. **No Intermediate Buffering**: Data written directly to file
2. **Progressive Persistence**: Sync every 1MB (`MTP_FILE_SYNC_INTERVAL`)
3. **Error Resilience**: Partial writes detectable
4. **State Management**: Uses global `persistence` structure

---

### Transfer State Machine

```
[Idle]
  │
  │ SEND_OBJECT_INFO command
  ▼
[Awaiting Object Info Data]
  │
  │ Data packets (accumulated in global_buffer)
  ▼
[Object Info Complete]
  │
  │ Create file, issue handle
  ▼
[Object Created] ← persistence.prev_handle stored
  │
  │ SEND_OBJECT command
  ▼
[Awaiting Object Data]
  │
  │ Data packets (streaming write)
  ▼
[Writing File] ← persistence.current_file open
  │
  │ Each packet:
  │ - Write chunk
  │ - Update left_bytes
  │ - Periodic sync
  │
  │ Last packet (left_bytes == 0)
  ▼
[Transfer Complete]
  │
  │ Final sync, close file, send OK
  ▼
[Idle]
```

---

### Error Handling in File Transfers

**Error Scenarios**:

1. **Invalid Handle**:
```c
if(handle == 0) {
    send_mtp_response(mtp, MTP_RESP_INVALID_OBJECT_HANDLE, transaction_id);
    return;
}
```

2. **File Open Failure**:
```c
if(!storage_file_open(file, path, FSAM_WRITE, FSOM_OPEN_EXISTING)) {
    send_mtp_response(mtp, MTP_RESP_INVALID_OBJECT_HANDLE, transaction_id);
    storage_file_free(file);
    persistence.current_file = NULL;
    return;
}
```

3. **Write Failure** (disk full):
```c
if(bytes_written != bytes_length) {
    send_mtp_response(mtp, MTP_RESP_STORE_FULL, transaction_id);
    storage_file_close(file);
    storage_file_free(file);
    persistence.current_file = NULL;
    return;
}
```

**Recovery**:
- ✅ Error responses sent to host
- ✅ File closed and freed
- ✅ State reset
- ❌ **Partial file left on disk** (not deleted after write failure)

---

## Storage Abstraction

### Current Storage Interface Usage

**Direct Flipper API Calls**:

```c
// File operations
storage_file_alloc(storage)
storage_file_open(file, path, mode, open_mode)
storage_file_read(file, buffer, bytes)
storage_file_write(file, buffer, bytes)
storage_file_sync(file)
storage_file_close(file)
storage_file_free(file)
storage_file_size(file)

// Directory operations
storage_dir_open(dir, path)
storage_dir_read(dir, fileinfo, filename, name_length)
storage_dir_close(dir)

// File system operations
storage_file_exists(storage, path)
storage_dir_exists(storage, path)
storage_simply_mkdir(storage, path)
storage_simply_remove_recursive(storage, path)
storage_common_remove(storage, path)
storage_common_rename(storage, old_path, new_path)
storage_common_stat(storage, path, fileinfo)

// Storage info
storage_sd_info(storage, sd_info)
storage_common_fs_info(storage, path, total_space, free_space)
```

**Abstraction Level**: **None** - Direct coupling to Flipper Storage API

---

### Storage ID Mapping

**File**: [storage_ops.c:4-11](storage_ops.c#L4-L11)

```c
char* get_base_path_from_storage_id(uint32_t storage_id) {
    if(storage_id == INTERNAL_STORAGE_ID) {
        return STORAGE_INT_PATH_PREFIX;  // "/int"
    } else if(storage_id == EXTERNAL_STORAGE_ID) {
        return STORAGE_EXT_PATH_PREFIX;  // "/ext"
    }
    return NULL;
}
```

**MTP Storage IDs**:
- `INTERNAL_STORAGE_ID = 0x00010001`
- `EXTERNAL_STORAGE_ID = 0x00020001`

**Flipper Paths**:
- Internal: `/int` (firmware, settings)
- External: `/ext` (SD card)

**Reverse Mapping** (path → storage_id):
```c
// From GetObjectInfo() and GetObjectPropValueInternal()
if(memcmp(path, STORAGE_INT_PATH_PREFIX, strlen(STORAGE_INT_PATH_PREFIX)) == 0) {
    storage_id = INTERNAL_STORAGE_ID;
} else if(memcmp(path, STORAGE_EXT_PATH_PREFIX, strlen(STORAGE_EXT_PATH_PREFIX)) == 0) {
    storage_id = EXTERNAL_STORAGE_ID;
}
```

---

### Storage Information Reporting

**File**: [mtp_ops.c:19-77](mtp_ops.c#L19-L77)

```c
int GetStorageInfo(AppMTP* mtp, uint32_t storage_id, uint8_t* buf) {
    MTPStorageInfoHeader* info = (MTPStorageInfoHeader*)buf;

    // Common fields
    info->free_space_in_objects = 20;  // Arbitrary
    info->filesystem_type = 0x0002;    // Generic hierarchical
    info->access_capability = 0x0000;  // Read-write

    if(storage_id == INTERNAL_STORAGE_ID) {
        info->storage_type = 0x0003;  // Fixed RAM

        uint64_t total, free;
        storage_common_fs_info(storage, STORAGE_INT_PATH_PREFIX, &total, &free);

        info->max_capacity = total / BLOCK_SIZE;
        info->free_space_in_bytes = free / BLOCK_SIZE;

        WriteMTPString(ptr, "Internal Storage", &length);
        WriteMTPString(ptr, "INT_STORAGE", &length);

    } else if(storage_id == EXTERNAL_STORAGE_ID) {
        info->storage_type = 0x0004;  // Removable RAM

        SDInfo sd_info;
        storage_sd_info(storage, &sd_info);

        info->max_capacity = sd_info.kb_total * 1024 / BLOCK_SIZE;
        info->free_space_in_bytes = sd_info.kb_free * 1024 / BLOCK_SIZE;

        WriteMTPString(ptr, "SD Card", &length);
        WriteMTPString(ptr, "SD_CARD", &length);
    }

    return ptr - buf;
}
```

**Capacity Reporting**:
- Values are divided by `BLOCK_SIZE` (65536)
- MTP spec defines units differently than actual bytes
- **Quirk**: `free_space_in_objects` is hardcoded to 20 (not accurate)

---

## Response Construction

### Response Hierarchy

**Three Response Functions**:

1. **send_mtp_response()** - Simple response with parameters
2. **send_mtp_response_buffer()** - Response with pre-built buffer
3. **send_mtp_response_stream()** - Streaming response with callback

---

### Level 1: Simple Response

**File**: [mtp.c:631-645](mtp.c#L631-L645)

```c
void send_mtp_response(
    AppMTP* mtp,
    uint16_t resp_type,      // MTP_TYPE_RESPONSE
    uint16_t resp_code,      // MTP_RESP_OK, etc.
    uint32_t transaction_id,
    uint32_t* params         // Up to 5 parameters (or NULL)
) {
    uint32_t response[5] = {0};

    if(params != NULL) {
        memcpy(response, params, sizeof(uint32_t) * 5);
    }

    send_mtp_response_buffer(
        mtp, resp_type, resp_code, transaction_id,
        (uint8_t*)response, sizeof(response)
    );
}
```

**Usage**:
```c
// Simple OK response
send_mtp_response(mtp, MTP_TYPE_RESPONSE, MTP_RESP_OK, transaction_id, NULL);

// Response with parameters (e.g., SendObjectInfo returns handle)
uint32_t params[5] = {0};
params[0] = storage_id;
params[1] = parent;
params[2] = handle;
send_mtp_response(mtp, MTP_TYPE_RESPONSE, MTP_RESP_OK, transaction_id, params);
```

---

### Level 2: Buffer Response

**File**: [mtp.c:614-629](mtp.c#L614-L629)

```c
void send_mtp_response_buffer(
    AppMTP* mtp,
    uint16_t resp_type,
    uint16_t resp_code,
    uint32_t transaction_id,
    uint8_t* buffer,         // Pre-built data
    uint32_t size            // Data size
) {
    struct MTPResponseBufferContext* ctx = malloc(sizeof(*ctx));
    ctx->buffer = buffer;
    ctx->size = size;
    ctx->sent = 0;

    send_mtp_response_stream(
        mtp, resp_type, resp_code, transaction_id,
        ctx, send_mtp_response_buffer_callback, size
    );

    free(ctx);
}
```

**Callback** (adapter pattern):
```c
int send_mtp_response_buffer_callback(void* ctx, uint8_t* buffer, int size) {
    struct MTPResponseBufferContext* context = ctx;

    uint32_t remaining = context->size - context->sent;
    uint32_t to_send = (remaining < size) ? remaining : size;

    memcpy(buffer, context->buffer + context->sent, to_send);
    context->sent += to_send;

    return to_send;
}
```

**Purpose**: Adapts buffer-based data to streaming interface

---

### Level 3: Streaming Response (Core Implementation)

**File**: [mtp.c:542-594](mtp.c#L542-L594)

```c
void send_mtp_response_stream(
    AppMTP* mtp,
    uint16_t resp_type,
    uint16_t resp_code,
    uint32_t transaction_id,
    void* callback_context,
    int (*callback)(void* ctx, uint8_t* buffer, int length),
    uint32_t length          // Total data length (excluding header)
) {
    int chunk_idx = 0;
    size_t buffer_available = MTP_MAX_PACKET_SIZE;
    uint8_t* buffer = malloc(MTP_MAX_PACKET_SIZE);
    uint32_t sent_length = 0;

    do {
        uint8_t* ptr = buffer;
        buffer_available = MTP_MAX_PACKET_SIZE;

        // First chunk: include MTP header
        if(chunk_idx == 0) {
            struct MTPHeader* hdr = (struct MTPHeader*)buffer;
            hdr->len = length + sizeof(*hdr);
            hdr->type = resp_type;
            hdr->op = resp_code;
            hdr->transaction_id = transaction_id;

            ptr += sizeof(*hdr);
            buffer_available -= sizeof(*hdr);
        }

        // Fill buffer via callback
        int read_bytes = callback(callback_context, ptr, buffer_available);
        uint32_t usb_bytes = (ptr - buffer) + read_bytes;

        // Send USB packet
        usbd_ep_write(mtp->dev, MTP_EP_IN_ADDR, buffer, usb_bytes);

        sent_length += usb_bytes;
        chunk_idx++;

    } while(sent_length < length + sizeof(struct MTPHeader));

    free(buffer);
}
```

**Packet Structure**:

**Chunk 0** (First Packet):
```
[MTPHeader: 12 bytes][Data: up to (MTP_MAX_PACKET_SIZE - 12) bytes]
```

**Chunk 1+** (Subsequent Packets):
```
[Data: up to MTP_MAX_PACKET_SIZE bytes]
```

**Flow**:
```
Iteration 1:
  - Write MTPHeader (12 bytes)
  - Call callback to fill remaining (500 bytes)
  - Send 512-byte USB packet
  - sent_length = 512

Iteration 2:
  - Call callback to fill buffer (512 bytes)
  - Send 512-byte USB packet
  - sent_length = 1024

...

Iteration N:
  - Call callback to fill buffer (100 bytes remaining)
  - Send 100-byte USB packet
  - sent_length = total_length + 12
  - Loop exits
```

---

## Memory Management

### Allocation Patterns

**Stack vs Heap Analysis**:

```c
// PATTERN 1: Stack allocation (small, fixed-size)
FileInfo fileinfo;
char filename[256];

// PATTERN 2: Heap allocation (variable or large)
uint8_t* buffer = malloc(MTP_BUFFER_SIZE);
char* path = malloc(256);

// PATTERN 3: Persistent allocation (handle storage)
FileHandle* node = malloc(sizeof(FileHandle));
node->path = malloc(strlen(path) + 1);
```

---

### Buffer Size Constants

**File**: [mtp.h:92-98](mtp.h#L92-L98)

```c
#define MTP_BUFFER_SIZE 1024         // General purpose buffer
#define MTP_PATH_SIZE   256          // Maximum path length
#define MTP_NAME_SIZE   256          // Maximum filename length
#define MTP_FILE_SYNC_INTERVAL (1024 * 1024)  // 1MB sync interval
```

**Usage**:
- `MTP_BUFFER_SIZE`: Response building buffers
- `MTP_PATH_SIZE`: Path manipulation
- `MTP_NAME_SIZE`: Filename extraction
- `MTP_FILE_SYNC_INTERVAL`: File write sync frequency

---

### Memory Ownership Rules

**Current Conventions** (implicit, not documented):

1. **Caller-allocated buffers**:
   - `GetObjectInfo(mtp, handle, buffer)` - Caller provides buffer
   - Function writes to buffer, returns length

2. **Callee-allocated responses**:
   - `GetDevicePropValue()` - Allocates buffer internally
   - Sends response
   - Frees buffer before return

3. **Persistent allocations**:
   - `issue_object_handle()` - Allocates path copy
   - **Never freed** (application lifetime)

4. **Returned strings**:
   - `ReadMTPString()` - Returns malloc'd string (except empty case!)
   - Caller must free (but this is inconsistent)

**Issues**:
- ❌ **No consistent pattern**
- ❌ **Memory leak potential** in handle system
- ❌ **ReadMTPString() inconsistency** (sometimes static, sometimes malloc)

---

### Resource Cleanup

**File Handles**:
```c
// Allocation
File* file = storage_file_alloc(storage);
storage_file_open(file, path, mode, open_mode);

// Usage
storage_file_read/write(file, ...);

// Cleanup (REQUIRED)
storage_file_close(file);
storage_file_free(file);
```

**Directory Handles**:
```c
File* dir = storage_file_alloc(storage);
storage_dir_open(dir, path);

while(storage_dir_read(dir, ...)) { }

storage_dir_close(dir);
storage_file_free(dir);
```

**Cleanup Pattern**: **Consistent** - All file/dir operations properly cleaned up

---

## Error Handling

### Error Response Pattern

**Standard Pattern**:
```c
if(error_condition) {
    FURI_LOG_E("MTP", "Error message: %s", details);
    send_mtp_response(
        mtp,
        MTP_TYPE_RESPONSE,
        MTP_RESP_ERROR_CODE,
        transaction_id,
        NULL
    );
    // Cleanup resources
    return;
}
```

---

### Error Categories

**1. Invalid Handle**:
```c
char* path = get_path_from_handle(mtp, handle);
if(path == NULL) {
    send_mtp_response(mtp, MTP_TYPE_RESPONSE, MTP_RESP_INVALID_OBJECT_HANDLE,
                     transaction_id, NULL);
    return;
}
```

**2. Storage Errors**:
```c
if(storage_sd_info(storage, &sd_info) != FSE_OK) {
    FURI_LOG_E("MTP", "SD Card not found");
    // Continue with capacity = 0
}
```

**3. File I/O Errors**:
```c
if(!storage_file_open(file, path, mode, open_mode)) {
    send_mtp_response(mtp, MTP_TYPE_RESPONSE, MTP_RESP_INVALID_OBJECT_HANDLE,
                     transaction_id, NULL);
    storage_file_free(file);
    return;
}
```

**4. Write Errors**:
```c
if(bytes_written != bytes_length) {
    send_mtp_response(mtp, MTP_TYPE_RESPONSE, MTP_RESP_STORE_FULL,
                     transaction_id, NULL);
    storage_file_close(file);
    storage_file_free(file);
    return;
}
```

---

### Error Handling Gaps

**Missing Error Checks**:
1. ❌ **Buffer overflow validation**: No bounds checking on buffer writes
2. ❌ **Malloc failure**: No NULL checks after malloc()
3. ❌ **Path length validation**: No check if paths exceed MTP_PATH_SIZE
4. ❌ **Transaction ID validation**: No check for transaction ID mismatch (except in one place)
5. ❌ **Session validation**: Sessions not enforced

**Partial File Cleanup**:
- ✅ Error responses sent
- ✅ Resources freed (file handles, buffers)
- ❌ **Partial files not deleted** after write failure
- ❌ **Handle not removed** from list after delete

---

## Performance Optimizations

### 1. Direct Buffer Construction

**Pattern**:
```c
uint8_t* ptr = buffer;
*(uint32_t*)ptr = value;
ptr += sizeof(uint32_t);
```

**Benefit**: No intermediate copies, direct memory writes

**Alternative** (slower):
```c
uint32_t temp = value;
memcpy(buffer + offset, &temp, sizeof(uint32_t));
offset += sizeof(uint32_t);
```

---

### 2. Streaming File Transfer

**No Buffering**:
```c
// Read directly from file → USB
GetObject_callback() → storage_file_read() → USB packet
```

**Benefit**:
- ✅ Constant memory usage (1 packet buffer)
- ✅ Works with arbitrarily large files
- ✅ Low latency (streaming starts immediately)

---

### 3. Handle Deduplication

**Pattern**:
```c
if(strcmp(current->path, path) == 0) {
    return current->handle;  // Reuse existing
}
```

**Benefit**: Same path always gets same handle (consistency)

**Cost**: O(n) lookup for every `issue_object_handle()` call

---

### 4. Periodic File Sync

**Pattern**:
```c
if(persistence.buffer_offset % MTP_FILE_SYNC_INTERVAL == 0) {
    storage_file_sync(file);
}
```

**Benefit**:
- ✅ Balance between performance and data safety
- ✅ Reduces sync overhead (only every 1MB)
- ✅ Progressive persistence (partial recovery possible)

**Alternative** (worse):
- Sync every packet: Too slow
- Sync only at end: Risk losing entire file on error

---

### 5. Dual-Pass Directory Scan

**Issue** (negative optimization):
```c
// First pass: count
int count = list_and_issue_handles(mtp, storage_id, assoc, NULL);

// Second pass: populate
list_and_issue_handles(mtp, storage_id, assoc, handles);
```

**Cost**: Directory scanned twice for every GetObjectHandles

**Better Approach**:
- Dynamic array with realloc(), single pass
- Or pre-allocate max buffer (if max objects known)

---

## Implementation Quality Assessment

### ✅ Strengths

1. **Streaming I/O**: Excellent memory efficiency for file transfers
2. **Direct Buffer Construction**: Efficient response building
3. **Error Handling**: Most critical errors handled gracefully
4. **Resource Cleanup**: Files and memory consistently freed
5. **MTP Compliance**: Implements core MTP operations correctly
6. **Flipper Integration**: Good use of Flipper Storage API

---

### ⚠️ Weaknesses

1. **Global State**: `persistence` global makes concurrent operations impossible
2. **Memory Leaks**: Object handles never freed, grow indefinitely
3. **O(n) Lookups**: Linked list for handles is inefficient
4. **No Abstraction**: Direct coupling to Flipper Storage API
5. **Inconsistent Memory Management**: `ReadMTPString()` sometimes malloc, sometimes static
6. **No Session Validation**: Sessions are stubs, not enforced
7. **Unicode Handling**: Limited to ASCII, fallback for Unicode filenames
8. **Dual Directory Scan**: GetObjectHandles scans directory twice
9. **Buffer Overflow Risk**: No bounds checking on buffer writes
10. **SOLID Violations**: Large switch statements, mixed responsibilities

---

### 🔍 Code Smells

1. **Magic Numbers**: Hardcoded values (e.g., `free_space_in_objects = 20`)
2. **Copy-Paste**: Similar patterns repeated across operations
3. **Long Functions**: `handle_mtp_command()` is 183 lines with 15+ cases
4. **Mixed Abstraction Levels**: High-level MTP logic mixed with byte manipulation
5. **Unclear Ownership**: Who owns allocated strings from `ReadMTPString()`?

---

## Conclusion

The current MTP implementation is **functional and practical** for the Flipper Zero use case, with particularly strong streaming I/O implementation. However, it exhibits several **maintainability and scalability issues** that would benefit from refactoring:

**High Priority Improvements**:
1. Refactor global `persistence` to encapsulated state objects
2. Optimize handle management (hash table or better data structure)
3. Add abstraction layer for storage operations
4. Consolidate operation handlers with consistent patterns

**Medium Priority**:
5. Fix memory management inconsistencies
6. Add proper session validation
7. Improve Unicode filename support
8. Single-pass directory scanning

**Low Priority**:
9. Add comprehensive error validation (buffer bounds, malloc failures)
10. Reduce code duplication through shared utilities

**Recommended Approach**: Follow the **phased refactoring plan** in [MTP_CONTROL_FLOW_ANALYSIS.md](MTP_CONTROL_FLOW_ANALYSIS.md) to address these issues incrementally while maintaining backward compatibility.

---

**Document Version**: 1.0
**Last Updated**: 2025-11-24
**Author**: Claude Code Analysis
**Status**: Complete Implementation Analysis
