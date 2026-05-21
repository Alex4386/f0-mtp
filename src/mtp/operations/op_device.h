#pragma once

#include "../core/operation.h"

// Standard MTP operations supported by this device. Hand-curated to match
// what real desktop MTP clients call on a Flipper Zero.
extern const uint16_t MTP_SUPPORTED_OPERATIONS[];
extern const size_t MTP_SUPPORTED_OPERATIONS_COUNT;

extern const uint16_t MTP_SUPPORTED_EVENTS[];
extern const size_t MTP_SUPPORTED_EVENTS_COUNT;

extern const uint16_t MTP_SUPPORTED_DEVICE_PROPS[];
extern const size_t MTP_SUPPORTED_DEVICE_PROPS_COUNT;

extern const uint16_t MTP_SUPPORTED_CAPTURE_FORMATS[];
extern const size_t MTP_SUPPORTED_CAPTURE_FORMATS_COUNT;

extern const uint16_t MTP_SUPPORTED_PLAYBACK_FORMATS[];
extern const size_t MTP_SUPPORTED_PLAYBACK_FORMATS_COUNT;

extern const uint16_t MTP_SUPPORTED_OBJECT_PROPS[];
extern const size_t MTP_SUPPORTED_OBJECT_PROPS_COUNT;

// Operation handlers (used directly by register_all.c).
MTP_OPERATION_HANDLER(get_device_info);
MTP_OPERATION_HANDLER(open_session);
MTP_OPERATION_HANDLER(close_session);
MTP_OPERATION_HANDLER(get_object_props_supported);
