#ifdef FLIPPER_ZERO

#include "flipper_usb_desc.h"

const struct usb_string_descriptor mtp_dev_manuf_desc = USB_STRING_DESC(MTP_USB_MANUFACTURER_STRING);
const struct usb_string_descriptor mtp_dev_prod_desc = USB_STRING_DESC(MTP_USB_PRODUCT_STRING);

// Microsoft OS string descriptor (forces Windows to query our extended
// compat ID, which sets the class to MTP).
const uint8_t mtp_os_string[] = {
    18,
    USB_DTYPE_STRING,
    'M', 0, 'S', 0, 'F', 0, 'T', 0, '1', 0, '0', 0, '0', 0,
    /* vendor code */ 1,
    /* padding */ 0,
};
const uint8_t mtp_os_string_len = sizeof(mtp_os_string);

const struct usb_device_descriptor mtp_dev_descr = {
    .bLength = sizeof(struct usb_device_descriptor),
    .bDescriptorType = USB_DTYPE_DEVICE,
    .bcdUSB = VERSION_BCD(2, 0, 0),
    .bDeviceClass = USB_CLASS_STILL_IMAGE,
    .bDeviceSubClass = MTP_USB_SUBCLASS,
    .bDeviceProtocol = MTP_USB_PROTO,
    .bMaxPacketSize0 = USB_EP0_SIZE,
    .idVendor = 0x0483,
    .idProduct = 0x0105,
    .bcdDevice = MTP_BCD_VERSION,
    .iManufacturer = UsbDevManuf,
    .iProduct = UsbDevProduct,
    .iSerialNumber = UsbDevSerial,
    .bNumConfigurations = 1,
};

const struct MtpDescriptor mtp_cfg_descr = {
    .config = {
        .bLength = sizeof(struct usb_config_descriptor),
        .bDescriptorType = USB_DTYPE_CONFIGURATION,
        .wTotalLength = sizeof(struct MtpDescriptor),
        .bNumInterfaces = 1,
        .bConfigurationValue = MTP_USB_CONF_VAL,
        .iConfiguration = NO_DESCRIPTOR,
        .bmAttributes = USB_CFG_ATTR_RESERVED | USB_CFG_ATTR_SELFPOWERED,
        .bMaxPower = USB_CFG_POWER_MA(100),
    },
    .intf = {
        .bLength = sizeof(struct usb_interface_descriptor),
        .bDescriptorType = USB_DTYPE_INTERFACE,
        .bInterfaceNumber = 0,
        .iInterface = UsbDevManuf,
        .bAlternateSetting = 0,
        .bNumEndpoints = 3,
        .bInterfaceClass = USB_CLASS_STILL_IMAGE,
        .bInterfaceSubClass = MTP_USB_SUBCLASS,
        .bInterfaceProtocol = MTP_USB_PROTO,
    },
    .ep_in = {
        .bLength = sizeof(struct usb_endpoint_descriptor),
        .bDescriptorType = USB_DTYPE_ENDPOINT,
        .bEndpointAddress = MTP_EP_IN_ADDR,
        .bmAttributes = USB_EPTYPE_BULK,
        .wMaxPacketSize = MTP_USB_PACKET_BYTES,
        .bInterval = 0,
    },
    .ep_out = {
        .bLength = sizeof(struct usb_endpoint_descriptor),
        .bDescriptorType = USB_DTYPE_ENDPOINT,
        .bEndpointAddress = MTP_EP_OUT_ADDR,
        .bmAttributes = USB_EPTYPE_BULK,
        .wMaxPacketSize = MTP_USB_PACKET_BYTES,
        .bInterval = 0,
    },
    .ep_int_in = {
        .bLength = sizeof(struct usb_endpoint_descriptor),
        .bDescriptorType = USB_DTYPE_ENDPOINT,
        .bEndpointAddress = MTP_EP_INT_IN_ADDR,
        .bmAttributes = USB_EPTYPE_INTERRUPT,
        .wMaxPacketSize = USB_MAX_INTERRUPT_SIZE,
        .bInterval = 6,
    },
};

const struct mtp_ext_cfg_desc mtp_ext_cfg_desc = {
    .header = {
        .dwLength = sizeof(mtp_ext_cfg_desc),
        .bcdVersion = MTP_BCD_VERSION,
        .wIndex = 0x04,
        .bCount = 1,
    },
    .function = {
        .bFirstInterfaceNumber = 0,
        .bInterfaceCount = 1,
        .compatibleID = {'M', 'T', 'P', 0, 0, 0, 0, 0},
    },
};

#endif // FLIPPER_ZERO
