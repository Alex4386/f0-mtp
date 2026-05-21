#include "object_props.h"
#include "../core/mtp_string.h"
#include <string.h>

static bool put_u8(uint8_t** p, uint8_t* end, uint8_t v) {
    if(end - *p < 1) return false;
    **p = v;
    *p += 1;
    return true;
}

static bool put_u16(uint8_t** p, uint8_t* end, uint16_t v) {
    if(end - *p < 2) return false;
    (*p)[0] = (uint8_t)(v & 0xFF);
    (*p)[1] = (uint8_t)((v >> 8) & 0xFF);
    *p += 2;
    return true;
}

static bool put_u32(uint8_t** p, uint8_t* end, uint32_t v) {
    if(end - *p < 4) return false;
    (*p)[0] = (uint8_t)(v & 0xFF);
    (*p)[1] = (uint8_t)((v >> 8) & 0xFF);
    (*p)[2] = (uint8_t)((v >> 16) & 0xFF);
    (*p)[3] = (uint8_t)((v >> 24) & 0xFF);
    *p += 4;
    return true;
}

size_t mtp_build_object_prop_value(
    uint16_t prop_code,
    uint32_t storage_id,
    const char* filename,
    uint8_t* buffer,
    size_t buffer_size) {
    if(!buffer || buffer_size == 0) return 0;

    uint8_t* ptr = buffer;
    uint8_t* end = buffer + buffer_size;

    switch(prop_code) {
    case MTP_PROP_STORAGE_ID:
        if(!put_u32(&ptr, end, prop_code)) return 0;
        if(!put_u32(&ptr, end, 0x0006)) return 0; // data type: UINT32
        if(!put_u8(&ptr, end, 0x00)) return 0;
        if(!put_u32(&ptr, end, storage_id)) return 0;
        if(!put_u8(&ptr, end, 0x00)) return 0;
        return (size_t)(ptr - buffer);

    case MTP_PROP_OBJECT_FORMAT:
        if(!put_u32(&ptr, end, prop_code)) return 0;
        if(!put_u32(&ptr, end, 0x0006)) return 0; // data type: UINT32 (kept for parity with src.old wire output)
        if(!put_u8(&ptr, end, 0x00)) return 0;
        if(!put_u16(&ptr, end, MTP_FORMAT_UNDEFINED)) return 0;
        if(!put_u8(&ptr, end, 0x00)) return 0;
        return (size_t)(ptr - buffer);

    case MTP_PROP_OBJECT_FILE_NAME: {
        if(!filename) filename = "";

        if(!put_u32(&ptr, end, prop_code)) return 0;
        if(!put_u32(&ptr, end, 0xffff)) return 0; // data type: STRING
        if(!put_u8(&ptr, end, 0x01)) return 0;

        size_t worst = 1 + (strlen(filename) + 1) * 2;
        if((size_t)(end - ptr) < worst + 1) return 0;
        size_t n = mtp_string_write(ptr, filename);
        if(n == 0 && strlen(filename) > 0) return 0;
        ptr += n;

        if(!put_u8(&ptr, end, 0x00)) return 0;
        return (size_t)(ptr - buffer);
    }

    default:
        return 0;
    }
}
