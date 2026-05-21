#pragma once

#ifdef FLIPPER_ZERO

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb.h>

#include "../core/dispatcher.h"

// Flipper USB binding for the MTP class.
//
// Owns:
//   - The FuriHalUsbInterface MTP descriptor
//   - The worker thread that pumps bulk OUT packets into the dispatcher
//   - The MTPTransport whose send callback writes to USB IN endpoint
//
// Lifecycle (caller responsibility):
//   1. Build dispatcher + registry + context
//   2. flipper_usb_attach(dispatcher) — sets MTP USB config
//   3. ... event loop ...
//   4. flipper_usb_detach() — restores previous USB config

typedef struct FlipperMTPUsb FlipperMTPUsb;

FlipperMTPUsb* flipper_usb_create(MTPDispatcher* dispatcher);
void flipper_usb_destroy(FlipperMTPUsb* fu);

// Switch USB into MTP mode. Returns false if the system rejected the config.
bool flipper_usb_attach(FlipperMTPUsb* fu);

// Restore the prior USB config (typically VCP).
void flipper_usb_detach(FlipperMTPUsb* fu);

bool flipper_usb_is_connected(FlipperMTPUsb* fu);

#endif // FLIPPER_ZERO
