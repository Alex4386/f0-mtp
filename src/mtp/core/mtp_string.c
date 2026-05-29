#include "mtp_string.h"
#include <stdlib.h>
#include <string.h>

MTPString* mtp_string_create(const char* utf8_str) {
    if(!utf8_str) {
        return NULL;
    }

    MTPString* str = malloc(sizeof(MTPString));
    if(!str) {
        return NULL;
    }

    size_t utf8_len = strlen(utf8_str);

    // For ASCII strings, length is same
    // For full Unicode, would need proper UTF-8 to UTF-16 conversion
    // Current implementation assumes ASCII (like original code)
    str->length = utf8_len + 1; // +1 for null terminator

    str->data = malloc(sizeof(uint16_t) * str->length);
    if(!str->data) {
        free(str);
        return NULL;
    }

    // Convert ASCII to UTF-16LE (simple expansion)
    for(size_t i = 0; i < utf8_len; i++) {
        str->data[i] = (uint16_t)(uint8_t)utf8_str[i];
    }
    str->data[utf8_len] = 0; // Null terminator

    return str;
}

void mtp_string_destroy(MTPString* str) {
    if(!str) {
        return;
    }

    free(str->data);
    free(str);
}

char* mtp_string_decode(const uint8_t* buffer, size_t buffer_size) {
    if(!buffer || buffer_size == 0) {
        return NULL;
    }

    // Read length byte
    uint8_t length = buffer[0];

    if(length == 0) {
        // Empty string
        char* empty = malloc(1);
        if(empty) {
            empty[0] = '\0';
        }
        return empty;
    }

    // Check buffer has enough data
    size_t required_size = 1 + (length * 2); // length byte + UTF-16LE chars
    if(buffer_size < required_size) {
        return NULL;
    }

    // Allocate result buffer
    char* result = malloc(length); // length includes null terminator
    if(!result) {
        return NULL;
    }

    // Decode UTF-16LE to ASCII/UTF-8
    const uint16_t* utf16_data = (const uint16_t*)(buffer + 1);
    for(size_t i = 0; i < length - 1; i++) {
        uint16_t ch = utf16_data[i];

        // Simple conversion (assumes ASCII or low Unicode)
        // Full Unicode would need proper UTF-16 to UTF-8 conversion
        if(ch > 0xFF) {
            // Non-ASCII character - use replacement character
            result[i] = '?';
        } else {
            result[i] = (char)ch;
        }
    }
    result[length - 1] = '\0';

    return result;
}

bool mtp_string_has_unicode(const uint8_t* buffer) {
    if(!buffer) {
        return false;
    }

    uint8_t length = buffer[0];
    if(length == 0) {
        return false;
    }

    const uint16_t* utf16_data = (const uint16_t*)(buffer + 1);
    for(size_t i = 0; i < length; i++) {
        if(utf16_data[i] > 0x7F) {
            return true;
        }
    }

    return false;
}

size_t mtp_string_write(uint8_t* buffer, const char* utf8_str) {
    if(!buffer || !utf8_str) {
        return 0;
    }

    uint8_t* ptr = buffer;
    size_t str_len = strlen(utf8_str);

    // Handle empty string
    if(str_len == 0) {
        *ptr = 0x00;
        return 1;
    }

    // Write length byte (including null terminator)
    *ptr = (uint8_t)(str_len + 1);
    ptr++;

    // Write characters in UTF-16LE (ASCII expansion)
    for(size_t i = 0; i < str_len; i++) {
        *ptr++ = (uint8_t)utf8_str[i]; // Low byte
        *ptr++ = 0x00; // High byte (0 for ASCII)
    }

    // Write null terminator
    *ptr++ = 0x00;
    *ptr++ = 0x00;

    return ptr - buffer;
}

uint8_t mtp_string_read_length(const uint8_t* buffer) {
    return buffer ? buffer[0] : 0;
}

bool mtp_string_is_valid_utf8(const char* str) {
    if(!str) {
        return false;
    }

    // Simple ASCII validation
    // Full UTF-8 validation would check multi-byte sequences
    while(*str) {
        if((unsigned char)*str > 0x7F) {
            // For now, reject non-ASCII
            // Could implement full UTF-8 validation later
            return false;
        }
        str++;
    }

    return true;
}
