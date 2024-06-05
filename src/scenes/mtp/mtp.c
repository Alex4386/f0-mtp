#include <furi.h>
#include <storage/storage.h>
#include "main.h"
#include "mtp.h"
#include "usb_desc.h"

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
        break;
    }
}

void send_device_info(AppMTP* mtp, uint32_t transaction_id) {
    uint8_t response[256];
    int length = BuildDeviceInfo(response);
    send_mtp_response_buffer(mtp, 2, MTP_RESP_OK, transaction_id, response, length);
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

    for(int i = 0; i < chunk_count; i++) {
        FURI_LOG_I("MTP", "Sending chunk %d/%d", i + 1, chunk_count);
        mtp->write_pending = true;
        size_t chunk_size = (i == chunk_count - 1) ? target_size % MTP_MAX_PACKET_SIZE :
                                                     MTP_MAX_PACKET_SIZE;

        uint8_t* buf = NULL;

        if(i == 0) {
            // first send header
            buf = malloc(chunk_size);
            memcpy(buf, &header, sizeof(header));
            memcpy(buf, ptr, chunk_size - sizeof(header));
            usbd_ep_write(mtp->dev, MTP_EP_OUT_ADDR, buf, chunk_size);
            ptr += chunk_size - sizeof(header);
        } else {
            usbd_ep_write(mtp->dev, MTP_EP_OUT_ADDR, ptr, chunk_size);
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
    struct MTPContainer response = {
        .header =
            {
                .len = sizeof(struct MTPContainer),
                .type = resp_type,
                .op = resp_code,
                .transaction_id = transaction_id,
            },
    };

    if(params != NULL) {
        memcpy(response.params, params, sizeof(response.params));
    }

    usbd_ep_write(mtp->dev, MTP_EP_OUT_ADDR, (uint8_t*)&response, sizeof(response));
}

int mtp_handle_class_control(AppMTP* mtp, usbd_device* dev, usbd_ctlreq* req) {
    UNUSED(dev);

    int value = -1;
    uint32_t handle = req->wIndex;
    uint8_t* buffer = req->data;
    uint32_t size = req->wLength;

    // Extract the MTP operation code from the request
    uint16_t mtp_op = (req->bRequest << 8) | (req->bmRequestType & 0xFF);

    FURI_LOG_I(
        "MTP",
        "Control Request: bmRequestType=0x%02x, bRequest=0x%02x, mtp_op=0x%04x, wValue=0x%04x, wIndex=0x%04lx, wLength=%ld",
        req->bmRequestType,
        req->bRequest,
        mtp_op,
        req->wValue,
        handle,
        size);

    switch(mtp_op) {
    case MTP_REQ_GET_DEVICE_STATUS:
        // Handle GetDeviceStatus request
        struct mtp_device_status* status = (struct mtp_device_status*)req->data;
        status->wLength = sizeof(*status);
        status->wCode = (mtp->state == MTPStateBusy) ? MTP_RESP_DEVICE_BUSY : MTP_RESP_OK;
        value = status->wLength;
        break;
        value = BuildDeviceInfo(buffer);
        break;

    case MTP_OP_GET_STORAGE_IDS:
        // Handle GetStorageIDs request
        uint32_t storage_ids[2];
        uint32_t count;
        GetStorageIDs(mtp, storage_ids, &count);
        memcpy(buffer, storage_ids, count * sizeof(uint32_t));
        value = count * sizeof(uint32_t);
        break;

    case MTP_OP_GET_STORAGE_INFO:
        // Handle GetStorageInfo request
        if(req->wIndex == INTERNAL_STORAGE_ID || req->wIndex == EXTERNAL_STORAGE_ID) {
            MTPStorageInfo info;
            GetStorageInfo(mtp, req->wIndex, &info);
            memcpy(buffer, &info, sizeof(info));
            value = sizeof(info);
        } else {
            // Invalid storage ID
            value = 0;
        }
        break;
    case MTP_OP_SEND_OBJECT_INFO:
        // Handle SendObjectInfo request
        uint32_t parent_handle = req->wIndex; // Assuming parent handle is in wIndex
        char name[256];
        // extract name from req
        bool is_directory = false;
        handle = CreateObject(mtp, parent_handle, name, is_directory);
        // respond with the new handle
        break;
    case MTP_OP_GET_OBJECT:
        ReadObject(mtp, handle, buffer, size);
        // send buffer back in response
        break;
    case MTP_OP_SEND_OBJECT:
        // Handle SendObject request
        handle = req->wIndex; // Assuming handle is in wIndex
        WriteObject(mtp, handle, buffer, size);
        break;
    case MTP_OP_DELETE_OBJECT:
        // Handle DeleteObject request
        handle = req->wIndex; // Assuming handle is in wIndex
        if(!DeleteObject(mtp, handle))
            value = -1;
        else
            value = 0;
        // send confirmation response
        break;
    // Handle other MTP requests
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
    ptr += 2;

    // Vendor extension ID
    *(uint32_t*)ptr = MTP_VENDOR_EXTENSION_ID;
    ptr += 4;

    // Vendor extension version
    *(uint16_t*)ptr = MTP_VENDOR_EXTENSION_VERSION;
    ptr += 2;

    // Vendor extension description
    WriteMTPString(ptr, "microsoft.com: 1.0;", &length);
    ptr += length;

    // Functional mode
    *(uint16_t*)ptr = MTP_FUNCTIONAL_MODE;
    ptr += 2;

    // Supported operations (example, add as needed)
    *(uint32_t*)ptr = 14; // Number of supported operations
    ptr += 4;
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
    for(int i = 0; i < 14; i++) {
        *(uint16_t*)ptr = supported_operations[i];
        ptr += 2;
    }

    // Supported events (example, add as needed)
    *(uint32_t*)ptr = 0; // Number of supported events
    ptr += 4;

    // Supported device properties (example, add as needed)
    *(uint32_t*)ptr = 1; // Number of supported device properties
    ptr += 4;

    uint16_t supported_device_properties[] = {0xd402}; // Device friendly name
    for(int i = 0; i < 1; i++) {
        *(uint16_t*)ptr = supported_device_properties[i];
        ptr += 2;
    }

    // Supported capture formats (example, add as needed)
    *(uint32_t*)ptr = 0; // Number of supported capture formats
    ptr += 4;

    // Supported playback formats (example, add as needed)
    *(uint32_t*)ptr = 2; // Number of supported playback formats
    ptr += 4;

    uint16_t supported_playback_formats[] = {MTP_FORMAT_UNDEFINED, MTP_FORMAT_ASSOCIATION};
    for(int i = 0; i < 2; i++) {
        *(uint16_t*)ptr = supported_playback_formats[i];
        ptr += 2;
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
    WriteMTPString(ptr, "hakurei", &length);
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

void GetStorageInfo(AppMTP* mtp, uint32_t storage_id, MTPStorageInfo* info) {
    if(storage_id == INTERNAL_STORAGE_ID) {
        // Fill in details for internal storage
        info->storage_type = 0x0003; // Fixed RAM
        info->filesystem_type = 0x0002; // Generic hierarchical
        info->access_capability = 0x0000; // Read-write

        // Fill in details for internal storage
        uint64_t total_space;
        uint64_t free_space;
        storage_common_fs_info(mtp->storage, STORAGE_INT_PATH_PREFIX, &total_space, &free_space);

        info->max_capacity = total_space;
        info->free_space_in_bytes = free_space;
        info->free_space_in_objects = 0xFFFFFFFF;

        strcpy(info->storage_description, "Internal Storage");
        strcpy(info->volume_identifier, "int");
    } else if(storage_id == EXTERNAL_STORAGE_ID) {
        SDInfo sd_info;
        FS_Error err = storage_sd_info(mtp->storage, &sd_info);

        // Fill in details for internal storage
        info->storage_type = 0x0004; // Removable RAM
        info->filesystem_type = 0x0002; // Generic hierarchical
        info->access_capability = 0x0000; // Read-write

        if(err != FSE_OK) {
            info->max_capacity = 0;
            info->free_space_in_bytes = 0;
            strcpy(info->storage_description, "External Storage");
            strcpy(info->volume_identifier, "EXT_STORAGE");
            FURI_LOG_E("MTP", "SD Card not found");
            return;
        }

        // Fill in details for external storage
        info->max_capacity = sd_info.kb_total * 1024;
        info->free_space_in_bytes = sd_info.kb_free * 1024;
        strcpy(info->storage_description, "External Storage");
        strcpy(info->volume_identifier, "EXT_STORAGE");
    }
}

// Microsoft-style UTF-16LE string:
void WriteMTPString(uint8_t* buffer, const char* str, uint16_t* length) {
    // MTP strings are UTF-16LE and prefixed with a length byte
    uint8_t* ptr = buffer;
    uint8_t str_len = strlen(str);
    *ptr++ = str_len + 1; // Length byte (includes the length itself and a null terminator)
    while(*str) {
        *ptr++ = *str++;
        *ptr++ = 0x00; // UTF-16LE null byte
    }
    *ptr++ = 0x00; // Null terminator
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