#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb.h>
#include "usb.h"
#include "usb_desc.h"

const struct usb_string_descriptor dev_manuf_desc = USB_STRING_DESC("Flipper Devices Inc.");
const struct usb_string_descriptor dev_prod_desc = USB_STRING_DESC("MTP Device");

const struct usb_device_descriptor usb_mtp_dev_descr = {
    .bLength = sizeof(struct usb_device_descriptor),
    .bDescriptorType = USB_DTYPE_DEVICE,
    .bcdUSB = VERSION_BCD(2, 0, 0),
    .bDeviceClass = USB_CLASS_STILL_IMAGE, // MTP falls under Still Image class
    .bDeviceSubClass = USB_SUBCLASS_MTP, // Subclass for MTP
    .bDeviceProtocol = USB_PROTO_MTP, // Protocol for MTP
    .bMaxPacketSize0 = MTP_MAX_PACKET_SIZE,
    .idVendor = 0x0483, // STMicroelectronics
    .idProduct = 0x5741, // Custom Product ID
    .bcdDevice = VERSION_BCD(1, 0, 0),
    .iManufacturer = UsbDevManuf, // UsbDevManuf
    .iProduct = UsbDevProduct, // UsbDevProduct
    .iSerialNumber = UsbDevSerial, // UsbDevSerial
    .bNumConfigurations = 1,
};

const struct MtpDescriptor usb_mtp_cfg_descr = {
    .config =
        {
            .bLength = sizeof(struct usb_config_descriptor),
            .bDescriptorType = USB_DTYPE_CONFIGURATION,
            .wTotalLength = sizeof(struct MtpDescriptor),
            .bNumInterfaces = 1,
            .bConfigurationValue = USB_CONF_VAL_MTP,
            .iConfiguration = UsbDevHighSpeed,
            .bmAttributes = USB_CFG_ATTR_RESERVED | USB_CFG_ATTR_SELFPOWERED,
            .bMaxPower = USB_CFG_POWER_MA(100),
        },
    .intf =
        {
            .bLength = sizeof(struct usb_interface_descriptor),
            .bDescriptorType = USB_DTYPE_INTERFACE,
            .bInterfaceNumber = 0,
            .bAlternateSetting = 0,
            .bNumEndpoints = 2,
            .bInterfaceClass = USB_CLASS_STILL_IMAGE,
            .bInterfaceSubClass = 1, // Subclass for MTP
            .bInterfaceProtocol = 1, // Protocol for MTP
            .iInterface = NO_DESCRIPTOR,
        },
    .ep_rx =
        {
            .bLength = sizeof(struct usb_endpoint_descriptor),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = USB_MTP_RX_EP,
            .bmAttributes = USB_EPTYPE_BULK,
            .wMaxPacketSize = USB_MTP_RX_EP_SIZE,
            .bInterval = 0,
        },
    .ep_tx =
        {
            .bLength = sizeof(struct usb_endpoint_descriptor),
            .bDescriptorType = USB_DTYPE_ENDPOINT,
            .bEndpointAddress = USB_MTP_TX_EP,
            .bmAttributes = USB_EPTYPE_BULK,
            .wMaxPacketSize = USB_MTP_TX_EP_SIZE,
            .bInterval = 0,
        },
};

FuriHalUsbInterface usb_mtp_interface = {
    .init = usb_mtp_init,
    .deinit = usb_mtp_deinit,
    .wakeup = usb_mtp_wakeup,
    .suspend = usb_mtp_suspend,

    .dev_descr = (struct usb_device_descriptor*)&usb_mtp_dev_descr,

    .str_manuf_descr = (void*)&dev_manuf_desc,
    .str_prod_descr = (void*)&dev_prod_desc,
    .str_serial_descr = NULL,

    .cfg_descr = (void*)&usb_mtp_cfg_descr,
};
