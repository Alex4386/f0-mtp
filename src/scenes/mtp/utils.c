#include "utils.h"

const char hex[] = "0123456789ABCDEF";

char byte_to_hex(uint8_t byte) {
    return hex[byte];
}

void print_bytes(char* tag, uint8_t* bytes, size_t len) {
    FURI_LOG_I("MTP", "Dumping bytes - TAG: %s", tag);
    int lines = len > 0 ? (len / 16) + 1 : 0;
    size_t last_line_len = len % 16;
    char line[16 * 3 + 3 + 16 + 1]; // Stack allocation: 52 bytes
    for(int i = 0; i < lines; i++) {
        int line_len = i == lines - 1 ? last_line_len : 16;
        for(int j = 0; j < 16; j++) {
            if(j >= line_len) {
                line[j * 3] = ' ';
                line[j * 3 + 1] = ' ';
                line[j * 3 + 2] = ' ';
                continue;
            }

            // write hex without sprintf
            line[j * 3] = hex[bytes[i * 16 + j] >> 4];
            line[j * 3 + 1] = hex[bytes[i * 16 + j] & 0x0F];
            line[j * 3 + 2] = ' ';
        }
        line[16 * 3] = ' ';
        line[16 * 3 + 1] = '|';
        line[16 * 3 + 2] = ' ';
        for(int j = 0; j < line_len; j++) {
            // write ascii without sprintf
            line[16 * 3 + 3 + j] =
                bytes[i * 16 + j] >= 32 && bytes[i * 16 + j] <= 126 ? bytes[i * 16 + j] : '.';
        }
        line[16 * 3 + 3 + line_len] = '\0';
        FURI_LOG_I("MTP", line);
    }
    FURI_LOG_I("MTP", "End of dump - TAG: %s", tag);
}

void merge_path(char* target, char* base, char* name) {
    char* ptr = target;
    strcpy(target, base);
    ptr += strlen(base);
    strcpy(ptr, "/");
    ptr++;
    strcpy(ptr, name);
}

bool CheckMTPStringHasUnicode(uint8_t* buffer) {
    uint8_t* base = buffer;
    int len = *base;

    uint16_t* ptr = (uint16_t*)(base + 1);

    for(int i = 0; i < len; i++) {
        if(ptr[i] > 0x7F) {
            return true;
        }
    }

    return false;
}

void ReadMTPStringToBuffer(uint8_t* buffer, char* output, size_t output_size) {
    int len16 = *(uint8_t*)buffer;
    if(len16 == 0) {
        output[0] = '\0';
        return;
    }

    uint8_t* base = buffer + 1;
    uint16_t* ptr = (uint16_t*)base;

    // Ensure we don't overflow the output buffer
    int copy_len = len16 < output_size - 1 ? len16 : output_size - 1;

    for(int i = 0; i < copy_len; i++) {
        output[i] = (char)(*ptr++);
    }
    output[copy_len] = '\0';
}

const char* ReadMTPString(uint8_t* buffer) {
    static char static_buffer[256]; // Static buffer for simple cases
    ReadMTPStringToBuffer(buffer, static_buffer, sizeof(static_buffer));
    return static_buffer;
}

void WriteMTPString(uint8_t* buffer, const char* str, uint16_t* length) {
    uint8_t* ptr = buffer;

    // Handle NULL string
    if(str == NULL) {
        *ptr = 0x00;
        *length = 1;
        return;
    }

    uint8_t str_len = strlen(str);

    FURI_LOG_I("MTP", "Writing MTP string: %s", str);
    FURI_LOG_I("MTP", "String length: %d", str_len);

    // extra handling for empty string
    if(str_len == 0) {
        *ptr = 0x00;

        // that's it!
        *length = 1;
        return;
    }

    *ptr = str_len + 1; // Length byte (number of characters including the null terminator)
    ptr++;
    while(*str) {
        *ptr++ = *str++;
        *ptr++ = 0x00; // UTF-16LE encoding (add null byte for each character)
    }
    *ptr++ = 0x00; // Null terminator (UTF-16LE)
    *ptr++ = 0x00;

    FURI_LOG_I("MTP", "String byte length: %d", ptr - buffer);
    *length = ptr - buffer;
}
