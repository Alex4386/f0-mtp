#pragma once
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb.h>

struct MtpDescriptor {
    struct usb_config_descriptor config;
    struct usb_interface_descriptor intf;
    struct usb_endpoint_descriptor ep_rx;
    struct usb_endpoint_descriptor ep_tx;
} __attribute__((packed));

extern const struct usb_string_descriptor dev_manuf_desc;
extern const struct usb_string_descriptor dev_prod_desc;
extern const struct usb_device_descriptor usb_mtp_dev_descr;
extern const struct MtpDescriptor usb_mtp_cfg_descr;
extern FuriHalUsbInterface usb_mtp_interface;
