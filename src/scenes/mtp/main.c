#include <gui/view.h>
#include <gui/modules/submenu.h>
#include "../../main.h"
#include "main.h"
#include <demo_app_icons.h>
#include "usb.h"

#define THIS_SCENE MTP

AppMTP* tmp;

void MTP_on_draw(Canvas* canvas, void* context);
AppMTP* MTP_alloc() {
    AppMTP* about = (AppMTP*)malloc(sizeof(AppMTP));
    about->view = view_alloc();
    view_set_context(about->view, about);
    view_set_draw_callback(about->view, MTP_on_draw);

    about->usb_connected = false;
    tmp = about;

    return about;
}

void MTP_on_draw(Canvas* canvas, void* context) {
    AppMTP* about = (AppMTP*)context;

    canvas_clear(canvas);
    canvas_set_bitmap_mode(canvas, true);

    bool usb_connected = false;
    if(about == NULL) {
        // facepalm
        if(tmp != NULL) {
            usb_connected = tmp->usb_connected;
        }
    } else {
        usb_connected = about->usb_connected;
    }

    if(usb_connected) {
        canvas_set_bitmap_mode(canvas, true);
        canvas_draw_icon(canvas, 0, 14, &I_DFU);
        canvas_draw_icon(canvas, 2, 2, &I_Pin_back_arrow);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 43, 10, "MTP Connection");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 15, 10, "Back");
        canvas_draw_str(canvas, 3, 22, "Disconnect or");
        canvas_draw_icon(canvas, 28, 23, &I_Pin_back_arrow);
        canvas_draw_str(canvas, 3, 31, "Press");
    } else {
        canvas_draw_icon(canvas, 1, 31, &I_Connect_me);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 10, 25, "Plug me into computer!");
        canvas_draw_icon(canvas, 2, 2, &I_Pin_back_arrow);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 15, 10, "Back");
        canvas_draw_str(canvas, 61, 41, "Waiting for USB");
        canvas_draw_str(canvas, 72, 50, "Connection...");
    }
}

void MTP_free(void* ptr) {
    AppMTP* home = (AppMTP*)ptr;
    FURI_LOG_I("DemoApp", "Triggering Free for view");

    view_free(home->view);
    home->view = NULL;

    free(home);
}

View* MTP_get_view(void* ptr) {
    AppMTP* home = (AppMTP*)ptr;
    return home->view;
}

void MTP_on_enter(void* context) {
    App* app = (App*)context;

    view_dispatcher_switch_to_view(app->view_dispatcher, THIS_SCENE);
    furi_assert(app->allocated_scenes != NULL, "App allocated scenes is NULL");

    AppMTP* mtp = app->allocated_scenes[THIS_SCENE];
    if(mtp != NULL) {
        mtp->old_usb = furi_hal_usb_get_config();
        furi_hal_usb_set_config(&usb_mtp_interface, mtp);
    }
}

bool MTP_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    if(event.type == SceneManagerEventTypeBack) {
        return false;
    }

    return true;
}

void MTP_on_exit(void* context) {
    App* app = (App*)context;
    if(app == NULL) {
        return;
    }

    // revert to old usb mode
    furi_hal_usb_set_config(tmp->old_usb, NULL);

    // if(app->view_dispatcher) view_dispatcher_switch_to_view(app->view_dispatcher, Home);
    // if(app->scene_manager) scene_manager_previous_scene(app->scene_manager);
}
