#pragma once

#include "../core/operation.h"

// Multi-packet operation handlers. These react to the COMMAND container by
// preparing the transfer state machine; the dispatcher then forwards the
// subsequent DATA container into transfer_state_feed_data().

MTP_OPERATION_HANDLER(send_object_info);
MTP_OPERATION_HANDLER(send_object);
