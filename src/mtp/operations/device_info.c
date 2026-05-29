#include "device_info.h"
#include "../core/mtp_string.h"
#include <string.h>

// Small helpers --------------------------------------------------------------

static bool write_u16(uint8_t** ptr, uint8_t* end, uint16_t value) {
    if(end - *ptr < 2) return false;
    (*ptr)[0] = (uint8_t)(value & 0xFF);
    (*ptr)[1] = (uint8_t)((value >> 8) & 0xFF);
    *ptr += 2;
    return true;
}

static bool write_u32(uint8_t** ptr, uint8_t* end, uint32_t value) {
    if(end - *ptr < 4) return false;
    (*ptr)[0] = (uint8_t)(value & 0xFF);
    (*ptr)[1] = (uint8_t)((value >> 8) & 0xFF);
    (*ptr)[2] = (uint8_t)((value >> 16) & 0xFF);
    (*ptr)[3] = (uint8_t)((value >> 24) & 0xFF);
    *ptr += 4;
    return true;
}

static bool write_u16_array(uint8_t** ptr, uint8_t* end, const uint16_t* values, size_t count) {
    if(!write_u32(ptr, end, (uint32_t)count)) return false;
    for(size_t i = 0; i < count; i++) {
        if(!write_u16(ptr, end, values[i])) return false;
    }
    return true;
}

static bool write_mtp_string(uint8_t** ptr, uint8_t* end, const char* str) {
    if(!str) str = "";
    // Worst case: 1 length byte + (strlen+1) * 2 bytes UTF-16LE
    size_t worst = 1 + (strlen(str) + 1) * 2;
    if((size_t)(end - *ptr) < worst) return false;
    size_t n = mtp_string_write(*ptr, str);
    if(n == 0 && strlen(str) > 0) return false;
    *ptr += n;
    return true;
}

// --------------------------------------------------------------------------

size_t mtp_build_device_info(const MTPDeviceInfoInput* in, uint8_t* buffer, size_t buffer_size) {
    if(!in || !buffer || buffer_size == 0) return 0;

    uint8_t* ptr = buffer;
    uint8_t* end = buffer + buffer_size;

    // Standard version
    if(!write_u16(&ptr, end, MTP_STANDARD_VERSION)) return 0;

    // Vendor extension ID
    if(!write_u32(&ptr, end, MTP_VENDOR_EXTENSION_ID)) return 0;

    // Vendor extension version
    if(!write_u16(&ptr, end, MTP_VENDOR_EXTENSION_VERSION)) return 0;

    // Vendor extension description (MTP-string)
    if(!write_mtp_string(&ptr, end, "microsoft.com: 1.0;")) return 0;

    // Functional mode
    if(!write_u16(&ptr, end, MTP_FUNCTIONAL_MODE)) return 0;

    // Operations supported (array of uint16)
    if(!write_u16_array(&ptr, end, in->supported_operations, in->supported_operations_count))
        return 0;

    // Events supported
    if(!write_u16_array(&ptr, end, in->supported_events, in->supported_events_count)) return 0;

    // Device properties supported
    if(!write_u16_array(&ptr, end, in->supported_device_props, in->supported_device_props_count))
        return 0;

    // Capture formats supported
    if(!write_u16_array(
           &ptr, end, in->supported_capture_formats, in->supported_capture_formats_count))
        return 0;

    // Playback formats supported
    if(!write_u16_array(
           &ptr, end, in->supported_playback_formats, in->supported_playback_formats_count))
        return 0;

    // Strings: manufacturer, model, device version, serial
    if(!write_mtp_string(&ptr, end, in->manufacturer)) return 0;
    if(!write_mtp_string(&ptr, end, in->model)) return 0;
    if(!write_mtp_string(&ptr, end, in->device_version)) return 0;
    if(!write_mtp_string(&ptr, end, in->serial)) return 0;

    return (size_t)(ptr - buffer);
}
