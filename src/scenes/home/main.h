#pragma once
#include <gui/view.h>
#include <gui/modules/submenu.h>
#include <gui/modules/popup.h>

typedef struct AppHome {
    Submenu* menu;
    View* view;
} AppHome;

AppHome* Home_alloc();
void Home_free(void* ptr);
View* Home_get_view(void* ptr);
void Home_on_enter(void* context);
bool Home_on_event(void* context, SceneManagerEvent event);
void Home_on_exit(void* context);
