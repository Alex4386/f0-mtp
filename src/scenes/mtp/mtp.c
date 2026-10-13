#include <furi.h>
#include <power/power_service/power.h>
#include <storage/storage.h>
#include "main.h"
#include "mtp.h"
#include "mtp_ops.h"
#include "mtp_support.h"
#include "device_props.h"
#include "storage_ops.h"
#include "usb.h"
#include "usb_desc.h"
#include "utils.h"

// temporary storage for buffer
MTPDataPersistence persistence;

void mtp_reset_transfer(AppMTP* mtp) {
    UNUSED(mtp);

    if(persistence.current_file != NULL) {
        storage_file_close(persistence.current_file);
        storage_file_free(persistence.current_file);
    }

    if(persistence.global_buffer != NULL) {
        free(persistence.global_buffer);
    }

    memset(&persistence, 0, sizeof(persistence));
}

void mtp_handle_bulk(AppMTP* mtp, uint8_t* buffer, uint32_t length) {
    if(persistence.left_bytes > 0) {
        handle_mtp_data_packet(mtp, buffer, length, true);
        return;
    }

    if(length < sizeof(struct MTPHeader)) {
        FURI_LOG_E("MTP", "Invalid MTP packet");
        return;
    }

    struct MTPHeader* header = (struct MTPHeader*)buffer;
    uint16_t type = header->type;

    if(type == MTP_TYPE_COMMAND) {
        struct MTPContainer* container = (struct MTPContainer*)buffer;

        // params that were not sent must not leak in from the previous packet
        uint32_t param_count = (MIN(length, header->len) - sizeof(struct MTPHeader)) / 4;
        for(uint32_t i = param_count; i < 5; i++) {
            container->params[i] = 0;
        }

        handle_mtp_command(mtp, container);
    } else if(type == MTP_TYPE_DATA) {
        handle_mtp_data_packet(mtp, buffer, length, false);
    } else if(type == MTP_TYPE_RESPONSE) {
        handle_mtp_response(mtp, header);
    } else {
        FURI_LOG_W("MTP", "Unsupported MTP packet type: %d", type);
    }
}

void setup_persistence(struct MTPContainer* container) {
    persistence.transaction_id = container->header.transaction_id;
    memcpy(persistence.params, container->params, sizeof(uint32_t) * 5);
}

static bool flush_object_buffer(void) {
    if(persistence.buffer_offset == 0) return true;

    size_t written = storage_file_write(
        persistence.current_file, persistence.global_buffer, persistence.buffer_offset);
    bool ok = written == persistence.buffer_offset;
    persistence.buffer_offset = 0;
    return ok;
}

static void close_object_file(void) {
    if(persistence.current_file == NULL) return;

    storage_file_close(persistence.current_file);
    storage_file_free(persistence.current_file);
    persistence.current_file = NULL;
}

static void begin_send_object(AppMTP* mtp) {
    char* path = get_path_from_handle(mtp, persistence.prev_handle);
    if(persistence.prev_handle == 0 || path == NULL) {
        FURI_LOG_E("MTP", "SendObject without valid SendObjectInfo");
        persistence.resp_code = MTP_RESP_NO_VALID_OBJECT_INFO;
        return;
    }

    FURI_LOG_I("MTP", "Writing to file: %s", path);

    File* file = storage_file_alloc(mtp->storage);
    if(!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_E("MTP", "Failed to open file: %s", path);
        storage_file_free(file);
        persistence.resp_code = MTP_RESP_ACCESS_DENIED;
        return;
    }

    persistence.current_file = file;
    persistence.global_buffer = malloc(MTP_FILE_BUFFER_SIZE);
}

static void write_send_object(uint8_t* data, uint32_t size) {
    while(persistence.current_file != NULL && size > 0) {
        uint32_t chunk = MIN(size, MTP_FILE_BUFFER_SIZE - persistence.buffer_offset);
        memcpy(persistence.global_buffer + persistence.buffer_offset, data, chunk);
        persistence.buffer_offset += chunk;
        data += chunk;
        size -= chunk;

        if(persistence.buffer_offset == MTP_FILE_BUFFER_SIZE && !flush_object_buffer()) {
            FURI_LOG_E("MTP", "Write failed");
            persistence.resp_code = MTP_RESP_STORE_FULL;
            close_object_file();
        }
    }
}

