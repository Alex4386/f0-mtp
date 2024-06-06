#include <furi.h>
#include <storage/storage.h>
#include "main.h"
#include "mtp.h"
#include "usb_desc.h"

#define BLOCK_SIZE 65536

// Supported operations (example, add as needed)
uint16_t supported_operations[] = {
    MTP_OP_GET_DEVICE_INFO,
    MTP_OP_OPEN_SESSION,
    MTP_OP_CLOSE_SESSION,
    MTP_OP_GET_STORAGE_IDS,
    MTP_OP_GET_STORAGE_INFO,
    MTP_OP_GET_NUM_OBJECTS,
    MTP_OP_GET_OBJECT_HANDLES,
    MTP_OP_GET_OBJECT_INFO,
    MTP_OP_GET_OBJECT,
    MTP_OP_SEND_OBJECT_INFO,
    MTP_OP_SEND_OBJECT,
    MTP_OP_DELETE_OBJECT,
    MTP_OP_GET_DEVICE_PROP_DESC,
    MTP_OP_GET_DEVICE_PROP_VALUE};

// Supported device properties (example, add as needed)
uint16_t supported_device_properties[] = {
    0xd402, // Device friendly name
};

uint16_t supported_playback_formats[] = {
    MTP_FORMAT_UNDEFINED,
    MTP_FORMAT_ASSOCIATION,
};

void mtp_handle_bulk(AppMTP* mtp, uint8_t* buffer, int32_t length) {
    UNUSED(mtp);

    if(length < 12) {
        FURI_LOG_E("MTP", "Invalid MTP packet");
        return;
    }
    struct MTPContainer* container = (struct MTPContainer*)buffer;
    uint16_t mtp_op = container->header.op;
    FURI_LOG_I("MTP", "Handling MTP operation: 0x%04x", mtp_op);

    switch(mtp_op) {
    case MTP_OP_GET_DEVICE_INFO:
        FURI_LOG_I("MTP", "GetDeviceInfo operation");
        send_device_info(mtp, container->header.transaction_id);
        // Process the GetDeviceInfo operation
        break;
    case MTP_OP_OPEN_SESSION:
    case MTP_OP_CLOSE_SESSION:
        FURI_LOG_I("MTP", "Open/CloseSession operation (STUB)");
        send_mtp_response(mtp, 3, MTP_RESP_OK, container->header.transaction_id, NULL);
        break;
    case MTP_OP_GET_STORAGE_IDS:
        FURI_LOG_I("MTP", "GetStorageIDs operation");
        send_storage_ids(mtp, container->header.transaction_id);
        break;
    case MTP_OP_GET_STORAGE_INFO: {
        FURI_LOG_I("MTP", "GetStorageInfo operation");
        uint8_t info[256];
        int length = GetStorageInfo(mtp, container->params[0], info);
        send_mtp_response_buffer(
            mtp,
            MTP_TYPE_DATA,
            MTP_OP_GET_STORAGE_INFO,
            container->header.transaction_id,
            (uint8_t*)&info,
            length);
        send_mtp_response_buffer(
            mtp, MTP_TYPE_RESPONSE, MTP_RESP_OK, container->header.transaction_id, NULL, 0);
        break;
    }
    case MTP_OP_GET_OBJECT_HANDLES:
        FURI_LOG_I("MTP", "GetObjectHandles operation");
        if(container->params[1]) {
            send_mtp_response_buffer(
                mtp,
                MTP_TYPE_RESPONSE,
                MTP_RESP_SPEC_BY_FORMAT_UNSUPPORTED,
                container->header.transaction_id,
                NULL,
                0);
            break;
        } else {
            uint8_t buffer[256];
            int length = GetObjectHandles(mtp, container->params[0], container->params[2], buffer);
            send_mtp_response_buffer(
                mtp,
                MTP_TYPE_DATA,
                MTP_OP_GET_OBJECT_HANDLES,
                container->header.transaction_id,
                buffer,
                length);

            send_mtp_response_buffer(
                mtp, MTP_TYPE_RESPONSE, MTP_RESP_OK, container->header.transaction_id, NULL, 0);
        }
        break;
    case MTP_OP_GET_OBJECT_INFO: {
        FURI_LOG_I("MTP", "GetObjectInfo operation");
        uint8_t* buffer = malloc(sizeof(uint8_t) * 512);
        int length = GetObjectInfo(mtp, container->params[0], buffer);

        if(length < 0) {
            send_mtp_response(
                mtp, 3, MTP_RESP_INVALID_OBJECT_HANDLE, container->header.transaction_id, NULL);
            break;
        }

        send_mtp_response_buffer(
            mtp,
            MTP_TYPE_DATA,
            MTP_OP_GET_OBJECT_INFO,
            container->header.transaction_id,
            buffer,
            length);
        free(buffer);

        send_mtp_response_buffer(
            mtp, MTP_TYPE_RESPONSE, MTP_RESP_OK, container->header.transaction_id, NULL, 0);
        break;
    }

    case MTP_OP_GET_DEVICE_PROP_VALUE:
        FURI_LOG_I("MTP", "GetDevicePropValue operation");
        send_device_prop_value(mtp, container->header.transaction_id, container->params[0]);
        // Process the GetDevicePropValue operation
        break;
    case MTP_OP_GET_DEVICE_PROP_DESC:
        FURI_LOG_I("MTP", "GetDevicePropDesc operation");
        send_device_prop_desc(mtp, container->header.transaction_id, container->params[0]);
        // Process the GetDevicePropDesc operation
        break;
    // Handle bulk transfer specific operations
    case MTP_OP_SEND_OBJECT_INFO:
        FURI_LOG_I("MTP", "SendObjectInfo operation");
        // Process the SendObjectInfo operation
        break;
    case MTP_OP_GET_OBJECT:
        FURI_LOG_I("MTP", "GetObject operation");
        // Process the GetObject operation
        break;
    // Handle other bulk operations here...
    default:
        FURI_LOG_W("MTP", "Unsupported MTP operation in bulk transfer: 0x%04x", mtp_op);
        send_mtp_response(mtp, 3, MTP_RESP_UNKNOWN, container->header.transaction_id, NULL);
        break;
    }
}

