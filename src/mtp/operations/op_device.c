#include "op_device.h"
#include "device_info.h"
#include "../core/context.h"
#include <stdlib.h>
#include <string.h>

const uint16_t MTP_SUPPORTED_OPERATIONS[] = {
    MTP_OP_GET_DEVICE_INFO,
    MTP_OP_OPEN_SESSION,
    MTP_OP_CLOSE_SESSION,
    MTP_OP_GET_STORAGE_IDS,
    MTP_OP_GET_STORAGE_INFO,
    MTP_OP_GET_NUM_OBJECTS,
    MTP_OP_GET_OBJECT_HANDLES,
    MTP_OP_GET_OBJECT_INFO,
    MTP_OP_GET_OBJECT,
    MTP_OP_DELETE_OBJECT,
    MTP_OP_SEND_OBJECT_INFO,
    MTP_OP_SEND_OBJECT,
    MTP_OP_GET_DEVICE_PROP_DESC,
    MTP_OP_GET_DEVICE_PROP_VALUE,
    MTP_OP_MOVE_OBJECT,
    MTP_OP_GET_OBJECT_PROPS_SUPPORTED,
    MTP_OP_GET_OBJECT_PROP_VALUE,
    MTP_OP_SET_OBJECT_PROP_VALUE,
};
const size_t MTP_SUPPORTED_OPERATIONS_COUNT =
    sizeof(MTP_SUPPORTED_OPERATIONS) / sizeof(uint16_t);

const uint16_t MTP_SUPPORTED_EVENTS[] = {0};
const size_t MTP_SUPPORTED_EVENTS_COUNT = 0;

const uint16_t MTP_SUPPORTED_DEVICE_PROPS[] = {
    MTP_DEVICE_PROP_DEVICE_FRIENDLY_NAME,
    MTP_DEVICE_PROP_BATTERY_LEVEL,
};
const size_t MTP_SUPPORTED_DEVICE_PROPS_COUNT =
    sizeof(MTP_SUPPORTED_DEVICE_PROPS) / sizeof(uint16_t);

const uint16_t MTP_SUPPORTED_CAPTURE_FORMATS[] = {0};
const size_t MTP_SUPPORTED_CAPTURE_FORMATS_COUNT = 0;

const uint16_t MTP_SUPPORTED_PLAYBACK_FORMATS[] = {
    MTP_FORMAT_UNDEFINED,
    MTP_FORMAT_ASSOCIATION,
};
const size_t MTP_SUPPORTED_PLAYBACK_FORMATS_COUNT =
    sizeof(MTP_SUPPORTED_PLAYBACK_FORMATS) / sizeof(uint16_t);

const uint16_t MTP_SUPPORTED_OBJECT_PROPS[] = {
    MTP_PROP_STORAGE_ID,
    MTP_PROP_OBJECT_FORMAT,
    MTP_PROP_OBJECT_FILE_NAME,
};
const size_t MTP_SUPPORTED_OBJECT_PROPS_COUNT =
    sizeof(MTP_SUPPORTED_OBJECT_PROPS) / sizeof(uint16_t);

MTP_OPERATION_HANDLER(get_device_info) {
    (void)request;

    MTPDeviceInfoInput in = {
        .manufacturer = ctx->device_info.manufacturer ? ctx->device_info.manufacturer :
                                                        "Flipper Devices Inc.",
        .model = ctx->device_info.model ? ctx->device_info.model : "Flipper Zero",
        .device_version = ctx->device_info.firmware_version ? ctx->device_info.firmware_version :
                                                              "1.0.0",
        .serial = ctx->device_info.serial ? ctx->device_info.serial : "0000",

        .supported_operations = MTP_SUPPORTED_OPERATIONS,
        .supported_operations_count = MTP_SUPPORTED_OPERATIONS_COUNT,

        .supported_events = MTP_SUPPORTED_EVENTS,
        .supported_events_count = MTP_SUPPORTED_EVENTS_COUNT,

        .supported_device_props = MTP_SUPPORTED_DEVICE_PROPS,
        .supported_device_props_count = MTP_SUPPORTED_DEVICE_PROPS_COUNT,

        .supported_capture_formats = MTP_SUPPORTED_CAPTURE_FORMATS,
        .supported_capture_formats_count = MTP_SUPPORTED_CAPTURE_FORMATS_COUNT,

        .supported_playback_formats = MTP_SUPPORTED_PLAYBACK_FORMATS,
        .supported_playback_formats_count = MTP_SUPPORTED_PLAYBACK_FORMATS_COUNT,
    };

    uint8_t* buf = malloc(MTP_BUFFER_SIZE);
    if(!buf) {
        mtp_response_set_code(response, MTP_RESP_GENERAL_ERROR);
        return;
    }

    size_t n = mtp_build_device_info(&in, buf, MTP_BUFFER_SIZE);
    if(n == 0) {
        free(buf);
        mtp_response_set_code(response, MTP_RESP_GENERAL_ERROR);
        return;
    }

    mtp_response_set_data(response, buf, n);
    mtp_response_set_code(response, MTP_RESP_OK);
}

MTP_OPERATION_HANDLER(open_session) {
    uint32_t session_id = request->params[0];
    if(session_id == 0) {
        mtp_response_set_code(response, MTP_RESP_INVALID_TRANSACTION_ID);
        return;
    }

    mtp_context_open_session(ctx, session_id);
    mtp_response_set_code(response, MTP_RESP_OK);
}

MTP_OPERATION_HANDLER(close_session) {
    (void)request;
    mtp_context_close_session(ctx);

    // Clear object handles so the next session starts fresh.
    if(ctx->object_index) {
        mtp_object_index_clear(ctx->object_index);
    }

    mtp_response_set_code(response, MTP_RESP_OK);
}

MTP_OPERATION_HANDLER(get_object_props_supported) {
    (void)ctx;
    (void)request;

    // payload = [count: u32][props: u16 ...]
    size_t payload_size = 4 + 2 * MTP_SUPPORTED_OBJECT_PROPS_COUNT;
    uint8_t* buf = malloc(payload_size);
    if(!buf) {
        mtp_response_set_code(response, MTP_RESP_GENERAL_ERROR);
        return;
    }

    uint32_t count = (uint32_t)MTP_SUPPORTED_OBJECT_PROPS_COUNT;
    buf[0] = (uint8_t)(count & 0xFF);
    buf[1] = (uint8_t)((count >> 8) & 0xFF);
    buf[2] = (uint8_t)((count >> 16) & 0xFF);
    buf[3] = (uint8_t)((count >> 24) & 0xFF);
    for(size_t i = 0; i < MTP_SUPPORTED_OBJECT_PROPS_COUNT; i++) {
        uint16_t p = MTP_SUPPORTED_OBJECT_PROPS[i];
        buf[4 + i * 2 + 0] = (uint8_t)(p & 0xFF);
        buf[4 + i * 2 + 1] = (uint8_t)((p >> 8) & 0xFF);
    }

    mtp_response_set_data(response, buf, payload_size);
    mtp_response_set_code(response, MTP_RESP_OK);
}
