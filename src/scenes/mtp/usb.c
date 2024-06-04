#include "main.h"
#include "usb.h"
#include "usbd_core.h"
#include "usb_std.h"
#include "usb_desc.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb.h>

#include "mtp.h"

AppMTP* global_mtp;

typedef enum {
    EventExit = 1 << 0,
    EventReset = 1 << 1,
    EventRxTx = 1 << 2,
    EventAll = EventExit | EventReset | EventRxTx,
} MTPEvent;

int32_t usb_mtp_worker(void* ctx) {
    AppMTP* mtp = ctx;
    usbd_device* dev = mtp->dev;

    UNUSED(dev);

    while(true) {
        MTPEvent flags = furi_thread_flags_wait(EventAll, FuriFlagWaitAny, FuriWaitForever);
        if(flags & EventExit) {
            FURI_LOG_W("MTP", "Worker thread exit");
            break;
        }

        if(flags & EventReset) {
            FURI_LOG_W("MTP", "USB reset");
            // Handle USB reset if necessary
        }

        if(flags & EventRxTx) {
            FURI_LOG_W("MTP", "USB Rx/Tx event");
            // Handle MTP RX/TX data here
            // Implement the logic for processing MTP requests, sending responses, etc.
        }
    }

    return 0;
}

usbd_respond usb_mtp_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback) {
    UNUSED(dev);
    UNUSED(callback);

    AppMTP* mtp = global_mtp;
    int value = -1;
    uint16_t w_index = req->wIndex;
    uint16_t w_value = req->wValue;
    uint16_t w_length = req->wLength;

    FURI_LOG_I(
        "MTP",
        "Control Request: bmRequestType=0x%02x, bRequest=0x%02x, wValue=0x%04x, wIndex=0x%04x, wLength=0x%04x",
        req->bmRequestType,
        req->bRequest,
        w_value,
        w_index,
        w_length);

    if(req->bRequest == USB_STD_GET_DESCRIPTOR) {
        if((w_value >> 8) == USB_DTYPE_STRING && (w_value & 0xff) == 0xee) {
            FURI_LOG_I("MTP", "GET_DESCRIPTOR OS_STRING");
            value = (w_length < usb_mtp_os_string_len ? w_length : usb_mtp_os_string_len);
            memcpy(req->data, usb_mtp_os_string, value);
            return usbd_ack;
        }
    } else if((req->bmRequestType & USB_REQ_TYPE) == USB_REQ_VENDOR) {
        if(req->bRequest == 1 && (req->bmRequestType & USB_EPDIR_IN) &&
           (w_index == 4 || w_index == 5)) {
            FURI_LOG_I("MTP", "REQ_VENDOR - OS Descriptor");
            value =
                (w_length < sizeof(mtp_ext_config_desc) ? w_length : sizeof(mtp_ext_config_desc));
            memcpy(req->data, &mtp_ext_config_desc, value);
            return usbd_ack;
        }
    } else if((req->bmRequestType & USB_REQ_TYPE) == USB_REQ_CLASS) {
        switch(req->bRequest) {
        case MTP_REQ_RESET:
            if(w_index == 0 && w_value == 0) {
                FURI_LOG_I("MTP", "MTP_REQ_RESET");
                if(!mtp || mtp->dev != dev) return usbd_fail;
                furi_thread_flags_set(furi_thread_get_id(mtp->worker_thread), EventRxTx);
                value = w_length;
            }
            break;
        case MTP_REQ_CANCEL:
            if(w_index == 0 && w_value == 0) {
                FURI_LOG_I("MTP", "MTP_REQ_CANCEL");
                if(mtp->state == MTPStateBusy) {
                    mtp->state = MTPStateCanceled;
                }
                value = w_length;
            }
            break;
        case MTP_REQ_GET_DEVICE_STATUS:
            if(w_index == 0 && w_value == 0) {
                FURI_LOG_I("MTP", "MTP_REQ_GET_DEVICE_STATUS");
                struct mtp_device_status* status = (struct mtp_device_status*)req->data;
                status->wLength = sizeof(*status);
                if(mtp->state == MTPStateBusy) {
                    status->wCode = 0x2019; // Device Busy
                } else {
                    status->wCode = 0x2001; // Device Ready
                }
                value = sizeof(*status);
            }
            break;
        default:
            FURI_LOG_W("MTP", "Unsupported CLASS request: bRequest=0x%02x", req->bRequest);
            break;
        }
    }

    if(value >= 0) {
        if(value > w_length) {
            value = w_length;
        }
        dev->driver->ep_write(0x00, req->data, value);
        return usbd_ack;
    }

    return usbd_fail;
}

