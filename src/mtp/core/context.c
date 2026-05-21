#include "context.h"
#include <stdlib.h>
#include <string.h>

MTPContext* mtp_context_create(Storage* storage) {
    if(!storage) {
        return NULL;
    }

    MTPContext* ctx = calloc(1, sizeof(MTPContext));
    if(!ctx) {
        return NULL;
    }

    ctx->storage = storage;

    // Create object index
    ctx->object_index = mtp_object_index_create();
    if(!ctx->object_index) {
        free(ctx);
        return NULL;
    }

    // Initialize session state
    ctx->session.session_id = 0;
    ctx->session.is_open = false;

    // Transfer manager will be created when needed
    ctx->transfer_manager = NULL;

    // Clear error context
    mtp_error_context_clear(&ctx->last_error);

    return ctx;
}

void mtp_context_destroy(MTPContext* ctx) {
    if(!ctx) {
        return;
    }

    // Destroy object index
    if(ctx->object_index) {
        mtp_object_index_destroy(ctx->object_index);
    }

    // Destroy transfer manager if one was created. Indirect call so that
    // tests / modules that don't use the transfer state don't have to link
    // transfer_state.c.
    if(ctx->transfer_manager && ctx->transfer_manager_destroy) {
        ctx->transfer_manager_destroy(ctx->transfer_manager);
    }

    // Note: storage is NOT destroyed here (owned by caller)

    free(ctx);
}

void mtp_context_set_device_name(MTPContext* ctx, const char* name) {
    if(ctx) {
        ctx->device_info.device_name = name;
    }
}

void mtp_context_set_manufacturer(MTPContext* ctx, const char* manufacturer) {
    if(ctx) {
        ctx->device_info.manufacturer = manufacturer;
    }
}

void mtp_context_set_model(MTPContext* ctx, const char* model) {
    if(ctx) {
        ctx->device_info.model = model;
    }
}

void mtp_context_set_serial(MTPContext* ctx, const char* serial) {
    if(ctx) {
        ctx->device_info.serial = serial;
    }
}

void mtp_context_set_firmware_version(MTPContext* ctx, const char* version) {
    if(ctx) {
        ctx->device_info.firmware_version = version;
    }
}

void mtp_context_open_session(MTPContext* ctx, uint32_t session_id) {
    if(!ctx) {
        return;
    }

    ctx->session.session_id = session_id;
    ctx->session.is_open = true;
}

void mtp_context_close_session(MTPContext* ctx) {
    if(!ctx) {
        return;
    }

    ctx->session.session_id = 0;
    ctx->session.is_open = false;
}

bool mtp_context_is_session_open(const MTPContext* ctx) {
    return ctx ? ctx->session.is_open : false;
}
