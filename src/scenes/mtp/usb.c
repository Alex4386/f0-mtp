#include "main.h"
#include "usb.h"
#include <furi.h>
#include <furi_hal.h>

// Define MTP specific request constants
#define MTP_REQ_GET_DEVICE_STATUS 0x67
#define MTP_REQ_SEND_DATA 0x68
#define MTP_REQ_GET_DATA 0x69

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
    while(true) {
        furi_thread_flags_wait(EventAll, FuriFlagWaitAny, FuriWaitForever);
        if(furi_thread_flags_get() & EventExit) break;

        if(furi_thread_flags_get() & EventReset) {
            furi_thread_flags_clear(EventReset);
            furi_hal_usb_set_config(mtp->old_usb, NULL);
        }

        if(furi_thread_flags_get() & EventRxTx) {
            furi_thread_flags_clear(EventRxTx);
            // Handle MTP RX/TX data here
            // Implement the logic for processing MTP requests, sending responses, etc.
        }
    }
    return 0;
}

void usb_mtp_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx) {
    UNUSED(intf);
    AppMTP* mtp = ctx;
    global_mtp = mtp;

    usbd_reg_control(dev, usb_mtp_control);
    usbd_connect(dev, true);

    mtp->worker_thread = furi_thread_alloc();
    furi_thread_set_name(mtp->worker_thread, "FlipperMTPUsb");
    furi_thread_set_stack_size(mtp->worker_thread, 1024);
    furi_thread_set_context(mtp->worker_thread, ctx);
    furi_thread_set_callback(mtp->worker_thread, usb_mtp_worker);
    furi_thread_start(mtp->worker_thread);
}

usbd_respond usb_mtp_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* callback) {
    UNUSED(callback);
    if(((USB_REQ_RECIPIENT | USB_REQ_TYPE) & req->bmRequestType) !=
       (USB_REQ_INTERFACE | USB_REQ_CLASS)) {
        if(global_mtp != NULL) {
            global_mtp->usb_connected = false;
        }
        return usbd_fail;
    }
    switch(req->bRequest) {};
    return usbd_fail;
}

void usb_deinit(usbd_device* dev) {
    usbd_reg_control(dev, NULL);

    AppMTP* mtp = global_mtp;
    if(!mtp || mtp->dev != dev) {
        FURI_LOG_E("MTP", "deinit mtp_cur leak");
        return;
    }

    global_mtp = NULL;

    furi_assert(mtp->worker_thread);
    furi_thread_flags_set(furi_thread_get_id(mtp->worker_thread), EventExit);
    furi_thread_join(mtp->worker_thread);
    furi_thread_free(mtp->worker_thread);
    mtp->worker_thread = NULL;
}

void usb_wakeup(usbd_device* dev) {
    UNUSED(dev);
}

void usb_suspend(usbd_device* dev) {
    AppMTP* mtp = global_mtp;
    if(!mtp || mtp->dev != dev) return;
    furi_thread_flags_set(furi_thread_get_id(mtp->worker_thread), EventReset);
}
