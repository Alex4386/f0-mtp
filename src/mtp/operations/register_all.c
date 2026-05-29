#include "register_all.h"
#include "op_device.h"
#include "op_storage.h"
#include "op_objects.h"
#include "op_properties.h"
#include "op_device_props.h"
#include "op_transfer.h"

size_t mtp_register_all_operations(MTPOperationRegistry* registry) {
    if(!registry) return 0;

    static const MTPOperationEntry entries[] = {
        // Device / session
        {.op_code = MTP_OP_GET_DEVICE_INFO,
         .name = "get_device_info",
         .handler = mtp_op_get_device_info},
        {.op_code = MTP_OP_OPEN_SESSION, .name = "open_session", .handler = mtp_op_open_session},
        {.op_code = MTP_OP_CLOSE_SESSION, .name = "close_session", .handler = mtp_op_close_session},

        // Storage
        {.op_code = MTP_OP_GET_STORAGE_IDS,
         .name = "get_storage_ids",
         .handler = mtp_op_get_storage_ids},
        {.op_code = MTP_OP_GET_STORAGE_INFO,
         .name = "get_storage_info",
         .handler = mtp_op_get_storage_info},

        // Objects
        {.op_code = MTP_OP_GET_NUM_OBJECTS,
         .name = "get_num_objects",
         .handler = mtp_op_get_num_objects},
        {.op_code = MTP_OP_GET_OBJECT_HANDLES,
         .name = "get_object_handles",
         .handler = mtp_op_get_object_handles},
        {.op_code = MTP_OP_GET_OBJECT_INFO,
         .name = "get_object_info",
         .handler = mtp_op_get_object_info},
        {.op_code = MTP_OP_GET_OBJECT, .name = "get_object", .handler = mtp_op_get_object},
        {.op_code = MTP_OP_DELETE_OBJECT, .name = "delete_object", .handler = mtp_op_delete_object},
        {.op_code = MTP_OP_MOVE_OBJECT, .name = "move_object", .handler = mtp_op_move_object},

        // Properties
        {.op_code = MTP_OP_GET_OBJECT_PROPS_SUPPORTED,
         .name = "get_object_props_supported",
         .handler = mtp_op_get_object_props_supported},
        {.op_code = MTP_OP_GET_OBJECT_PROP_VALUE,
         .name = "get_object_prop_value",
         .handler = mtp_op_get_object_prop_value},
        {.op_code = MTP_OP_SET_OBJECT_PROP_VALUE,
         .name = "set_object_prop_value",
         .handler = mtp_op_set_object_prop_value},

        // Device props
        {.op_code = MTP_OP_GET_DEVICE_PROP_VALUE,
         .name = "get_device_prop_value",
         .handler = mtp_op_get_device_prop_value},
        {.op_code = MTP_OP_GET_DEVICE_PROP_DESC,
         .name = "get_device_prop_desc",
         .handler = mtp_op_get_device_prop_desc},

        // Transfer (multi-packet)
        {.op_code = MTP_OP_SEND_OBJECT_INFO,
         .name = "send_object_info",
         .handler = mtp_op_send_object_info},
        {.op_code = MTP_OP_SEND_OBJECT, .name = "send_object", .handler = mtp_op_send_object},
    };

    size_t registered = 0;
    const size_t n = sizeof(entries) / sizeof(entries[0]);
    for(size_t i = 0; i < n; i++) {
        if(mtp_operation_registry_add(registry, &entries[i])) {
            registered++;
        }
    }
    return registered;
}
