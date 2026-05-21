#pragma once

#ifdef FLIPPER_ZERO

#include "../operations/op_device_props.h"

// Returns a singleton provider that reads from the real Flipper SDK:
//   - get_device_name: furi_hal_version_get_name_ptr()
//   - get_battery_level: power_get_info().charge
const MTPDevicePropsProvider* flipper_device_props_provider(void);

// Compose a hex-encoded device serial number from furi_hal_version_uid().
// Returns a newly malloc()'d string the caller owns.
char* flipper_format_serial(void);

#endif
