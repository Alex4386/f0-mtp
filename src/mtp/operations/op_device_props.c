#include "op_device_props.h"
#include "../core/context.h"
#include "../core/mtp_string.h"

#include <stdlib.h>
#include <string.h>

void mtp_device_props_install(MTPContext* ctx, const MTPDevicePropsProvider* provider) {
    if(!ctx) return;
    ctx->user_data = (void*)provider;
}

static const MTPDevicePropsProvider* get_provider(MTPContext* ctx) {
    return ctx ? (const MTPDevicePropsProvider*)ctx->user_data : NULL;
}

// --- value writers --------------------------------------------------------

static size_t write_value_bytes(MTPContext* ctx, uint32_t prop_code, uint8_t* buf, size_t cap) {
    const MTPDevicePropsProvider* p = get_provider(ctx);

    switch(prop_code) {
    case MTP_DEVICE_PROP_DEVICE_FRIENDLY_NAME: {
        const char* name = (p && p->get_device_name) ? p->get_device_name() : NULL;
        if(!name)
            name = ctx->device_info.device_name ? ctx->device_info.device_name : "Flipper Zero";
        if(cap < 1 + (strlen(name) + 1) * 2) return 0;
        return mtp_string_write(buf, name);
    }
    case MTP_DEVICE_PROP_BATTERY_LEVEL: {
        if(cap < 1) return 0;
        uint8_t level = (p && p->get_battery_level) ? p->get_battery_level() : 0;
        buf[0] = level;
        return 1;
    }
    default:
        return 0;
    }
}

// --- GetDevicePropValue ---------------------------------------------------

MTP_OPERATION_HANDLER(get_device_prop_value) {
    uint32_t prop_code = request->params[0];

    uint8_t* buf = malloc(MTP_BUFFER_SIZE);
    if(!buf) {
        mtp_response_set_code(response, MTP_RESP_GENERAL_ERROR);
        return;
    }

    size_t n = write_value_bytes(ctx, prop_code, buf, MTP_BUFFER_SIZE);
    if(n == 0) {
        free(buf);
        mtp_response_set_code(response, MTP_RESP_DEVICE_PROP_NOT_SUPPORTED);
        return;
    }

    mtp_response_set_data(response, buf, n);
    mtp_response_set_code(response, MTP_RESP_OK);
}

// --- GetDevicePropDesc ----------------------------------------------------

MTP_OPERATION_HANDLER(get_device_prop_desc) {
    uint32_t prop_code = request->params[0];

    uint8_t* buf = malloc(MTP_BUFFER_SIZE);
    if(!buf) {
        mtp_response_set_code(response, MTP_RESP_GENERAL_ERROR);
        return;
    }

    uint8_t* ptr = buf;
    uint8_t* end = buf + MTP_BUFFER_SIZE;

    // prop code u16
    if(end - ptr < 2) goto fail;
    ptr[0] = (uint8_t)(prop_code & 0xFF);
    ptr[1] = (uint8_t)((prop_code >> 8) & 0xFF);
    ptr += 2;

    // data type u16
    uint16_t data_type;
    switch(prop_code) {
    case MTP_DEVICE_PROP_DEVICE_FRIENDLY_NAME:
        data_type = 0xffff; // STRING
        break;
    case MTP_DEVICE_PROP_BATTERY_LEVEL:
        data_type = 0x0002; // UINT8
        break;
    default:
        free(buf);
        mtp_response_set_code(response, MTP_RESP_DEVICE_PROP_NOT_SUPPORTED);
        return;
    }
    if(end - ptr < 2) goto fail;
    ptr[0] = (uint8_t)(data_type & 0xFF);
    ptr[1] = (uint8_t)((data_type >> 8) & 0xFF);
    ptr += 2;

    // get-only
    if(end - ptr < 1) goto fail;
    *ptr++ = 0x00;

    // factory default (= current value)
    size_t n1 = write_value_bytes(ctx, prop_code, ptr, (size_t)(end - ptr));
    if(n1 == 0) goto fail;
    ptr += n1;

    // current value
    size_t n2 = write_value_bytes(ctx, prop_code, ptr, (size_t)(end - ptr));
    if(n2 == 0) goto fail;
    ptr += n2;

    // no-form
    if(end - ptr < 1) goto fail;
    *ptr++ = 0x00;

    mtp_response_set_data(response, buf, (size_t)(ptr - buf));
    mtp_response_set_code(response, MTP_RESP_OK);
    return;

fail:
    free(buf);
    mtp_response_set_code(response, MTP_RESP_GENERAL_ERROR);
}
