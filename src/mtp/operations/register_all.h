#pragma once

#include "../core/operation_registry.h"

// Register every operation handler this app implements. Returns the number
// of handlers actually registered (useful for sanity-checking that the
// registry didn't reject anyone).
size_t mtp_register_all_operations(MTPOperationRegistry* registry);
