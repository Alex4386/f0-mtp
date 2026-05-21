#include "op_properties.h"
#include "object_props.h"
#include "../core/context.h"
#include "../storage/storage_paths.h"

#include <stdlib.h>
#include <string.h>

MTP_OPERATION_HANDLER(get_object_prop_value) {
    uint32_t handle = request->params[0];
    uint32_t prop_code = request->params[1];

    const char* path = mtp_object_index_get_path(ctx->object_index, handle);
    if(!path) {
        mtp_response_set_code(response, MTP_RESP_INVALID_OBJECT_HANDLE);
        return;
    }

    uint8_t* buf = malloc(MTP_BUFFER_SIZE);
    if(!buf) {
        mtp_response_set_code(response, MTP_RESP_GENERAL_ERROR);
        return;
    }

    uint32_t storage_id = mtp_storage_id_from_path(path);
    const char* basename = mtp_storage_basename(path);

    size_t n = mtp_build_object_prop_value(
        (uint16_t)prop_code, storage_id, basename, buf, MTP_BUFFER_SIZE);
    if(n == 0) {
        free(buf);
        mtp_response_set_code(response, MTP_RESP_INVALID_OBJECT_PROP_CODE);
        return;
    }

    mtp_response_set_data(response, buf, n);
    mtp_response_set_code(response, MTP_RESP_OK);
}

// NOTE: SetObjectPropValue is a two-phase operation in MTP — the COMMAND
// container carries the [handle, prop_code] params, then a DATA container
// carries the new value bytes. This handler responds to the command phase by
// preparing the transfer state; the actual rename/data-completion happens in
// transfer_state.c when the DATA arrives.
//
// For now this is a stub that responds OK so the host can complete the
// transaction. Filename rename is wired through the transfer state.
MTP_OPERATION_HANDLER(set_object_prop_value) {
    (void)ctx;
    (void)request;
    mtp_response_set_code(response, MTP_RESP_OK);
}
