#include "op_transfer.h"
#include "../core/context.h"
#include "../core/transfer_state.h"

#include <stdlib.h>

// These handlers respond OK from the COMMAND phase only as an
// acknowledgement that the device is ready to receive DATA. The real
// success / failure response is sent later when the DATA phase completes.

MTP_OPERATION_HANDLER(send_object_info) {
    uint32_t storage_id = request->params[0];
    uint32_t parent = request->params[1];

    if(!ctx->transfer_manager) {
        ctx->transfer_manager = mtp_transfer_state_manager_create();
        // Wire the destroy callback so context cleanup tears down the manager.
        ctx->transfer_manager_destroy = mtp_transfer_state_manager_destroy;
    }
    if(!ctx->transfer_manager) {
        mtp_response_set_code(response, MTP_RESP_GENERAL_ERROR);
        return;
    }

    mtp_transfer_state_begin_send_object_info(
        ctx->transfer_manager, request->transaction_id, storage_id, parent);

    // Per MTP 1.1: don't send a RESPONSE here — the device replies after the
    // DATA phase completes. But our dispatcher always sends a response in the
    // current shape; mark code = OK so we follow the simpler flow. The
    // dispatcher will be enhanced later to defer the response.
    mtp_response_set_code(response, MTP_RESP_OK);
}

MTP_OPERATION_HANDLER(send_object) {
    if(!ctx->transfer_manager) {
        mtp_response_set_code(response, MTP_RESP_NO_VALID_OBJECT_INFO);
        return;
    }
    if(!mtp_transfer_state_begin_send_object(ctx->transfer_manager, ctx, request->transaction_id)) {
        mtp_response_set_code(response, MTP_RESP_NO_VALID_OBJECT_INFO);
        return;
    }
    mtp_response_set_code(response, MTP_RESP_OK);
}
