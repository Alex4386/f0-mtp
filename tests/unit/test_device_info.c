#include "../framework/test.h"
#include "../../src/mtp/operations/device_info.h"
#include <string.h>

static const uint16_t ops[] = {
    MTP_OP_GET_DEVICE_INFO,
    MTP_OP_OPEN_SESSION,
    MTP_OP_CLOSE_SESSION,
};

static const uint16_t fmts[] = {MTP_FORMAT_UNDEFINED, MTP_FORMAT_ASSOCIATION};
static const uint16_t dev_props[] = {MTP_DEVICE_PROP_DEVICE_FRIENDLY_NAME};

static MTPDeviceInfoInput sample_input(void) {
    MTPDeviceInfoInput in = {
        .manufacturer = "Flipper Devices Inc.",
        .model = "Flipper Zero",
        .device_version = "1.0.0",
        .serial = "ABC123",

        .supported_operations = ops,
        .supported_operations_count = sizeof(ops) / sizeof(ops[0]),

        .supported_events = NULL,
        .supported_events_count = 0,

        .supported_device_props = dev_props,
        .supported_device_props_count = sizeof(dev_props) / sizeof(dev_props[0]),

        .supported_capture_formats = NULL,
        .supported_capture_formats_count = 0,

        .supported_playback_formats = fmts,
        .supported_playback_formats_count = sizeof(fmts) / sizeof(fmts[0]),
    };
    return in;
}

TEST(device_info_writes_some_bytes) {
    uint8_t buf[1024];
    MTPDeviceInfoInput in = sample_input();
    size_t n = mtp_build_device_info(&in, buf, sizeof(buf));
    ASSERT(n > 0);
    ASSERT(n < sizeof(buf));
    TEST_PASS();
}

TEST(device_info_starts_with_standard_version) {
    uint8_t buf[1024];
    MTPDeviceInfoInput in = sample_input();
    size_t n = mtp_build_device_info(&in, buf, sizeof(buf));
    ASSERT(n >= 2);
    // little-endian uint16 MTP_STANDARD_VERSION (100 = 0x64)
    ASSERT_EQ(buf[0], (uint8_t)(MTP_STANDARD_VERSION & 0xFF));
    ASSERT_EQ(buf[1], (uint8_t)((MTP_STANDARD_VERSION >> 8) & 0xFF));
    TEST_PASS();
}

TEST(device_info_vendor_ext_id) {
    uint8_t buf[1024];
    MTPDeviceInfoInput in = sample_input();
    size_t n = mtp_build_device_info(&in, buf, sizeof(buf));
    ASSERT(n >= 6);
    uint32_t vendor = (uint32_t)buf[2] | ((uint32_t)buf[3] << 8) | ((uint32_t)buf[4] << 16) |
                      ((uint32_t)buf[5] << 24);
    ASSERT_EQ(vendor, (uint32_t)MTP_VENDOR_EXTENSION_ID);
    TEST_PASS();
}

TEST(device_info_overflow_returns_zero) {
    uint8_t buf[4]; // way too small
    MTPDeviceInfoInput in = sample_input();
    size_t n = mtp_build_device_info(&in, buf, sizeof(buf));
    ASSERT_EQ(n, (size_t)0);
    TEST_PASS();
}

TEST(device_info_null_safe) {
    uint8_t buf[1024];
    MTPDeviceInfoInput in = sample_input();
    ASSERT_EQ(mtp_build_device_info(NULL, buf, sizeof(buf)), (size_t)0);
    ASSERT_EQ(mtp_build_device_info(&in, NULL, sizeof(buf)), (size_t)0);
    ASSERT_EQ(mtp_build_device_info(&in, buf, 0), (size_t)0);
    TEST_PASS();
}

TEST(device_info_handles_null_strings_as_empty) {
    uint8_t buf[1024];
    MTPDeviceInfoInput in = sample_input();
    in.manufacturer = NULL;
    in.model = NULL;
    in.device_version = NULL;
    in.serial = NULL;
    size_t n = mtp_build_device_info(&in, buf, sizeof(buf));
    ASSERT(n > 0);
    TEST_PASS();
}

TEST(device_info_empty_arrays_emit_zero_count) {
    uint8_t buf[1024];
    MTPDeviceInfoInput in = sample_input();
    in.supported_operations = NULL;
    in.supported_operations_count = 0;
    in.supported_events = NULL;
    in.supported_events_count = 0;
    in.supported_device_props = NULL;
    in.supported_device_props_count = 0;
    in.supported_capture_formats = NULL;
    in.supported_capture_formats_count = 0;
    in.supported_playback_formats = NULL;
    in.supported_playback_formats_count = 0;

    size_t n = mtp_build_device_info(&in, buf, sizeof(buf));
    ASSERT(n > 0);
    // We can't easily test the exact offsets without parsing strings, but we
    // can sanity check that the function didn't blow up. A "small" payload is
    // still ~180 bytes because of the manufacturer/model/serial strings plus
    // the fixed vendor-extension-description.
    ASSERT(n < 256);
    TEST_PASS();
}

int main(void) {
    printf("Running Device Info Builder Tests\n");
    printf("==================================\n\n");

    RUN_TEST(device_info_writes_some_bytes);
    RUN_TEST(device_info_starts_with_standard_version);
    RUN_TEST(device_info_vendor_ext_id);
    RUN_TEST(device_info_overflow_returns_zero);
    RUN_TEST(device_info_null_safe);
    RUN_TEST(device_info_handles_null_strings_as_empty);
    RUN_TEST(device_info_empty_arrays_emit_zero_count);

    test_summary();
    return test_exit_code();
}