void usb_mtp_txrx(usbd_device* dev, uint8_t event, uint8_t ep) {
    UNUSED(ep);
    UNUSED(event);

    AppMTP* mtp = global_mtp;
    if(!mtp || mtp->dev != dev) return;

    furi_thread_flags_set(furi_thread_get_id(mtp->worker_thread), EventRxTx);
}

usbd_respond usb_mtp_ep_config(usbd_device* dev, uint8_t cfg) {
    AppMTP* mtp = global_mtp;
    FURI_LOG_I("MTP", "USB EP config: cfg=%d", cfg);

    switch(cfg) {
    case 0: // deconfigure
        FURI_LOG_I("MTP", "USB deconfigure");
        usbd_ep_deconfig(dev, MTP_EP_IN_ADDR);
        usbd_ep_deconfig(dev, MTP_EP_OUT_ADDR);
        usbd_ep_deconfig(dev, MTP_EP_INT_IN_ADDR);
        usbd_reg_endpoint(dev, MTP_EP_IN_ADDR, NULL);
        usbd_reg_endpoint(dev, MTP_EP_OUT_ADDR, NULL);
        usbd_reg_endpoint(dev, MTP_EP_INT_IN_ADDR, NULL);
        if(mtp != NULL) mtp->usb_connected = false;
        break;
    case 1: // configure
        FURI_LOG_I("MTP", "USB configure");
        usbd_ep_config(dev, MTP_EP_OUT_ADDR, USB_EPTYPE_BULK, MTP_MAX_PACKET_SIZE);
        usbd_ep_config(dev, MTP_EP_IN_ADDR, USB_EPTYPE_BULK, MTP_MAX_PACKET_SIZE);
        usbd_ep_config(dev, MTP_EP_INT_IN_ADDR, USB_EPTYPE_INTERRUPT, USB_MAX_INTERRUPT_SIZE);
        usbd_reg_endpoint(dev, MTP_EP_OUT_ADDR, usb_mtp_txrx);
        usbd_reg_endpoint(dev, MTP_EP_IN_ADDR, usb_mtp_txrx);
        usbd_reg_endpoint(dev, MTP_EP_INT_IN_ADDR, usb_mtp_txrx);
        if(mtp != NULL) mtp->usb_connected = true;
        break;
    default:
        return usbd_fail;
    }

    return usbd_ack;
}

void usb_mtp_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx) {
    UNUSED(intf);
    if(dev == NULL) {
        FURI_LOG_E("MTP", "dev is NULL");
    }

    AppMTP* mtp = ctx;
    global_mtp = mtp;
    mtp->dev = dev;

    FURI_LOG_I("MTP", "Initializing MTP device");

    // Register the configuration and control handlers

    usbd_reg_config(dev, usb_mtp_ep_config);
    usbd_reg_control(dev, usb_mtp_control);
    FURI_LOG_I("MTP", "Registered configuration and control handlers");

    // Connect the device
    usbd_connect(dev, true);
    FURI_LOG_I("MTP", "Connected device");

    // Initialize worker thread
    mtp->worker_thread = furi_thread_alloc();
    furi_thread_set_name(mtp->worker_thread, "FlipperMTPUsb");
    furi_thread_set_stack_size(mtp->worker_thread, 1024);
    furi_thread_set_context(mtp->worker_thread, ctx);
    furi_thread_set_callback(mtp->worker_thread, usb_mtp_worker);
    furi_thread_start(mtp->worker_thread);
    FURI_LOG_I("MTP", "Started worker thread");
}

void usb_mtp_deinit(usbd_device* dev) {
    usbd_reg_config(dev, NULL);
    usbd_reg_control(dev, NULL);
    FURI_LOG_I("MTP", "Unregistered configuration and control handlers");

    AppMTP* mtp = global_mtp;
    if(!mtp || mtp->dev != dev) {
        FURI_LOG_E("MTP", "deinit mtp_cur leak");
        return;
    }

    global_mtp = NULL;

    furi_assert(mtp->worker_thread);
    FURI_LOG_I("MTP", "Worker thread Condition pass");

    furi_thread_flags_set(furi_thread_get_id(mtp->worker_thread), EventExit);
    furi_thread_join(mtp->worker_thread);
    furi_thread_free(mtp->worker_thread);
    mtp->worker_thread = NULL;

    mtp->usb_connected = false;
    mtp->is_working = false;

    FURI_LOG_I("MTP", "Deinit worker thread");
}

void usb_mtp_wakeup(usbd_device* dev) {
    AppMTP* mtp = global_mtp;
    if(!mtp || mtp->dev != dev) return;

    mtp->is_working = true;
    UNUSED(dev);
}

void usb_mtp_suspend(usbd_device* dev) {
    AppMTP* mtp = global_mtp;
    if(!mtp || mtp->dev != dev) return;

    mtp->is_working = false;
    furi_thread_flags_set(furi_thread_get_id(mtp->worker_thread), EventReset);
}
