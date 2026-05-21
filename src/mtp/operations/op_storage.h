#pragma once

#include "../core/operation.h"

// Storage-related operation handlers. Both reach into ctx->storage and the
// Flipper storage API; in tests they're driven through flipper_api_fake.
MTP_OPERATION_HANDLER(get_storage_ids);
MTP_OPERATION_HANDLER(get_storage_info);
