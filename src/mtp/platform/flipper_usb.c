#ifdef FLIPPER_ZERO

#include "flipper_usb.h"
#include "flipper_usb_desc.h"

#include "usbd_core.h"
#include "usb_std.h"
#include "usb.h"

#include <stdlib.h>
#include <string.h>

// --- Module state ---------------------------------------------------------

typedef enum {
    EventExit = 1 << 0,
    EventReset = 1 << 1,
    EventRx = 1 << 2,
    EventTx = 1 << 3,
    EventAll = EventExit | EventReset | EventRx | EventTx,
} MTPEvent;

struct FlipperMTPUsb {
    MTPDispatcher* dispatcher;
    usbd_device* dev;
    FuriHalUsbInterface* old_usb;
    FuriThread* worker;
    bool connected;
    bool write_pending;
};

// global needed so usbd_* callbacks (which take only `usbd_device*`) can find
// us. There can only be one active MTP USB instance at a time anyway.
static FlipperMTPUsb* g_active = NULL;

// --- USB endpoint callbacks ----------------------------------------------

static void mtp_txrx(usbd_device* dev, uint8_t event, uint8_t ep) {
    (void)event;
    FlipperMTPUsb* fu = g_active;
    if(!fu || fu->dev != dev) return;

    if(ep == MTP_EP_OUT_ADDR) {
        furi_thread_flags_set(furi_thread_get_id(fu->worker), EventRx);
    } else if(ep == MTP_EP_IN_ADDR) {
        furi_thread_flags_set(furi_thread_get_id(fu->worker), EventTx);
    }
}

static void mtp_interrupt(usbd_device* dev, uint8_t event, uint8_t ep) {
    (void)event;
    (void)ep;
    (void)dev;
}

// --- USB class control requests ------------------------------------------

static int handle_class_request(FlipperMTPUsb* fu, usbd_ctlreq* req) {
    (void)fu;
    switch(req->bRequest) {
    case 0x64: // CANCEL
    case 0x65: // GET_EXT_EVENT_DATA
    case 0x66: // RESET
        return 0;
    case 0x67: { // GET_DEVICE_STATUS
        struct {
            uint16_t wLength;
            uint16_t wCode;
        } __attribute__((packed)) status = {
            .wLength = 4,
            .wCode = MTP_RESP_OK,
        };
        size_t copy = req->wLength < sizeof(status) ? req->wLength : sizeof(status);
        memcpy(req->data, &status, copy);
        return (int)copy;
    }
    default:
        return -1;
    }
}

static usbd_respond mtp_control(usbd_device* dev, usbd_ctlreq* req, usbd_rqc_callback* cb) {
    (void)cb;
    FlipperMTPUsb* fu = g_active;
    if(!fu) return usbd_fail;

    uint16_t w_index = req->wIndex;
    uint16_t w_value = req->wValue;
    uint16_t w_length = req->wLength;
    int value = -1;

    if(req->bRequest == USB_STD_GET_DESCRIPTOR) {
        // Microsoft OS String descriptor
        if((w_value >> 8) == USB_DTYPE_STRING && (w_value & 0xff) == 0xee) {
            value = (w_length < mtp_os_string_len ? w_length : mtp_os_string_len);
            memcpy(req->data, mtp_os_string, value);
            return usbd_ack;
        }
    } else if((req->bmRequestType & USB_REQ_TYPE) == USB_REQ_VENDOR) {
        if(req->bRequest == 1 && (req->bmRequestType & USB_EPDIR_IN) &&
           (w_index == 4 || w_index == 5)) {
            value = (w_length < sizeof(mtp_ext_cfg_desc) ? w_length : sizeof(mtp_ext_cfg_desc));
            memcpy(req->data, &mtp_ext_cfg_desc, value);
            return usbd_ack;
        }
    } else if((req->bmRequestType & USB_REQ_TYPE) == USB_REQ_CLASS) {
        value = handle_class_request(fu, req);
    }

    if(value >= 0) {
        if(value > w_length) value = w_length;
        usbd_ep_write(dev, 0x00, req->data, value);
        return usbd_ack;
    }
    return usbd_fail;
}

// --- USB ep config -------------------------------------------------------

static usbd_respond mtp_ep_config(usbd_device* dev, uint8_t cfg) {
    FlipperMTPUsb* fu = g_active;
    switch(cfg) {
    case 0:
        usbd_ep_deconfig(dev, MTP_EP_IN_ADDR);
        usbd_ep_deconfig(dev, MTP_EP_OUT_ADDR);
        usbd_ep_deconfig(dev, MTP_EP_INT_IN_ADDR);
        usbd_reg_endpoint(dev, MTP_EP_IN_ADDR, NULL);
        usbd_reg_endpoint(dev, MTP_EP_OUT_ADDR, NULL);
        usbd_reg_endpoint(dev, MTP_EP_INT_IN_ADDR, NULL);
        if(fu) fu->connected = false;
        return usbd_ack;
    case 1:
        usbd_ep_config(dev, MTP_EP_OUT_ADDR, USB_EPTYPE_BULK, MTP_USB_PACKET_BYTES);
        usbd_ep_config(dev, MTP_EP_IN_ADDR, USB_EPTYPE_BULK, MTP_USB_PACKET_BYTES);
        usbd_ep_config(dev, MTP_EP_INT_IN_ADDR, USB_EPTYPE_INTERRUPT, USB_MAX_INTERRUPT_SIZE);
        usbd_reg_endpoint(dev, MTP_EP_OUT_ADDR, mtp_txrx);
        usbd_reg_endpoint(dev, MTP_EP_IN_ADDR, mtp_txrx);
        usbd_reg_endpoint(dev, MTP_EP_INT_IN_ADDR, mtp_interrupt);
        if(fu) fu->connected = true;
        return usbd_ack;
    default:
        return usbd_fail;
    }
}