void send_storage_ids(AppMTP* mtp, uint32_t transaction_id) {
    uint32_t count;
    uint32_t storage_ids[3];
    GetStorageIDs(mtp, storage_ids, &count);

    FURI_LOG_I("MTP", "Sending storage IDs: %ld storages", count);
    uint32_t payload[3] = {count, storage_ids[0], storage_ids[1]};
    send_mtp_response_buffer(
        mtp,
        MTP_TYPE_DATA,
        MTP_OP_GET_STORAGE_IDS,
        transaction_id,
        (uint8_t*)payload,
        sizeof(uint32_t) * (count + 1));
    send_mtp_response_buffer(mtp, MTP_TYPE_RESPONSE, MTP_RESP_OK, transaction_id, NULL, 0);
}

void send_device_info(AppMTP* mtp, uint32_t transaction_id) {
    uint8_t response[256];
    int length = BuildDeviceInfo(response);
    send_mtp_response_buffer(
        mtp, MTP_TYPE_DATA, MTP_OP_GET_DEVICE_INFO, transaction_id, response, length);
    send_mtp_response_buffer(mtp, MTP_TYPE_RESPONSE, MTP_RESP_OK, transaction_id, NULL, 0);
}

void send_device_prop_value(AppMTP* mtp, uint32_t transaction_id, uint32_t prop_code) {
    uint8_t response[256];
    int length = GetDevicePropValue(prop_code, response);
    send_mtp_response_buffer(
        mtp, MTP_TYPE_DATA, MTP_OP_GET_DEVICE_PROP_VALUE, transaction_id, response, length);
    send_mtp_response_buffer(mtp, MTP_TYPE_RESPONSE, MTP_RESP_OK, transaction_id, NULL, 0);
}

