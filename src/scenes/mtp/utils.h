#pragma once

#include <furi.h>

char byte_to_hex(uint8_t byte);
void print_bytes(char* tag, uint8_t* bytes, size_t len);
bool merge_path(char* target, size_t size, const char* base, const char* name);
char* ReadMTPString(const uint8_t* buffer);
void WriteMTPString(uint8_t* buffer, const char* str, uint16_t* length);
