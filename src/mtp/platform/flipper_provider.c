#ifdef FLIPPER_ZERO

#include "flipper_provider.h"

#include <furi.h>
#include <furi_hal.h>
#include <power/power_service/power.h>
#include <stdlib.h>
#include <string.h>

static const char* provider_device_name(void) {
    const char* name = furi_hal_version_get_name_ptr();
    return name ? name : "Flipper Zero";
}

static uint8_t provider_battery_level(void) {
    Power* power = furi_record_open(RECORD_POWER);
    uint8_t level = 0;
    if(power) {
        PowerInfo info;
        power_get_info(power, &info);
        level = info.charge;
        furi_record_close(RECORD_POWER);
    }
    return level;
}

const MTPDevicePropsProvider* flipper_device_props_provider(void) {
    static const MTPDevicePropsProvider p = {
        .get_device_name = provider_device_name,
        .get_battery_level = provider_battery_level,
    };
    return &p;
}

static const char HEX[] = "0123456789ABCDEF";

char* flipper_format_serial(void) {
    size_t uid_len = furi_hal_version_uid_size();
    const uint8_t* uid = furi_hal_version_uid();
    char* out = malloc(uid_len * 2 + 1);
    if(!out) return NULL;
    for(size_t i = 0; i < uid_len; i++) {
        out[i * 2 + 0] = HEX[(uid[i] >> 4) & 0xF];
        out[i * 2 + 1] = HEX[uid[i] & 0xF];
    }
    out[uid_len * 2] = '\0';
    return out;
}

#endif
