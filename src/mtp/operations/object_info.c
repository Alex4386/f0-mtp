#include "object_info.h"
#include "../core/mtp_string.h"
#include <string.h>

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

static bool put_mtp_string(uint8_t** p, uint8_t* end, const char* str) {
    if(!str) str = "";
    size_t worst = 1 + (strlen(str) + 1) * 2;
    if((size_t)(end - *p) < worst) return false;
    size_t n = mtp_string_write(*p, str);
    if(n == 0 && strlen(str) > 0) return false;
    *p += n;
    return true;
}

size_t mtp_build_object_info(const MTPObjectInfoInput* in, uint8_t* buffer, size_t buffer_size) {
    if(!in || !buffer || buffer_size == 0) return 0;

    uint8_t* ptr = buffer;
    uint8_t* end = buffer + buffer_size;

    // Fixed part (ObjectInfoHeader)
    if(!put_u32(&ptr, end, in->storage_id)) return 0;
    if(!put_u16(&ptr, end, in->object_format)) return 0;
    if(!put_u16(&ptr, end, in->protection_status)) return 0;
    if(!put_u32(&ptr, end, in->object_compressed_size)) return 0;

    // Thumb format
    if(!put_u16(&ptr, end, 0)) return 0;
    if(!put_u32(&ptr, end, 0)) return 0; // thumb compressed size
    if(!put_u32(&ptr, end, 0)) return 0; // thumb pix width
    if(!put_u32(&ptr, end, 0)) return 0; // thumb pix height
    if(!put_u32(&ptr, end, 0)) return 0; // image pix width
    if(!put_u32(&ptr, end, 0)) return 0; // image pix height
    if(!put_u32(&ptr, end, 0)) return 0; // image bit depth

    if(!put_u32(&ptr, end, in->parent_object)) return 0;
    if(!put_u16(&ptr, end, in->association_type)) return 0;
    if(!put_u32(&ptr, end, in->association_desc)) return 0;

    // sequence_number - unused
    if(!put_u32(&ptr, end, 0)) return 0;

    // Variable part: filename, date_created, date_modified, keywords
    if(!put_mtp_string(&ptr, end, in->filename)) return 0;
    if(!put_mtp_string(&ptr, end, in->date_created)) return 0;
    if(!put_mtp_string(&ptr, end, in->date_modified)) return 0;
    if(!put_mtp_string(&ptr, end, in->keywords)) return 0;

    return (size_t)(ptr - buffer);
}
