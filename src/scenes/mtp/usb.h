#pragma once
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb.h>
#include "main.h"

/* === START furi_hal_usb_i.h === */
// https://github.com/flipperdevices/flipperzero-firmware/blob/03196fa11007c0f1e002cbb0b82102d8492456b5/targets/f7/furi_hal/furi_hal_usb_i.h#L5
#define USB_EP0_SIZE 8

enum UsbDevDescStr {
    UsbDevLang = 0,
    UsbDevManuf = 1,
    UsbDevProduct = 2,
    UsbDevSerial = 3,
};
/* ===   END furi_hal_usb_i.h === */

// Worker thread flags
typedef enum {
    EventExit = 1 << 0,
    EventReset = 1 << 1,
    EventRx = 1 << 2,
    EventTx = 1 << 3,
    EventRxOther = 1 << 4,
    EventInterrupt = 1 << 5,
    EventCancel = 1 << 6,
    EventAll = EventExit | EventReset | EventRx | EventTx | EventRxOther | EventInterrupt |
               EventCancel,
} MTPEvent;

// How long to wait for the host to collect an IN packet before giving up
#define MTP_TX_TIMEOUT_MS 5000

void usb_mtp_init(usbd_device* dev, FuriHalUsbInterface* intf, void* ctx);
void usb_mtp_deinit(usbd_device* dev);
void usb_mtp_wakeup(usbd_device* dev);
void usb_mtp_suspend(usbd_device* dev);

// Blocking bulk-IN write. Must be called from the worker thread.
bool usb_mtp_write(AppMTP* mtp, const uint8_t* buf, uint16_t len);
