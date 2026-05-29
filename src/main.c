#ifdef FLIPPER_ZERO

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_port.h>
#include <input/input.h>
#include <storage/storage.h>

#include <f0_mtp_icons.h>

#include "mtp/core/context.h"
#include "mtp/core/dispatcher.h"
#include "mtp/core/operation_registry.h"
#include "mtp/operations/register_all.h"
#include "mtp/operations/op_device_props.h"
#include "mtp/platform/flipper_usb.h"
#include "mtp/platform/flipper_provider.h"

typedef struct {
    FuriMessageQueue* input_queue;
    ViewPort* view_port;
    Gui* gui;

    Storage* storage;
    MTPContext* mtp_ctx;
    MTPOperationRegistry* registry;
    MTPTransport* transport;
    MTPDispatcher* dispatcher;
    FlipperMTPUsb* usb;
    bool usb_attached;

    char* serial;
    const Version* fw_version;
} MTPApp;

static void on_input(InputEvent* event, void* ctx) {
    MTPApp* app = ctx;
    if(event->type == InputTypeShort) {
        furi_message_queue_put(app->input_queue, event, FuriWaitForever);
    }
}

static void on_draw(Canvas* canvas, void* ctx) {
    MTPApp* app = ctx;
    bool connected = app->usb && flipper_usb_is_connected(app->usb);

    canvas_clear(canvas);
    canvas_set_bitmap_mode(canvas, true);

    if(!app->usb_attached) {
        canvas_draw_icon(canvas, 1, 31, &I_Connect_me);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 43, 10, "MTP Connection");
        canvas_draw_str(canvas, 10, 25, "USB mode busy");
        canvas_draw_icon(canvas, 2, 2, &I_Pin_back_arrow);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 15, 10, "Exit");
        canvas_draw_str(canvas, 42, 41, "Retrying USB");
        canvas_draw_str(canvas, 42, 50, "Reconnect USB");
    } else if(connected) {
        canvas_draw_icon(canvas, 0, 14, &I_DFU);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 43, 10, "MTP Connection");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 3, 22, "Disconnect or");
        canvas_draw_icon(canvas, 28, 23, &I_Pin_back_arrow);
        canvas_draw_str(canvas, 3, 31, "Press");
    } else {
        canvas_draw_icon(canvas, 1, 31, &I_Connect_me);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 43, 10, "MTP Connection");
        canvas_draw_str(canvas, 10, 25, "Plug me into computer!");
        canvas_draw_icon(canvas, 2, 2, &I_Pin_back_arrow);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 15, 10, "Exit");
        canvas_draw_str(canvas, 61, 41, "Waiting for USB");
        canvas_draw_str(canvas, 72, 50, "Connection...");
    }
}

static MTPApp* app_alloc(void) {
    MTPApp* app = malloc(sizeof(MTPApp));
    if(!app) return NULL;
    memset(app, 0, sizeof(*app));

    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);

    app->mtp_ctx = mtp_context_create(app->storage);
    app->serial = flipper_format_serial();
    app->fw_version = furi_hal_version_get_firmware_version();

    mtp_context_set_manufacturer(app->mtp_ctx, "Flipper Devices Inc.");
    mtp_context_set_model(app->mtp_ctx, "Flipper Zero");
    mtp_context_set_serial(app->mtp_ctx, app->serial ? app->serial : "HakureiReimu");
    mtp_context_set_firmware_version(
        app->mtp_ctx, app->fw_version ? version_get_version(app->fw_version) : "0.0.0");

    mtp_device_props_install(app->mtp_ctx, flipper_device_props_provider());

    app->registry = mtp_operation_registry_create();
    mtp_register_all_operations(app->registry);

    app->transport = mtp_transport_create(NULL);

    app->dispatcher = mtp_dispatcher_create(app->mtp_ctx, app->registry, app->transport);
    app->usb = flipper_usb_create(app->dispatcher);

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, on_draw, app);
    view_port_input_callback_set(app->view_port, on_input, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    return app;
}

static bool app_attach_usb(MTPApp* app) {
    if(!app || !app->usb || app->usb_attached) {
        return app && app->usb_attached;
    }

    app->usb_attached = flipper_usb_attach(app->usb);
    if(!app->usb_attached) {
        FURI_LOG_W("MTP", "Failed to switch USB into MTP mode, will retry");
    }
    return app->usb_attached;
}

static void app_free(MTPApp* app) {
    if(!app) return;

    if(app->usb) {
        if(app->usb_attached) {
            flipper_usb_detach(app->usb);
        }
        flipper_usb_destroy(app->usb);
    }
    if(app->dispatcher) mtp_dispatcher_destroy(app->dispatcher);
    if(app->transport) mtp_transport_destroy(app->transport);
    if(app->registry) mtp_operation_registry_destroy(app->registry);
    if(app->mtp_ctx) mtp_context_destroy(app->mtp_ctx);

    free(app->serial);

    if(app->view_port) {
        gui_remove_view_port(app->gui, app->view_port);
        view_port_free(app->view_port);
    }
    if(app->gui) furi_record_close(RECORD_GUI);
    if(app->storage) furi_record_close(RECORD_STORAGE);
    if(app->input_queue) furi_message_queue_free(app->input_queue);

    free(app);
}

int32_t entrypoint(void* p) {
    UNUSED(p);
    MTPApp* app = app_alloc();
    if(!app) return -1;

    app_attach_usb(app);

    InputEvent event;
    bool running = true;
    uint8_t retry_ticks = 0;
    while(running) {
        if(furi_message_queue_get(app->input_queue, &event, 100) == FuriStatusOk) {
            if(event.key == InputKeyBack) running = false;
            view_port_update(app->view_port);
        } else {
            if(!app->usb_attached && ++retry_ticks >= 10) {
                retry_ticks = 0;
                app_attach_usb(app);
            }
            view_port_update(app->view_port);
        }
    }

    app_free(app);
    return 0;
}

#endif // FLIPPER_ZERO
