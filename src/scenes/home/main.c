#include <gui/view.h>
#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>
#include "../../main.h"
#include "main.h"

#define THIS_SCENE Home

AppHome* Home_alloc() {
    AppHome* home = (AppHome*)malloc(sizeof(AppHome));
    home->menu = submenu_alloc();
    home->view = NULL;

    return home;
}

void Home_free(void* ptr) {
    AppHome* home = (AppHome*)ptr;

    FURI_LOG_I("DemoApp", "Freeing Home.");
    if(home == NULL) {
        FURI_LOG_I("DemoApp", "Home is NULL.");
        return;
    }

    if(home->view != NULL) {
        FURI_LOG_I("DemoApp", "Freeing View.");
        view_free(home->view);
        home->view = NULL;
    }

    // I don't know the reason why
    // but this is causing NULL pointer exception

    // if(home->menu != NULL) {
    //     FURI_LOG_I("DemoApp", "Freeing Submenu.");
    //     submenu_free(home->menu);
    //     home->menu = NULL;
    // }

    free(home);
    FURI_LOG_I("DemoApp", "Home freed.");
}

View* Home_get_view(void* ptr) {
    AppHome* home = (AppHome*)ptr;

    if(home->view == NULL) {
        home->view = submenu_get_view(home->menu);
        furi_assert(home->view != NULL, "View is NULL");
    }

    return home->view;
}

void Home_on_submenu_item(void* context, uint32_t index) {
    App* app = (App*)context;
    AppHome* home = app->allocated_scenes[THIS_SCENE];
    furi_assert(home != NULL, "Home is NULL");

    switch(index) {
    case 0:
        FURI_LOG_I("DemoApp", "Hello World");
        break;
    case 1:
        FURI_LOG_I("DemoApp", "About");
        scene_manager_next_scene(app->scene_manager, About);
        break;
    case 2:
        FURI_LOG_I("DemoApp", "MTP");
        scene_manager_next_scene(app->scene_manager, MTP);
        break;
    case 99:
        FURI_LOG_I("DemoApp", "Exit");
        Home_on_exit(app);
        view_dispatcher_stop(app->view_dispatcher);
        break;
    default:
        break;
    }
}

void Home_on_enter(void* context) {
    App* app = (App*)context;
    AppHome* home = app->allocated_scenes[THIS_SCENE];

    submenu_add_item(home->menu, "Hello World", 0, Home_on_submenu_item, app);
    submenu_add_item(home->menu, "About", 1, Home_on_submenu_item, app);
    submenu_add_item(home->menu, "MTP", 2, Home_on_submenu_item, app);
    submenu_add_item(home->menu, "Exit", 99, Home_on_submenu_item, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, THIS_SCENE);
}

bool Home_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);

    if(event.type == SceneManagerEventTypeBack) {
        return false;
    }

    return true;
}

void Home_on_exit(void* context) {
    App* app = (App*)context;
    // if the on_exit has been ran, the app will be NULL
    if(app == NULL || app->allocated_scenes == NULL) {
        return;
    }

    AppHome* home = app->allocated_scenes[THIS_SCENE];
    if(home == NULL) {
        return;
    }

    FURI_LOG_I("DemoApp", "Exiting Home.");

    Submenu* menu = home->menu;
    if(menu != NULL) {
        submenu_reset(menu);
    }

    FURI_LOG_I("DemoApp", "Reset submenu complete.");
}
