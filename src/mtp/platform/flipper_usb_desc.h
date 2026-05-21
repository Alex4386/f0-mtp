#pragma once

#ifdef FLIPPER_ZERO

#include <furi_hal_usb.h>
#include "usb.h"
#include "usb_std.h"

#define MTP_USB_MANUFACTURER_STRING "Flipper Devices Inc."
#define MTP_USB_PRODUCT_STRING      "Flipper Zero Virtual MTP Device"

#define MTP_USB_SUBCLASS 0x01
#define MTP_USB_PROTO    0x01
#define MTP_USB_CONF_VAL 1

#define MTP_EP_IN_ADDR     0x81
#define MTP_EP_OUT_ADDR    0x02
#define MTP_EP_INT_IN_ADDR 0x03

#define MTP_USB_PACKET_BYTES   64
#define USB_MAX_INTERRUPT_SIZE 28
#define MTP_BCD_VERSION        VERSION_BCD(1, 0, 0)

struct MtpDescriptor {
    struct usb_config_descriptor config;
    struct usb_interface_descriptor intf;

    struct usb_endpoint_descriptor ep_in;
    struct usb_endpoint_descriptor ep_out;
    struct usb_endpoint_descriptor ep_int_in;
} __attribute__((packed));

extern const struct usb_string_descriptor mtp_dev_manuf_desc;
extern const struct usb_string_descriptor mtp_dev_prod_desc;
extern const struct usb_device_descriptor mtp_dev_descr;
extern const struct MtpDescriptor mtp_cfg_descr;
extern FuriHalUsbInterface mtp_usb_interface;
extern const uint8_t mtp_os_string[];
extern const uint8_t mtp_os_string_len;

// Microsoft extended config descriptor
struct mtp_ext_cfg_hdr {
    uint32_t dwLength;
    uint16_t bcdVersion;
    uint16_t wIndex;
    uint8_t bCount;
    uint8_t reserved[7];
};
struct mtp_ext_cfg_fn {
    uint8_t bFirstInterfaceNumber;
    uint8_t bInterfaceCount;
    uint8_t compatibleID[8];
    uint8_t subCompatibleID[8];
    uint8_t reserved[6];
};
struct mtp_ext_cfg_desc {
    struct mtp_ext_cfg_hdr header;
    struct mtp_ext_cfg_fn function;
};
extern const struct mtp_ext_cfg_desc mtp_ext_cfg_desc;

#endif
