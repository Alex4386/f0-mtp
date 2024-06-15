#include "utils.h"

const char hex[] = "0123456789ABCDEF";

void print_bytes(char* tag, uint8_t* bytes, size_t len) {
    FURI_LOG_I("MTP", "Dumping bytes - TAG: %s", tag);
    int lines = len / 16;
    size_t last_line_len = len % 16;
    char* line = (char*)malloc(16 * 3 + 1);
    for(int i = 0; i < lines; i++) {
        int line_len = i == lines - 1 ? last_line_len : 16;
        for(int j = 0; j < line_len; j++) {
            // write hex without sprintf
            line[j * 3] = hex[bytes[i * 16 + j] >> 4];
            line[j * 3 + 1] = hex[bytes[i * 16 + j] & 0x0F];
            line[j * 3 + 2] = ' ';
        }
        line[16 * 3] = '\0';
        FURI_LOG_I("MTP", line);
    }
    free(line);
    FURI_LOG_I("MTP", "End of dump - TAG: %s", tag);
}
