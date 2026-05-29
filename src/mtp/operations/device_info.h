#pragma once

#include "../core/mtp_types.h"

// Inputs for building the GetDeviceInfo dataset.
// All string pointers must remain valid for the duration of the call but are
// only read.
typedef struct {
    const char* manufacturer;
    const char* model;
    const char* device_version;
    const char* serial;

    const uint16_t* supported_operations;
    size_t supported_operations_count;

    const uint16_t* supported_events;
    size_t supported_events_count;

    const uint16_t* supported_device_props;
    size_t supported_device_props_count;

    const uint16_t* supported_capture_formats;
    size_t supported_capture_formats_count;

    const uint16_t* supported_playback_formats;
    size_t supported_playback_formats_count;
} MTPDeviceInfoInput;

// Serialize a GetDeviceInfo response payload into `buffer` (size `buffer_size`).
// Returns the number of bytes written, or 0 on failure / overflow.
//
// Layout matches the MTP 1.1 spec, byte-for-byte compatible with src.old's
// BuildDeviceInfo() output.
size_t mtp_build_device_info(const MTPDeviceInfoInput* in, uint8_t* buffer, size_t buffer_size);