void send_device_prop_desc(AppMTP* mtp, uint32_t transaction_id, uint32_t prop_code) {
    uint8_t response[256];
    int length = GetDevicePropDesc(prop_code, response);
    send_mtp_response_buffer(
        mtp, MTP_TYPE_DATA, MTP_OP_GET_DEVICE_PROP_DESC, transaction_id, response, length);
    send_mtp_response_buffer(mtp, MTP_TYPE_RESPONSE, MTP_RESP_OK, transaction_id, NULL, 0);
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

uint32_t issue_object_handle(AppMTP* mtp, char* path) {
    int handle = 0;
    int length = strlen(path);
    char* path_store = malloc(sizeof(char) * (length + 1));
    strcpy(path_store, path);

    if(mtp->handles == NULL) {
        mtp->handles = malloc(sizeof(FileHandle));
        mtp->handles->handle = handle;
        mtp->handles->path = path_store;
        mtp->handles->next = NULL;
        return handle;
    }

    FileHandle* current = mtp->handles;
    if(strcmp(current->path, path) == 0) {
        return current->handle;
    }

    while(current->next != NULL) {
        if(strcmp(current->path, path) == 0) {
            return current->handle;
        }
        current = current->next;
        handle++;
    }

    current->next = malloc(sizeof(FileHandle));
    current = current->next;
    handle++;

    current->handle = handle;
    current->path = path_store;
    current->next = NULL;
    return handle;
}

int list_and_issue_handles(
    AppMTP* mtp,
    uint32_t storage_id,
    uint32_t association,
    uint32_t* handles) {
    Storage* storage = mtp->storage;
    char* base_path = "";
    if(storage_id == INTERNAL_STORAGE_ID) {
        base_path = STORAGE_INT_PATH_PREFIX;
    } else if(storage_id == EXTERNAL_STORAGE_ID) {
        base_path = STORAGE_EXT_PATH_PREFIX;
    }

    File* dir = storage_file_alloc(storage);
    if(association == 0xffffffff) {
        // count the objects in the root directory
        storage_dir_open(dir, base_path);
    } else {
        char* path = get_path_from_handle(mtp, association);
        if(path == NULL) {
            return 0;
        }
        storage_dir_open(dir, path);
    }

    int count = 0;
    FileInfo fileinfo;
    char* file_name = malloc(sizeof(char) * 256);
    char* full_path = malloc(sizeof(char) * 256);
    while(storage_dir_read(dir, &fileinfo, file_name, 256)) {
        if(file_info_is_dir(&fileinfo)) {
            FURI_LOG_I("MTP", "Found directory: %s", file_name);
        } else {
            FURI_LOG_I("MTP", "Found file: %s", file_name);
        }

        // implement this way since strcat is unavailable
        char* ptr = full_path;
        strcpy(full_path, base_path);
        ptr += strlen(base_path);
        strcpy(ptr, "/");
        ptr++;
        strcpy(ptr, file_name);

        FURI_LOG_I("MTP", "Full path: %s", full_path);

        uint32_t handle = issue_object_handle(mtp, full_path);
        if(handles != NULL) {
            handles[count] = handle;
        }
        count++;
    }

    FURI_LOG_I("MTP", "Getting number of objects in storage %ld", storage_id);
    FURI_LOG_I("MTP", "Base path: %s", base_path);
    FURI_LOG_I("MTP", "Association: %ld", association);
    FURI_LOG_I("MTP", "Number of objects: %d", count);

    storage_file_free(dir);
    free(file_name);
    free(full_path);
    return count;
}

int GetObjectInfo(AppMTP* mtp, uint32_t handle, uint8_t* buffer) {
    ObjectInfoHeader* header = (ObjectInfoHeader*)buffer;
    char* path = get_path_from_handle(mtp, handle);
    if(path == NULL) {
        return -1;
    }

    FURI_LOG_I("MTP", "Getting object info for handle %ld", handle);
    FURI_LOG_I("MTP", "Path: %s", path);

    header->protection_status = 0;

    Storage* storage = mtp->storage;
    File* file = storage_file_alloc(storage);
    uint16_t length;

    FileInfo fileinfo;
    storage_common_stat(storage, path, &fileinfo);

    if(memcmp(path, STORAGE_INT_PATH_PREFIX, strlen(STORAGE_INT_PATH_PREFIX)) == 0) {
        header->storage_id = INTERNAL_STORAGE_ID;
    } else if(memcmp(path, STORAGE_EXT_PATH_PREFIX, strlen(STORAGE_EXT_PATH_PREFIX)) == 0) {
        header->storage_id = EXTERNAL_STORAGE_ID;
    } else {
        return -1;
    }

    if(file_info_is_dir(&fileinfo)) {
        header->format = MTP_FORMAT_ASSOCIATION;
        header->association_type = 0x0001; // Generic folder
    } else {
        header->format = MTP_FORMAT_UNDEFINED;
    }

    header->compressed_size = 0;

    /*
    // is on root directory (/int or /ext)
    if(strchr(path + 1, '/') == NULL) {
        header->parent_object = 0;
    } else {
        char* parent_path = malloc(sizeof(char) * 256);
        strcpy(parent_path, path);
        char* last_slash = strrchr(parent_path, '/');
        *last_slash = '\0';
        header->parent_object = issue_object_handle(mtp, parent_path);

        free(parent_path);
    }
    */

    uint8_t* ptr = buffer + sizeof(ObjectInfoHeader);

    char* file_name = strrchr(path, '/');
    FURI_LOG_I("MTP", "File name: %s", file_name + 1);

    WriteMTPString(ptr, file_name + 1, &length);
    ptr += length;

    // get last modified
    WriteMTPString(ptr, "20231222T190925", &length);
    ptr += length;

    // get last modified
    WriteMTPString(ptr, "20231222T190925", &length);
    ptr += length;

    storage_file_free(file);

    return ptr - buffer;
}

int GetNumObjects(AppMTP* mtp, uint32_t storage_id, uint32_t association) {
    return list_and_issue_handles(mtp, storage_id, association, NULL);
}

int GetObjectHandles(AppMTP* mtp, uint32_t storage_id, uint32_t association, uint8_t* buffer) {
    uint8_t* ptr = buffer;
    uint16_t length;

    UNUSED(length);

    // For now, just return a single object handle
    int count = GetNumObjects(mtp, storage_id, association);

    *(uint32_t*)ptr = count;
    ptr += sizeof(uint32_t);

    uint32_t* handles = (uint32_t*)ptr;
    length = list_and_issue_handles(mtp, storage_id, association, handles);
    ptr += length * sizeof(uint32_t);

    return ptr - buffer;
}

int GetDevicePropValue(uint32_t prop_code, uint8_t* buffer) {
    uint8_t* ptr = buffer;
    uint16_t length;

    switch(prop_code) {
    case 0xd402:
        WriteMTPString(ptr, "Flipper Zero", &length);
        ptr += length;
        break;
    default:
        // Unsupported property
        break;
    }

    return ptr - buffer;
}

int GetDevicePropDesc(uint32_t prop_code, uint8_t* buffer) {
    uint8_t* ptr = buffer;
    uint16_t length;

    switch(prop_code) {
    case 0xd402:
        // Device friendly name
        *(uint16_t*)ptr = prop_code;
        ptr += 2;

        // type is string
        *(uint16_t*)ptr = 0xffff;
        ptr += 2;

        // read-only
        *(uint16_t*)ptr = 0x0000;
        ptr += 2;

        length = GetDevicePropValue(prop_code, ptr);
        ptr += length;

        length = GetDevicePropValue(prop_code, ptr);
        ptr += length;

        // no-form
        *(uint16_t*)ptr = 0x0000;
        ptr += 2;
        break;
    default:
        // Unsupported property
        break;
    }

    return ptr - buffer;
}

void send_mtp_response_buffer(
    AppMTP* mtp,
    uint16_t resp_type,
    uint16_t resp_code,
    uint32_t transaction_id,
    uint8_t* buffer,
    uint32_t size) {
    size_t target_size = size + sizeof(struct MTPHeader);
    struct MTPHeader header = {
        .len = target_size,
        .type = resp_type,
        .op = resp_code,
        .transaction_id = transaction_id,
    };

    // Send the response in one go
    /*
    uint8_t* response = malloc(target_size);
    memcpy(response, &header, sizeof(header));
    memcpy(response + sizeof(header), buffer, size);

    usbd_ep_write(mtp->dev, MTP_EP_OUT_ADDR, response, target_size);
    */

    // Send the response in chunks
    int chunk_count = (target_size / MTP_MAX_PACKET_SIZE) + 1;
    FURI_LOG_I("MTP", "Sending MTP response: %d bytes in %d chunks", target_size, chunk_count);
    uint8_t* ptr = buffer;

    mtp->write_pending = true;

    for(int i = 0; i < chunk_count; i++) {
        FURI_LOG_I("MTP", "Sending chunk %d/%d", i + 1, chunk_count);
        mtp->write_pending = true;
        size_t chunk_size = (i == chunk_count - 1) ? target_size % MTP_MAX_PACKET_SIZE :
                                                     MTP_MAX_PACKET_SIZE;

        uint8_t* buf = NULL;

        if(i == 0) {
            // first send header
            buf = malloc(chunk_size);
            FURI_LOG_I("MTP", "1st chunk! size = %d", chunk_size);
            memcpy(buf, &header, sizeof(header));
            memcpy(buf + sizeof(header), ptr, chunk_size - sizeof(header));
            usbd_ep_write(mtp->dev, MTP_EP_IN_ADDR, buf, chunk_size);
            ptr += chunk_size - sizeof(header);
        } else {
            usbd_ep_write(mtp->dev, MTP_EP_IN_ADDR, ptr, chunk_size);
            ptr += chunk_size;
        }

        FURI_LOG_I(
            "MTP",
            "Sent chunk %d/%d (offset: %d)",
            i + 1,
            chunk_count,
            ptr - buffer + sizeof(header));

        if(buf != NULL) {
            free(buf);
        }
    }
}

void send_mtp_response(
    AppMTP* mtp,
    uint16_t resp_type,
    uint16_t resp_code,
    uint32_t transaction_id,
    uint32_t* params) {
    uint32_t response[5] = {0};

    if(params != NULL) {
        memcpy(response, params, sizeof(uint32_t) * 5);
    }

    send_mtp_response_buffer(
        mtp, resp_type, resp_code, transaction_id, (uint8_t*)response, sizeof(response));
}

int mtp_handle_class_control(AppMTP* mtp, usbd_device* dev, usbd_ctlreq* req) {
    UNUSED(dev);

    int value = -1;
    uint32_t handle = req->wIndex;
    uint8_t* buffer = req->data;
    uint32_t size = req->wLength;

    UNUSED(handle);
    UNUSED(buffer);
    UNUSED(size);

    switch(req->bRequest) {
    case MTP_REQ_CANCEL:
        // Handle Cancel request
        value = 0;
        break;
    case MTP_REQ_GET_EXT_EVENT_DATA:
        // Handle GetExtEventData request
        value = 0;
        break;
    case MTP_REQ_RESET:
        // Handle Reset request
        mtp->state = MTPStateReady;
        value = 0;
        break;
    case MTP_REQ_GET_DEVICE_STATUS:
        // Handle GetDeviceStatus request
        struct mtp_device_status* status = (struct mtp_device_status*)req->data;
        status->wLength = sizeof(*status);
        status->wCode = (mtp->state == MTPStateBusy) ? MTP_RESP_DEVICE_BUSY : MTP_RESP_OK;
        value = status->wLength;
        break;
    default:
        // Unsupported request
        break;
    }

    return value;
}
int BuildDeviceInfo(uint8_t* buffer) {
    uint8_t* ptr = buffer;
    uint16_t length;

    // Standard version
    *(uint16_t*)ptr = 100;
    ptr += sizeof(uint16_t);

    // Vendor extension ID
    *(uint32_t*)ptr = MTP_VENDOR_EXTENSION_ID;
    ptr += sizeof(uint32_t);

    // Vendor extension version
    *(uint16_t*)ptr = MTP_VENDOR_EXTENSION_VERSION;
    ptr += sizeof(uint16_t);

    // Vendor extension description
    WriteMTPString(ptr, "microsoft.com: 1.0;", &length);
    ptr += length;

    // Functional mode
    *(uint16_t*)ptr = MTP_FUNCTIONAL_MODE;
    ptr += sizeof(uint16_t);

    // Operations supported
    length = sizeof(supported_operations) / sizeof(uint16_t);
    *(uint32_t*)ptr = length; // Number of supported operations
    ptr += sizeof(uint32_t);

    for(int i = 0; i < length; i++) {
        *(uint16_t*)ptr = supported_operations[i];
        ptr += sizeof(uint16_t);
    }

    // Supported events (example, add as needed)
    *(uint32_t*)ptr = 0; // Number of supported events
    ptr += sizeof(uint32_t);

    length = sizeof(supported_device_properties) / sizeof(uint16_t);
    *(uint32_t*)ptr = length; // Number of supported device properties
    ptr += sizeof(uint32_t);

    for(int i = 0; i < length; i++) {
        *(uint16_t*)ptr = supported_device_properties[i];
        ptr += sizeof(uint16_t);
    }

    // Supported capture formats (example, add as needed)
    *(uint32_t*)ptr = 0; // Number of supported capture formats
    ptr += sizeof(uint32_t);

    // Supported playback formats (example, add as needed)
    length = sizeof(supported_playback_formats) / sizeof(uint16_t);
    *(uint32_t*)ptr = length; // Number of supported playback formats
    ptr += sizeof(uint32_t);

    for(int i = 0; i < length; i++) {
        *(uint16_t*)ptr = supported_playback_formats[i];
        ptr += sizeof(uint16_t);
    }

    // Manufacturer
    WriteMTPString(ptr, USB_MANUFACTURER_STRING, &length);
    ptr += length;

    // Model
    WriteMTPString(ptr, USB_DEVICE_MODEL, &length);
    ptr += length;

    // Device version
    WriteMTPString(ptr, "1.0", &length);
    ptr += length;

    // Serial number
    WriteMTPString(ptr, "HakureiReimu", &length);
    ptr += length;

    return ptr - buffer;
}

void GetStorageIDs(AppMTP* mtp, uint32_t* storage_ids, uint32_t* count) {
    SDInfo sd_info;
    FS_Error err = storage_sd_info(mtp->storage, &sd_info);
    storage_ids[0] = INTERNAL_STORAGE_ID;
    if(err != FSE_OK) {
        FURI_LOG_E("MTP", "SD Card not found");
        *count = 1; // We have only one storage
        return;
    }

    storage_ids[1] = EXTERNAL_STORAGE_ID;
    *count = 2; // We have two storages: internal and external
}

int GetStorageInfo(AppMTP* mtp, uint32_t storage_id, uint8_t* buf) {
    MTPStorageInfoHeader* info = (MTPStorageInfoHeader*)buf;
    uint8_t* ptr = buf + sizeof(MTPStorageInfoHeader);
    uint16_t length;

    info->free_space_in_objects = 20ul;
    info->filesystem_type = 0x0002; // Generic hierarchical
    info->access_capability = 0x0000; // Read-write

    if(storage_id == INTERNAL_STORAGE_ID) {
        // Fill in details for internal storage
        info->storage_type = 0x0003; // Fixed RAM

        // Fill in details for internal storage
        uint64_t total_space;
        uint64_t free_space;
        FS_Error err = storage_common_fs_info(
            mtp->storage, STORAGE_INT_PATH_PREFIX, &total_space, &free_space);
        if(err != FSE_OK) {
            info->max_capacity = 0;
            info->free_space_in_bytes = 0;
        } else {
            info->max_capacity = total_space / BLOCK_SIZE;
            info->free_space_in_bytes = free_space / BLOCK_SIZE;
        }

        WriteMTPString(ptr, "Internal Storage", &length);
        ptr += length;

        WriteMTPString(ptr, "INT_STORAGE", &length);
        ptr += length;
    } else if(storage_id == EXTERNAL_STORAGE_ID) {
        SDInfo sd_info;
        FS_Error err = storage_sd_info(mtp->storage, &sd_info);

        // Fill in details for internal storage
        info->storage_type = 0x0004; // Removable RAM

        if(err != FSE_OK) {
            info->max_capacity = 0;
            info->free_space_in_bytes = 0;

            FURI_LOG_E("MTP", "SD Card not found");
        } else {
            // Fill in details for external storage
            info->max_capacity = (uint64_t)sd_info.kb_total * 1024 / BLOCK_SIZE;
            info->free_space_in_bytes = (uint64_t)sd_info.kb_free * 1024 / BLOCK_SIZE;
        }

        WriteMTPString(ptr, "SD Card", &length);
        ptr += length;

        WriteMTPString(ptr, "SD_CARD", &length);
        ptr += length;
    }

    // try to convert into big endian???
    //info->max_capacity = __builtin_bswap64(info->max_capacity);
    //info->free_space_in_bytes = __builtin_bswap64(info->free_space_in_bytes);

    return ptr - buf;
}

// Microsoft-style UTF-16LE string:
void WriteMTPString(uint8_t* buffer, const char* str, uint16_t* length) {
    uint8_t* ptr = buffer;
    uint8_t str_len = strlen(str);
    *ptr = str_len + 1; // Length byte (number of characters including the null terminator)
    ptr++;
    while(*str) {
        *ptr++ = *str++;
        *ptr++ = 0x00; // UTF-16LE encoding (add null byte for each character)
    }
    *ptr++ = 0x00; // Null terminator (UTF-16LE)
    *ptr++ = 0x00;
    *length = ptr - buffer;
}

void WriteMTPBEString(uint8_t* buffer, const char* str, uint16_t* length) {
    uint8_t* ptr = buffer;
    uint8_t str_len = strlen(str);
    *ptr++ = str_len + 1; // Length byte (number of characters including the null terminator)
    while(*str) {
        *ptr++ = 0x00; // UTF-16BE encoding (add null byte for each character)
        *ptr++ = *str++;
    }
    *ptr++ = 0x00; // Null terminator (UTF-16LE)
    *ptr++ = 0x00;
    *length = ptr - buffer;
}

uint32_t CreateObject(AppMTP* mtp, uint32_t parent_handle, const char* name, bool is_directory) {
    UNUSED(mtp);
    MTPObject new_object;
    UNUSED(new_object);
    UNUSED(parent_handle);
    UNUSED(name);
    UNUSED(is_directory);

    FURI_LOG_I("MTP", "Creating object %s", name);
    return 0;
}

uint32_t ReadObject(AppMTP* mtp, uint32_t handle, uint8_t* buffer, uint32_t size) {
    UNUSED(mtp);
    UNUSED(buffer);
    UNUSED(size);
    FURI_LOG_I("MTP", "Reading object %ld", handle);

    return 0;
}

void WriteObject(AppMTP* mtp, uint32_t handle, uint8_t* data, uint32_t size) {
    UNUSED(mtp);
    UNUSED(data);
    UNUSED(size);
    FURI_LOG_I("MTP", "Writing object %ld", handle);
}

bool DeleteObject(AppMTP* mtp, uint32_t handle) {
    UNUSED(mtp);
    FURI_LOG_I("MTP", "Deleting object %ld", handle);
    return true;
}