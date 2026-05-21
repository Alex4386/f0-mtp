#pragma once

#include "../core/operation.h"

// Storage-backed object handlers.
MTP_OPERATION_HANDLER(get_num_objects);
MTP_OPERATION_HANDLER(get_object_handles);
MTP_OPERATION_HANDLER(get_object_info);
MTP_OPERATION_HANDLER(get_object);
MTP_OPERATION_HANDLER(delete_object);
MTP_OPERATION_HANDLER(move_object);
