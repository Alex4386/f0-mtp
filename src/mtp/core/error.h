#pragma once

#include "mtp_types.h"

// Error codes
typedef enum {
    MTP_ERROR_NONE = 0,

    // General errors
    MTP_ERROR_INVALID_ARGUMENT,
    MTP_ERROR_OUT_OF_MEMORY,
    MTP_ERROR_INTERNAL,

    // MTP protocol errors
    MTP_ERROR_INVALID_HANDLE,
    MTP_ERROR_INVALID_STORAGE_ID,
    MTP_ERROR_INVALID_PARENT,
    MTP_ERROR_SESSION_NOT_OPEN,
    MTP_ERROR_OPERATION_NOT_SUPPORTED,

    // Storage errors
    MTP_ERROR_STORAGE_FULL,
    MTP_ERROR_STORAGE_READ_ONLY,
    MTP_ERROR_FILE_NOT_FOUND,
    MTP_ERROR_FILE_EXISTS,
    MTP_ERROR_ACCESS_DENIED,
    MTP_ERROR_IO_ERROR,

    // Transfer errors
    MTP_ERROR_TRANSFER_CANCELLED,
    MTP_ERROR_TRANSFER_CORRUPT,

} MTPError;

// Error context with debug information
typedef struct {
    MTPError code;
    char message[256];
    const char* file;
    int line;
    const char* function;
} MTPErrorContext;

// Map error code to MTP response code
uint16_t mtp_error_to_response_code(MTPError error);

// Get error description
const char* mtp_error_to_string(MTPError error);

// Error context management
void mtp_error_context_set(
    MTPErrorContext* ctx,
    MTPError code,
    const char* file,
    int line,
    const char* function,
    const char* fmt,
    ...
) __attribute__((format(printf, 6, 7)));

void mtp_error_context_clear(MTPErrorContext* ctx);

// Helper macros
#define MTP_SET_ERROR(error_ctx, error_code, fmt, ...) \
    mtp_error_context_set((error_ctx), (error_code), __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define MTP_CLEAR_ERROR(error_ctx) \
    mtp_error_context_clear(error_ctx)

#define MTP_HAS_ERROR(error_ctx) \
    ((error_ctx)->code != MTP_ERROR_NONE)
