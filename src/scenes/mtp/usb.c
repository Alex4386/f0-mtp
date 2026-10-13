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
uint8_t last_ep;

static bool usb_mtp_wait_tx(void) {
    const uint32_t abort_flags = EventExit | EventReset | EventCancel;
    uint32_t flags =
        furi_thread_flags_wait(EventTx | abort_flags, FuriFlagWaitAny, MTP_TX_TIMEOUT_MS);
    if(flags & FuriFlagError) {
        FURI_LOG_E("MTP", "Tx timeout");
        return false;
    }

    if(flags & abort_flags) {
        // hand the abort reason back to the worker loop
        furi_thread_flags_set(furi_thread_get_current_id(), flags & abort_flags);
        return false;
    }

    return true;
}

bool usb_mtp_write(AppMTP* mtp, const uint8_t* buf, uint16_t len) {
    // The IN endpoint is single buffered: a write while the previous packet
    // has not been collected yet is rejected by the driver.
    if(usbd_ep_write(mtp->dev, MTP_EP_IN_ADDR, buf, len) < 0) {
        if(!usb_mtp_wait_tx()) return false;
        if(usbd_ep_write(mtp->dev, MTP_EP_IN_ADDR, buf, len) < 0) return false;
    }

    return usb_mtp_wait_tx();
}

int32_t usb_mtp_worker(void* ctx) {
    AppMTP* mtp = ctx;
    usbd_device* dev = mtp->dev;
    uint8_t buffer[MTP_MAX_PACKET_SIZE];

    while(true) {
        uint32_t flags = furi_thread_flags_wait(EventAll, FuriFlagWaitAny, FuriWaitForever);
        if(flags & FuriFlagError) continue;

        if(flags & EventExit) {
            FURI_LOG_W("MTP", "Worker thread exit");
            mtp_reset_transfer(mtp);
            break;
        }

        if(flags & (EventReset | EventCancel)) {
            FURI_LOG_W("MTP", "Transfer reset");
            mtp_reset_transfer(mtp);
        }

        if(flags & EventRx) {
            int32_t readBytes = usbd_ep_read(dev, MTP_EP_OUT_ADDR, buffer, MTP_MAX_PACKET_SIZE);
            if(readBytes > 0) {
                mtp_handle_bulk(mtp, buffer, readBytes);
            }
        }

        if(flags & EventRxOther) {
            FURI_LOG_W("MTP", "USB Rx Other event. ep: %02x", last_ep);
        }

        if(flags & EventInterrupt) {
            FURI_LOG_D("MTP", "USB Interrupt event");
        }
    }

    return 0;
}

usbd_respond usb_mtp_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback) {
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
            value = (w_length < usb_mtp_os_string_len ? w_length : usb_mtp_os_string_len);
            memcpy(req->data, usb_mtp_os_string, value);
            return usbd_ack;
        }
    } else if((req->bmRequestType & USB_REQ_TYPE) == USB_REQ_VENDOR) {
        if(req->bRequest == 1 && (req->bmRequestType & USB_EPDIR_IN) &&
           (w_index == 4 || w_index == 5)) {
            value =
                (w_length < sizeof(mtp_ext_config_desc) ? w_length : sizeof(mtp_ext_config_desc));
            memcpy(req->data, &mtp_ext_config_desc, value);
            return usbd_ack;
        }
    } else if((req->bmRequestType & USB_REQ_TYPE) == USB_REQ_CLASS) {
        value = mtp_handle_class_control(mtp, dev, req);
    }

    if(value >= 0) {
        if(value > w_length) {
            value = w_length;
        }
        usbd_ep_write(dev, 0x00, req->data, value);
        return usbd_ack;
    }

    return usbd_fail;
}

void usb_mtp_txrx(usbd_device* dev, uint8_t event, uint8_t ep) {
    AppMTP* mtp = global_mtp;
    if(!mtp || mtp->dev != dev) return;

    if(event == usbd_evt_eprx) {
        furi_thread_flags_set(furi_thread_get_id(mtp->worker_thread), EventRx);
    } else if(event == usbd_evt_eptx) {
        furi_thread_flags_set(furi_thread_get_id(mtp->worker_thread), EventTx);
    } else {
        furi_thread_flags_set(furi_thread_get_id(mtp->worker_thread), EventRxOther);
        last_ep = ep;
    }
}

void usb_mtp_interrupt(usbd_device* dev, uint8_t event, uint8_t ep) {
    UNUSED(ep);
    UNUSED(event);

    AppMTP* mtp = global_mtp;
    if(!mtp || mtp->dev != dev) return;

    furi_thread_flags_set(furi_thread_get_id(mtp->worker_thread), EventInterrupt);
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
        usbd_reg_endpoint(dev, MTP_EP_INT_IN_ADDR, usb_mtp_interrupt);
        if(mtp != NULL) mtp->usb_connected = true;
        //usbd_ep_write(dev, MTP_MAX_PACKET_SIZE, 0, 0);
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

    // Disconnect the device first.
    usbd_connect(dev, false);

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
    furi_thread_set_stack_size(mtp->worker_thread, 2 * 1024);
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

    FURI_LOG_I("MTP", "USB wakeup");

    mtp->usb_connected = true;
    UNUSED(dev);
}

void usb_mtp_suspend(usbd_device* dev) {
    AppMTP* mtp = global_mtp;
    if(!mtp || mtp->dev != dev) return;

    FURI_LOG_I("MTP", "USB suspend");

    mtp->usb_connected = false;
    furi_thread_flags_set(furi_thread_get_id(mtp->worker_thread), EventReset);
}
