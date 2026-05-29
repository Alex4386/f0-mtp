#pragma once

#include "mtp_types.h"

// MTP String (UTF-16LE encoded)
typedef struct {
    uint16_t length; // Number of characters (including null terminator)
    uint16_t* data; // UTF-16LE character data
} MTPString;

// Create MTP string from UTF-8/ASCII string
// Returns NULL on allocation failure
MTPString* mtp_string_create(const char* utf8_str);

// Destroy MTP string
void mtp_string_destroy(MTPString* str);

// Decode MTP string from buffer to UTF-8
// Returns newly allocated string (caller must free)
// Returns NULL on error or if buffer is invalid
char* mtp_string_decode(const uint8_t* buffer, size_t buffer_size);

// Check if buffer contains non-ASCII characters
bool mtp_string_has_unicode(const uint8_t* buffer);

// Write MTP string to buffer (for response construction)
// Returns number of bytes written
// Format: [length:1byte][UTF-16LE chars][null terminator:2bytes]
size_t mtp_string_write(uint8_t* buffer, const char* utf8_str);

// Read MTP string length from buffer (without decoding)
uint8_t mtp_string_read_length(const uint8_t* buffer);

// Utility: Validate UTF-8 string
bool mtp_string_is_valid_utf8(const char* str);