// --- Worker thread -------------------------------------------------------

static int32_t mtp_worker(void* ctx) {
    FlipperMTPUsb* fu = ctx;
    uint8_t buffer[MTP_USB_PACKET_BYTES];

    while(true) {
        MTPEvent flags = furi_thread_flags_wait(EventAll, FuriFlagWaitAny, FuriWaitForever);

        if(flags & EventExit) break;

        if(flags & EventTx) {
            fu->write_pending = false;
        }

        if(flags & EventRx) {
            int32_t got = usbd_ep_read(fu->dev, MTP_EP_OUT_ADDR, buffer, sizeof(buffer));
            if(got > 0) {
                mtp_dispatcher_handle_packet(fu->dispatcher, buffer, (size_t)got);
            }
        }
    }
    return 0;
}

// --- USB transport callbacks (drive MTPTransport from this USB) ----------

static bool transport_send(MTPTransport* transport, const uint8_t* buffer, size_t size) {
    FlipperMTPUsb* fu = transport->usb_context;
    if(!fu || !fu->connected) return false;
    int32_t written = usbd_ep_write(fu->dev, MTP_EP_IN_ADDR, (void*)buffer, (int32_t)size);
    return written == (int32_t)size;
}

static bool transport_receive(MTPTransport* transport, uint8_t* buffer, size_t size, size_t* received) {
    // We drive receives through the worker thread + dispatcher_handle_packet,
    // so this synchronous receive API isn't used directly. Return false to
    // make any accidental caller fail loudly.
    (void)transport;
    (void)buffer;
    (void)size;
    if(received) *received = 0;
    return false;
}

static void transport_flush(MTPTransport* transport) {
    (void)transport;
}

// --- Lifecycle -----------------------------------------------------------

static void usb_init_cb(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx) {
    (void)intf;
    FlipperMTPUsb* fu = ctx;

    usbd_connect(dev, false);

    fu->dev = dev;
    g_active = fu;

    usbd_reg_config(dev, mtp_ep_config);
    usbd_reg_control(dev, mtp_control);

    usbd_connect(dev, true);

    fu->worker = furi_thread_alloc();
    furi_thread_set_name(fu->worker, "MtpUsbWorker");
    furi_thread_set_stack_size(fu->worker, 2048);
    furi_thread_set_context(fu->worker, fu);
    furi_thread_set_callback(fu->worker, mtp_worker);
    furi_thread_start(fu->worker);
}

static void usb_deinit_cb(usbd_device* dev) {
    FlipperMTPUsb* fu = g_active;
    if(!fu || fu->dev != dev) return;

    usbd_reg_config(dev, NULL);
    usbd_reg_control(dev, NULL);

    if(fu->worker) {
        furi_thread_flags_set(furi_thread_get_id(fu->worker), EventExit);
        furi_thread_join(fu->worker);
        furi_thread_free(fu->worker);
        fu->worker = NULL;
    }

    fu->connected = false;
    g_active = NULL;
}

static void usb_wakeup_cb(usbd_device* dev) {
    FlipperMTPUsb* fu = g_active;
    if(!fu || fu->dev != dev) return;
    fu->connected = true;
}

static void usb_suspend_cb(usbd_device* dev) {
    FlipperMTPUsb* fu = g_active;
    if(!fu || fu->dev != dev) return;
    fu->connected = false;
}

FuriHalUsbInterface mtp_usb_interface = {
    .init = usb_init_cb,
    .deinit = usb_deinit_cb,
    .wakeup = usb_wakeup_cb,
    .suspend = usb_suspend_cb,
    .dev_descr = (struct usb_device_descriptor*)&mtp_dev_descr,
    .str_manuf_descr = (void*)&mtp_dev_manuf_desc,
    .str_prod_descr = (void*)&mtp_dev_prod_desc,
    .str_serial_descr = NULL,
    .cfg_descr = (void*)&mtp_cfg_descr,
};

// --- Public API ----------------------------------------------------------

FlipperMTPUsb* flipper_usb_create(MTPDispatcher* dispatcher) {
    if(!dispatcher) return NULL;
    FlipperMTPUsb* fu = malloc(sizeof(*fu));
    if(!fu) return NULL;
    fu->dispatcher = dispatcher;
    fu->dev = NULL;
    fu->old_usb = NULL;
    fu->worker = NULL;
    fu->connected = false;
    fu->write_pending = false;

    // Wire the dispatcher's transport send callback to our USB IN endpoint.
    MTPTransport* t = mtp_dispatcher_transport(dispatcher);
    if(t) {
        t->usb_context = fu;
        mtp_transport_set_callbacks(t, transport_receive, transport_send, transport_flush);
    }
    return fu;
}

void flipper_usb_destroy(FlipperMTPUsb* fu) {
    free(fu);
}

bool flipper_usb_attach(FlipperMTPUsb* fu) {
    if(!fu) return false;
    fu->old_usb = furi_hal_usb_get_config();
    // Inherit serial number string from previous (CDC) config so the host
    // sees a stable serial.
    if(fu->old_usb) {
        mtp_usb_interface.str_serial_descr = fu->old_usb->str_serial_descr;
    }
    return furi_hal_usb_set_config(&mtp_usb_interface, fu);
}

void flipper_usb_detach(FlipperMTPUsb* fu) {
    if(!fu) return;
    if(fu->old_usb) {
        furi_hal_usb_set_config(fu->old_usb, NULL);
        fu->old_usb = NULL;
    }
}

bool flipper_usb_is_connected(FlipperMTPUsb* fu) {
    return fu ? fu->connected : false;
}

#endif // FLIPPER_ZERO
