#pragma once

#include "../core/operation.h"

// Device-property hooks. The MTP layer never calls into Flipper-specific
// APIs directly; instead it asks the application (via these function pointers
// stored on MTPContext->user_data) for the live value.
//
// In tests, set them to fake providers; on Flipper, the platform layer
// installs ones that call furi_hal_version_* / power_get_info / etc.

typedef struct {
    // Friendly name (e.g. furi_hal_version_get_name_ptr())
    const char* (*get_device_name)(void);

    // Battery percentage 0..100
    uint8_t (*get_battery_level)(void);
} MTPDevicePropsProvider;

// Install the provider on a context. Must be called before any device-prop
// handler runs; otherwise default safe values ("Flipper Zero", 0%) are used.
void mtp_device_props_install(MTPContext* ctx, const MTPDevicePropsProvider* provider);

MTP_OPERATION_HANDLER(get_device_prop_value);
MTP_OPERATION_HANDLER(get_device_prop_desc);
