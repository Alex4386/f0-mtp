#include "utils.h"

const char hex[] = "0123456789ABCDEF";

char byte_to_hex(uint8_t byte) {
    return hex[byte];
}

void print_bytes(char* tag, uint8_t* bytes, size_t len) {
    if(furi_log_get_level() < FuriLogLevelDebug) return;

    FURI_LOG_D("MTP", "Dumping bytes - TAG: %s", tag);
    int lines = len > 0 ? (len / 16) + 1 : 0;
    size_t last_line_len = len % 16;
    char* line = (char*)malloc(16 * 3 + 3 + 16 + 1);
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
        FURI_LOG_D("MTP", "%s", line);
    }
    free(line);
    FURI_LOG_D("MTP", "End of dump - TAG: %s", tag);
}

bool merge_path(char* target, size_t size, const char* base, const char* name) {
    int length = snprintf(target, size, "%s/%s", base, name);
    return length >= 0 && (size_t)length < size;
}

char* ReadMTPString(const uint8_t* buffer) {
    // length byte counts UTF-16 code units, including the terminator
    uint8_t len16 = buffer[0];
    const uint8_t* ptr = buffer + 1;

    char* str = malloc(sizeof(char) * (len16 + 1));
    int i = 0;
    for(; i < len16; i++) {
        uint16_t c = ptr[i * 2] | (ptr[i * 2 + 1] << 8);
        if(c == 0) break;

        // Flipper's filesystem can't take non-ASCII names
        str[i] = c < 0x80 ? (char)c : '_';
    }
    str[i] = '\0';

    return str;
}

void WriteMTPString(uint8_t* buffer, const char* str, uint16_t* length) {
    uint8_t* ptr = buffer;
    size_t str_len = strlen(str);

    // extra handling for empty string
    if(str_len == 0) {
        *ptr = 0x00;

        // that's it!
        *length = 1;
        return;
    }

    // the length byte can describe at most 255 units, terminator included
    if(str_len > 254) str_len = 254;

    *ptr = str_len + 1; // Length byte (number of characters including the null terminator)
    ptr++;
    for(size_t i = 0; i < str_len; i++) {
        *ptr++ = str[i];
        *ptr++ = 0x00; // UTF-16LE encoding (add null byte for each character)
    }
    *ptr++ = 0x00; // Null terminator (UTF-16LE)
    *ptr++ = 0x00;

    *length = ptr - buffer;
}
