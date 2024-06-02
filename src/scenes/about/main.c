#include <gui/view.h>
#include <gui/modules/submenu.h>
#include "../../main.h"
#include "main.h"
#include <demo_app_icons.h>

#define THIS_SCENE About

void About_on_draw(Canvas* canvas, void* context);
AppAbout* About_alloc() {
    AppAbout* about = (AppAbout*)malloc(sizeof(AppAbout));
    about->view = view_alloc();
    view_set_draw_callback(about->view, About_on_draw);

    return about;
}

void About_on_draw(Canvas* canvas, void* context) {
    UNUSED(context);

    canvas_clear(canvas);

    canvas_set_bitmap_mode(canvas, true);
    canvas_draw_icon(canvas, 2, 3, &I_DolphinWait);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 67, 11, "f0-template");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 72, 20, "by Alex4386");
    canvas_draw_line(canvas, 68, 25, 124, 25);
    canvas_draw_str(canvas, 71, 39, "Protected by");
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 61, 51, "Fantasy Seal");
    canvas_draw_str(canvas, 69, 61, "Technology");
}

void About_free(void* ptr) {
    AppAbout* home = (AppAbout*)ptr;
    FURI_LOG_I("DemoApp", "Triggering Free for view");

    view_free(home->view);
    home->view = NULL;

    free(home);
}

View* About_get_view(void* ptr) {
    AppAbout* home = (AppAbout*)ptr;
    return home->view;
}

void About_on_enter(void* context) {
    App* app = (App*)context;

    view_dispatcher_switch_to_view(app->view_dispatcher, THIS_SCENE);
}

bool About_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    if(event.type == SceneManagerEventTypeBack) {
        return false;
    }

    return true;
}

void About_on_exit(void* context) {
    App* app = (App*)context;
    if(app == NULL) {
        return;
    }

    //if(app->view_dispatcher) view_dispatcher_switch_to_view(app->view_dispatcher, Home);
    //if(app->scene_manager) scene_manager_previous_scene(app->scene_manager);
}
