#include "error.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

uint16_t mtp_error_to_response_code(MTPError error) {
    switch(error) {
    case MTP_ERROR_NONE:
        return MTP_RESP_OK;
    case MTP_ERROR_INVALID_HANDLE:
        return MTP_RESP_INVALID_OBJECT_HANDLE;
    case MTP_ERROR_INVALID_STORAGE_ID:
        return MTP_RESP_INVALID_STORAGE_ID;
    case MTP_ERROR_INVALID_PARENT:
        return MTP_RESP_INVALID_OBJECT_HANDLE;
    case MTP_ERROR_SESSION_NOT_OPEN:
        return MTP_RESP_SESSION_NOT_OPEN;
    case MTP_ERROR_OPERATION_NOT_SUPPORTED:
        return MTP_RESP_OPERATION_NOT_SUPPORTED;
    case MTP_ERROR_STORAGE_FULL:
        return MTP_RESP_STORE_FULL;
    case MTP_ERROR_STORAGE_READ_ONLY:
        return MTP_RESP_STORE_READ_ONLY;
    case MTP_ERROR_FILE_NOT_FOUND:
        return MTP_RESP_INVALID_OBJECT_HANDLE;
    case MTP_ERROR_FILE_EXISTS:
        return MTP_RESP_GENERAL_ERROR;
    case MTP_ERROR_ACCESS_DENIED:
        return MTP_RESP_ACCESS_DENIED;
    case MTP_ERROR_IO_ERROR:
        return MTP_RESP_GENERAL_ERROR;
    case MTP_ERROR_TRANSFER_CANCELLED:
        return MTP_RESP_INCOMPLETE_TRANSFER;
    case MTP_ERROR_TRANSFER_CORRUPT:
        return MTP_RESP_INCOMPLETE_TRANSFER;
    case MTP_ERROR_INVALID_ARGUMENT:
    case MTP_ERROR_OUT_OF_MEMORY:
    case MTP_ERROR_INTERNAL:
    default:
        return MTP_RESP_GENERAL_ERROR;
    }
}

const char* mtp_error_to_string(MTPError error) {
    switch(error) {
    case MTP_ERROR_NONE:
        return "No error";
    case MTP_ERROR_INVALID_ARGUMENT:
        return "Invalid argument";
    case MTP_ERROR_OUT_OF_MEMORY:
        return "Out of memory";
    case MTP_ERROR_INTERNAL:
        return "Internal error";
    case MTP_ERROR_INVALID_HANDLE:
        return "Invalid object handle";
    case MTP_ERROR_INVALID_STORAGE_ID:
        return "Invalid storage ID";
    case MTP_ERROR_INVALID_PARENT:
        return "Invalid parent handle";
    case MTP_ERROR_SESSION_NOT_OPEN:
        return "Session not open";
    case MTP_ERROR_OPERATION_NOT_SUPPORTED:
        return "Operation not supported";
    case MTP_ERROR_STORAGE_FULL:
        return "Storage full";
    case MTP_ERROR_STORAGE_READ_ONLY:
        return "Storage is read-only";
    case MTP_ERROR_FILE_NOT_FOUND:
        return "File not found";
    case MTP_ERROR_FILE_EXISTS:
        return "File already exists";
    case MTP_ERROR_ACCESS_DENIED:
        return "Access denied";
    case MTP_ERROR_IO_ERROR:
        return "I/O error";
    case MTP_ERROR_TRANSFER_CANCELLED:
        return "Transfer cancelled";
    case MTP_ERROR_TRANSFER_CORRUPT:
        return "Transfer data corrupt";
    default:
        return "Unknown error";
    }
}

void mtp_error_context_set(
    MTPErrorContext* ctx,
    MTPError code,
    const char* file,
    int line,
    const char* function,
    const char* fmt,
    ...) {
    ctx->code = code;
    ctx->file = file;
    ctx->line = line;
    ctx->function = function;

    va_list args;
    va_start(args, fmt);
    vsnprintf(ctx->message, sizeof(ctx->message), fmt, args);
    va_end(args);
}

void mtp_error_context_clear(MTPErrorContext* ctx) {
    ctx->code = MTP_ERROR_NONE;
    ctx->message[0] = '\0';
    ctx->file = NULL;
    ctx->line = 0;
    ctx->function = NULL;
}