static void end_send_object(AppMTP* mtp) {
    if(persistence.current_file != NULL) {
        if(!flush_object_buffer()) {
            FURI_LOG_E("MTP", "Write failed");
            persistence.resp_code = MTP_RESP_STORE_FULL;
        }
        close_object_file();
    }

    free(persistence.global_buffer);
    persistence.global_buffer = NULL;

    // an ObjectInfo is good for one object only
    persistence.prev_handle = 0;

    FURI_LOG_I("MTP", "File transfer completed: 0x%04x", persistence.resp_code);
    send_mtp_response(
        mtp, MTP_TYPE_RESPONSE, persistence.resp_code, persistence.transaction_id, NULL);
}

void handle_mtp_data_packet(AppMTP* mtp, uint8_t* buffer, int32_t length, bool cont) {
    uint8_t* ptr = buffer;
    uint32_t size = length;

    if(!cont) {
        struct MTPHeader* header = (struct MTPHeader*)buffer;

        if(header->transaction_id != persistence.transaction_id) {
            // the params value can not be trusted.
            // reset the params
            memset(persistence.params, 0, sizeof(persistence.params));
        }

        persistence.op = header->op;
        persistence.left_bytes =
            header->len > sizeof(struct MTPHeader) ? header->len - sizeof(struct MTPHeader) : 0;
        persistence.buffer_offset = 0;
        persistence.resp_code = MTP_RESP_OK;

        FURI_LOG_I(
            "MTP",
            "Handling MTP data: 0x%04x, transaction %ld, %ld bytes",
            header->op,
            header->transaction_id,
            persistence.left_bytes);

        ptr += sizeof(struct MTPHeader);
        size -= sizeof(struct MTPHeader);

        switch(persistence.op) {
        case MTP_OP_SEND_OBJECT_INFO:
        case MTP_OP_SET_OBJECT_PROP_VALUE:
            persistence.global_buffer = malloc(sizeof(uint8_t) * MTP_BUFFER_SIZE);
            break;
        case MTP_OP_SEND_OBJECT:
            begin_send_object(mtp);
            break;
        default:
            break;
        }
    }

    if(size > persistence.left_bytes) {
        size = persistence.left_bytes;
    }
    persistence.left_bytes -= size;

    switch(persistence.op) {
    case MTP_OP_SEND_OBJECT:
        write_send_object(ptr, size);
        if(persistence.left_bytes == 0) {
            end_send_object(mtp);
        }
        break;
    case MTP_OP_SEND_OBJECT_INFO:
    case MTP_OP_SET_OBJECT_PROP_VALUE:
        if(persistence.buffer_offset + size <= MTP_BUFFER_SIZE) {
            memcpy(persistence.global_buffer + persistence.buffer_offset, ptr, size);
            persistence.buffer_offset += size;
        } else {
            FURI_LOG_E("MTP", "Dataset too large");
            persistence.resp_code = MTP_RESP_GENERAL_ERROR;
        }

        if(persistence.left_bytes == 0) {
            handle_mtp_data_complete(mtp);
        }
        break;
    default:
        // data phase of an unsupported operation: drain it
        if(persistence.left_bytes == 0) {
            FURI_LOG_W("MTP", "Dropped data for operation: 0x%04x", persistence.op);
        }
        break;
    }
}

