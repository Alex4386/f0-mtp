#include "storage_info.h"
#include "../core/mtp_string.h"
#include <string.h>

static bool put_u16(uint8_t** ptr, uint8_t* end, uint16_t value) {
    if(end - *ptr < 2) return false;
    (*ptr)[0] = (uint8_t)(value & 0xFF);
    (*ptr)[1] = (uint8_t)((value >> 8) & 0xFF);
    *ptr += 2;
    return true;
}

static bool put_u32(uint8_t** ptr, uint8_t* end, uint32_t value) {
    if(end - *ptr < 4) return false;
    (*ptr)[0] = (uint8_t)(value & 0xFF);
    (*ptr)[1] = (uint8_t)((value >> 8) & 0xFF);
    (*ptr)[2] = (uint8_t)((value >> 16) & 0xFF);
    (*ptr)[3] = (uint8_t)((value >> 24) & 0xFF);
    *ptr += 4;
    return true;
}

static bool put_u64(uint8_t** ptr, uint8_t* end, uint64_t value) {
    if(end - *ptr < 8) return false;
    for(int i = 0; i < 8; i++) {
        (*ptr)[i] = (uint8_t)((value >> (i * 8)) & 0xFF);
    }
    *ptr += 8;
    return true;
}

static bool put_mtp_string(uint8_t** ptr, uint8_t* end, const char* str) {
    if(!str) str = "";
    size_t worst = 1 + (strlen(str) + 1) * 2;
    if((size_t)(end - *ptr) < worst) return false;
    size_t n = mtp_string_write(*ptr, str);
    if(n == 0 && strlen(str) > 0) return false;
    *ptr += n;
    return true;
}

size_t mtp_build_storage_info(const MTPStorageInfoInput* in, uint8_t* buffer, size_t buffer_size) {
    if(!in || !buffer || buffer_size == 0) return 0;

    uint8_t* ptr = buffer;
    uint8_t* end = buffer + buffer_size;

    if(!put_u16(&ptr, end, in->storage_type)) return 0;
    if(!put_u16(&ptr, end, in->filesystem_type)) return 0;
    if(!put_u16(&ptr, end, in->access_capability)) return 0;

    if(!put_u64(&ptr, end, in->max_capacity_blocks)) return 0;
    if(!put_u64(&ptr, end, in->free_space_blocks)) return 0;
    if(!put_u32(&ptr, end, in->free_space_in_objects)) return 0;

    if(!put_mtp_string(&ptr, end, in->storage_description)) return 0;
    if(!put_mtp_string(&ptr, end, in->volume_identifier)) return 0;

    return (size_t)(ptr - buffer);
}