static void handle_send_object_info(AppMTP* mtp) {
    uint8_t* data = persistence.global_buffer;
    if(persistence.buffer_offset <= MTP_OBJECT_INFO_FIXED_SIZE) {
        send_mtp_response(
            mtp,
            MTP_TYPE_RESPONSE,
            MTP_RESP_NO_VALID_OBJECT_INFO,
            persistence.transaction_id,
            NULL);
        return;
    }

    // format sits before the misaligned part of ObjectInfoHeader, so it is safe to read
    struct ObjectInfoHeader* info = (struct ObjectInfoHeader*)data;
    bool is_dir = info->format == MTP_FORMAT_ASSOCIATION;

    char* name = ReadMTPString(data + MTP_OBJECT_INFO_FIXED_SIZE);

    // if the name is blank, generate random name
    if(*name == 0) {
        free(name);
        name = malloc(sizeof(char) * 16);
        snprintf(name, 16, "file_%d", rand() % 1000);
    }

    FURI_LOG_I("MTP", "Creating object: %s", name);

    uint32_t storage_id = persistence.params[0];
    uint32_t parent = persistence.params[1];

    const char* base_path = get_base_path_from_storage_id(storage_id);
    if(base_path == NULL) {
        FURI_LOG_E("MTP", "Invalid storage ID: %ld", storage_id);
        send_mtp_response(
            mtp, MTP_TYPE_RESPONSE, MTP_RESP_INVALID_STORAGE_ID, persistence.transaction_id, NULL);
        free(name);
        return;
    }

    if(parent != 0xffffffff && parent != 0) {
        base_path = get_path_from_handle(mtp, parent);
        if(base_path == NULL) {
            FURI_LOG_E("MTP", "Invalid parent handle: %ld", parent);
            send_mtp_response(
                mtp,
                MTP_TYPE_RESPONSE,
                MTP_RESP_INVALID_PARENT_OBJECT,
                persistence.transaction_id,
                NULL);
            free(name);
            return;
        }
    }

    char full_path[MTP_PATH_SIZE];
    bool path_ok = merge_path(full_path, sizeof(full_path), base_path, name);
    free(name);

    if(!path_ok) {
        FURI_LOG_E("MTP", "Path too long");
        send_mtp_response(
            mtp, MTP_TYPE_RESPONSE, MTP_RESP_GENERAL_ERROR, persistence.transaction_id, NULL);
        return;
    }

    if(is_dir) {
        if(!storage_dir_exists(mtp->storage, full_path) &&
           !storage_simply_mkdir(mtp->storage, full_path)) {
            FURI_LOG_E("MTP", "Failed to create directory: %s", full_path);
            send_mtp_response(
                mtp, MTP_TYPE_RESPONSE, MTP_RESP_GENERAL_ERROR, persistence.transaction_id, NULL);
            return;
        }
    } else if(!storage_file_exists(mtp->storage, full_path)) {
        // create file, SendObject will fill it in
        File* file = storage_file_alloc(mtp->storage);
        bool created = storage_file_open(file, full_path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
        storage_file_close(file);
        storage_file_free(file);

        if(!created) {
            FURI_LOG_E("MTP", "Failed to create file: %s", full_path);
            send_mtp_response(
                mtp, MTP_TYPE_RESPONSE, MTP_RESP_GENERAL_ERROR, persistence.transaction_id, NULL);
            return;
        }
    }

    uint32_t handle = issue_object_handle(mtp, full_path);
    persistence.prev_handle = is_dir ? 0 : handle;

    uint32_t params[5] = {storage_id, parent, handle, 0, 0};
    send_mtp_response(mtp, MTP_TYPE_RESPONSE, MTP_RESP_OK, persistence.transaction_id, params);
}

static void handle_set_object_prop_value(AppMTP* mtp) {
    uint32_t handle = persistence.params[0];
    uint32_t prop_code = persistence.params[1];

    char* path = get_path_from_handle(mtp, handle);
    if(path == NULL) {
        FURI_LOG_E("MTP", "Invalid handle for MTP_OP_SET_OBJECT_PROP_VALUE");
        send_mtp_response(
            mtp,
            MTP_TYPE_RESPONSE,
            MTP_RESP_INVALID_OBJECT_HANDLE,
            persistence.transaction_id,
            NULL);
        return;
    }

    if(prop_code != MTP_PROP_OBJECT_FILE_NAME) {
        FURI_LOG_W("MTP", "Unsupported property code: 0x%04lx", prop_code);
        send_mtp_response(
            mtp,
            MTP_TYPE_RESPONSE,
            MTP_RESP_INVALID_OBJECT_PROP_CODE,
            persistence.transaction_id,
            NULL);
        return;
    }

    char* name = ReadMTPString(persistence.global_buffer);
    char* slash = strrchr(path, '/');

    uint16_t resp_code = MTP_RESP_OK;
    char new_path[MTP_PATH_SIZE];
    int len = snprintf(
        new_path, sizeof(new_path), "%.*s/%s", slash ? (int)(slash - path) : 0, path, name);

    if(*name == 0 || slash == NULL) {
        resp_code = MTP_RESP_INVALID_OBJECT_PROP_VALUE;
    } else if(len < 0 || (size_t)len >= sizeof(new_path)) {
        resp_code = MTP_RESP_GENERAL_ERROR;
    } else if(strcmp(path, new_path) != 0) {
        FURI_LOG_I("MTP", "Renaming %s to %s", path, new_path);
        if(storage_common_rename(mtp->storage, path, new_path) == FSE_OK) {
            rename_object_handles(mtp, path, new_path);
        } else {
            resp_code = MTP_RESP_GENERAL_ERROR;
        }
    }

    free(name);
    send_mtp_response(mtp, MTP_TYPE_RESPONSE, resp_code, persistence.transaction_id, NULL);
}

void handle_mtp_data_complete(AppMTP* mtp) {
    print_bytes("MTP Data", persistence.global_buffer, persistence.buffer_offset);

    if(persistence.resp_code != MTP_RESP_OK) {
        send_mtp_response(
            mtp, MTP_TYPE_RESPONSE, persistence.resp_code, persistence.transaction_id, NULL);
    } else if(persistence.op == MTP_OP_SEND_OBJECT_INFO) {
        handle_send_object_info(mtp);
    } else if(persistence.op == MTP_OP_SET_OBJECT_PROP_VALUE) {
        handle_set_object_prop_value(mtp);
    }

    free(persistence.global_buffer);
    persistence.global_buffer = NULL;
}

void handle_mtp_response(AppMTP* mtp, struct MTPHeader* header) {
    UNUSED(mtp);
    FURI_LOG_I("MTP", "Handling MTP response: 0x%04x", header->op);
    FURI_LOG_I("MTP", "Transaction ID: %ld", header->transaction_id);
}

void handle_mtp_command(AppMTP* mtp, struct MTPContainer* container) {
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
        //FURI_LOG_I("MTP", "Open/CloseSession operation (STUB)");
        send_mtp_response(mtp, 3, MTP_RESP_OK, container->header.transaction_id, NULL);
        break;
    case MTP_OP_GET_STORAGE_IDS:
        FURI_LOG_I("MTP", "GetStorageIDs operation");
        send_storage_ids(mtp, container->header.transaction_id);
        break;
    case MTP_OP_GET_STORAGE_INFO: {
        FURI_LOG_I("MTP", "GetStorageInfo operation");
        uint8_t* info = malloc(sizeof(uint8_t) * MTP_BUFFER_SIZE);
        int length = GetStorageInfo(mtp, container->params[0], info);
        send_mtp_response_buffer(
            mtp,
            MTP_TYPE_DATA,
            MTP_OP_GET_STORAGE_INFO,
            container->header.transaction_id,
            info,
            length);
        send_mtp_response_buffer(
            mtp, MTP_TYPE_RESPONSE, MTP_RESP_OK, container->header.transaction_id, NULL, 0);
        free(info);
        break;
    }
    case MTP_OP_GET_NUM_OBJECTS:
        FURI_LOG_I("MTP", "GetNumObjects operation");
        if(container->params[1]) {
            send_mtp_response(
                mtp,
                MTP_TYPE_RESPONSE,
                MTP_RESP_SPEC_BY_FORMAT_UNSUPPORTED,
                container->header.transaction_id,
                NULL);
        } else {
            uint32_t params[5] = {
                GetNumObjects(mtp, container->params[0], container->params[2]), 0, 0, 0, 0};
            send_mtp_response(
                mtp, MTP_TYPE_RESPONSE, MTP_RESP_OK, container->header.transaction_id, params);
        }
        break;
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
            uint32_t* handles = NULL;
            int count =
                list_and_issue_handles(mtp, container->params[0], container->params[2], &handles);
            send_mtp_response_buffer(
                mtp,
                MTP_TYPE_DATA,
                MTP_OP_GET_OBJECT_HANDLES,
                container->header.transaction_id,
                (uint8_t*)handles,
                sizeof(uint32_t) * (count + 1));

            send_mtp_response_buffer(
                mtp, MTP_TYPE_RESPONSE, MTP_RESP_OK, container->header.transaction_id, NULL, 0);
            free(handles);
        }
        break;
    case MTP_OP_GET_OBJECT_INFO: {
        FURI_LOG_I("MTP", "GetObjectInfo operation");
        uint8_t* buffer = malloc(sizeof(uint8_t) * MTP_BUFFER_SIZE);
        int length = GetObjectInfo(mtp, container->params[0], buffer);

        if(length < 0) {
            send_mtp_response(
                mtp,
                MTP_TYPE_RESPONSE,
                MTP_RESP_INVALID_OBJECT_HANDLE,
                container->header.transaction_id,
                NULL);
            free(buffer);
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
    case MTP_OP_GET_OBJECT_PROPS_SUPPORTED: {
        FURI_LOG_I("MTP", "GetObjectPropsSupported operation");
        uint8_t* buffer = malloc(sizeof(uint8_t) * MTP_BUFFER_SIZE);
        uint32_t count = sizeof(supported_object_props) / sizeof(uint16_t);
        memcpy(buffer, &count, sizeof(uint32_t));
        memcpy(buffer + sizeof(uint32_t), supported_object_props, sizeof(uint16_t) * count);

        int length = sizeof(uint32_t) + sizeof(uint16_t) * count;
        send_mtp_response_buffer(
            mtp,
            MTP_TYPE_DATA,
            MTP_OP_GET_OBJECT_PROPS_SUPPORTED,
            container->header.transaction_id,
            buffer,
            length);
        free(buffer);
        send_mtp_response_buffer(
            mtp, MTP_TYPE_RESPONSE, MTP_RESP_OK, container->header.transaction_id, NULL, 0);
        break;
    }
    case MTP_OP_DELETE_OBJECT:
        FURI_LOG_I("MTP", "DeleteObject operation");
        if(DeleteObject(mtp, container->params[0])) {
            send_mtp_response(mtp, 3, MTP_RESP_OK, container->header.transaction_id, NULL);
        } else {
            send_mtp_response(
                mtp, 3, MTP_RESP_INVALID_OBJECT_HANDLE, container->header.transaction_id, NULL);
        }
        break;

    case MTP_OP_GET_DEVICE_PROP_VALUE:
        FURI_LOG_I("MTP", "GetDevicePropValue operation");
        GetDevicePropValue(mtp, container->header.transaction_id, container->params[0]);
        // Process the GetDevicePropValue operation
        break;
    case MTP_OP_GET_DEVICE_PROP_DESC:
        FURI_LOG_I("MTP", "GetDevicePropDesc operation");
        GetDevicePropDesc(mtp, container->header.transaction_id, container->params[0]);
        // Process the GetDevicePropDesc operation
        break;
    // Handle bulk transfer specific operations
    case MTP_OP_SEND_OBJECT_INFO:
        FURI_LOG_I("MTP", "SendObjectInfo operation");
        setup_persistence(container);
        break;
    case MTP_OP_SEND_OBJECT:
        FURI_LOG_I("MTP", "SendObject operation");
        setup_persistence(container);
        break;
    case MTP_OP_GET_OBJECT: {
        FURI_LOG_I("MTP", "GetObject operation");
        GetObject(mtp, container->header.transaction_id, container->params[0]);

        break;
    }
    case MTP_OP_MOVE_OBJECT: {
        FURI_LOG_I("MTP", "MoveObject operation");
        // Process the MoveObject operation
        MoveObject(
            mtp,
            container->header.transaction_id,
            container->params[0],
            container->params[1],
            container->params[2]);
        break;
    }
    case MTP_OP_POWER_DOWN: {
        FURI_LOG_I("MTP", "PowerDown operation");

        FURI_LOG_I("MTP", "Good night.");
        send_mtp_response(mtp, 3, MTP_RESP_OK, container->header.transaction_id, NULL);

        furi_hal_power_off();
        break;
    }
    // Handle other bulk operations here...
    case MTP_OP_GET_OBJECT_PROP_VALUE: {
        FURI_LOG_I("MTP", "GetObjectPropValue operation");
        // Process the GetObjectPropValue operation
        GetObjectPropValue(
            mtp, container->header.transaction_id, container->params[0], container->params[1]);
        break;
    }
    case MTP_OP_SET_OBJECT_PROP_VALUE: {
        FURI_LOG_I("MTP", "SetObjectPropValue operation");
        setup_persistence(container);
        break;
    }
    default:
        FURI_LOG_W("MTP", "Unsupported MTP operation in bulk transfer: 0x%04x", mtp_op);
        send_mtp_response(
            mtp, 3, MTP_RESP_OPERATION_NOT_SUPPORTED, container->header.transaction_id, NULL);
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
    uint8_t* response = malloc(sizeof(uint8_t) * MTP_BUFFER_SIZE);
    int length = BuildDeviceInfo(response);
    send_mtp_response_buffer(
        mtp, MTP_TYPE_DATA, MTP_OP_GET_DEVICE_INFO, transaction_id, response, length);
    send_mtp_response_buffer(mtp, MTP_TYPE_RESPONSE, MTP_RESP_OK, transaction_id, NULL, 0);
    free(response);
}

int GetNumObjects(AppMTP* mtp, uint32_t storage_id, uint32_t association) {
    return list_and_issue_handles(mtp, storage_id, association, NULL);
}

/**
 * Sends a container of `length` payload bytes, pulling the payload from `callback`.
 * Returns false if the host stopped accepting data (timeout, cancel, reset).
 * If the callback runs dry early the rest is zero padded to keep the host in sync.
 */
bool send_mtp_response_stream(
    AppMTP* mtp,
    uint16_t resp_type,
    uint16_t resp_code,
    uint32_t transaction_id,
    void* callback_context,
    int (*callback)(void* ctx, uint8_t* buffer, int length),
    uint32_t length) {
    uint32_t packet[MTP_MAX_PACKET_SIZE / sizeof(uint32_t)]; // word aligned for the header
    uint8_t* buffer = (uint8_t*)packet;
    uint32_t total = length + sizeof(struct MTPHeader);
    uint32_t remaining = length;
    bool source_ok = true;
    bool first = true;

    while(first || remaining > 0) {
        uint8_t* ptr = buffer;

        if(first) {
            struct MTPHeader* hdr = (struct MTPHeader*)buffer;
            hdr->len = total;
            hdr->type = resp_type;
            hdr->op = resp_code;
            hdr->transaction_id = transaction_id;

            ptr += sizeof(*hdr);
            first = false;
        }

        uint32_t to_fill = MIN((uint32_t)(buffer + MTP_MAX_PACKET_SIZE - ptr), remaining);
        remaining -= to_fill;

        while(to_fill > 0) {
            int read_bytes = source_ok ? callback(callback_context, ptr, to_fill) : 0;
            if(read_bytes <= 0) {
                if(source_ok) FURI_LOG_E("MTP", "Stream source ended early");
                source_ok = false;
                memset(ptr, 0, to_fill);
                read_bytes = to_fill;
            }

            ptr += read_bytes;
            to_fill -= read_bytes;
        }

        if(!usb_mtp_write(mtp, buffer, ptr - buffer)) {
            FURI_LOG_E("MTP", "Stream aborted, %ld bytes left", remaining);
            return false;
        }
    }

    // a transfer ending on a packet boundary has to be terminated with a ZLP
    if(total % MTP_MAX_PACKET_SIZE == 0) {
        return usb_mtp_write(mtp, NULL, 0);
    }

    return true;
}

int send_mtp_response_buffer_callback(void* ctx, uint8_t* buffer, int size) {
    struct MTPResponseBufferContext* context = (struct MTPResponseBufferContext*)ctx;
    if(context->buffer == NULL || context->size == 0) {
        return 0;
    }

    uint32_t remaining = context->size - context->sent;
    uint32_t to_send = size;
    if(remaining < to_send) {
        to_send = remaining;
    }

    memcpy(buffer, context->buffer + context->sent, to_send);
    context->sent += to_send;

    return to_send;
}

void send_mtp_response_buffer(
    AppMTP* mtp,
    uint16_t resp_type,
    uint16_t resp_code,
    uint32_t transaction_id,
    uint8_t* buffer,
    uint32_t size) {
    struct MTPResponseBufferContext ctx = {
        .buffer = buffer,
        .size = size,
        .sent = 0,
    };

    send_mtp_response_stream(
        mtp, resp_type, resp_code, transaction_id, &ctx, send_mtp_response_buffer_callback, size);
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
    case MTP_REQ_RESET:
        // Runs in the USB IRQ: let the worker drop the transfer in progress
        mtp->state = MTPStateReady;
        if(mtp->worker_thread != NULL) {
            furi_thread_flags_set(furi_thread_get_id(mtp->worker_thread), EventCancel);
        }
        value = 0;
        break;
    case MTP_REQ_GET_EXT_EVENT_DATA:
        // Handle GetExtEventData request
        value = 0;
        break;
    case MTP_REQ_GET_DEVICE_STATUS: {
        // Handle GetDeviceStatus request
        struct mtp_device_status* status = (struct mtp_device_status*)req->data;
        status->wLength = sizeof(*status);
        status->wCode = (mtp->state == MTPStateBusy) ? MTP_RESP_DEVICE_BUSY : MTP_RESP_OK;
        value = status->wLength;
        break;
    }
    default:
        // Unsupported request
        break;
    }

    return value;
}
