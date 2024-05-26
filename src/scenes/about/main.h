#pragma once
#include <gui/view.h>
#include <gui/modules/submenu.h>

typedef struct AppAbout {
    View* view;
    Canvas* canvas;
} AppAbout;

AppAbout* About_alloc();
void About_free(void* ptr);
View* About_get_view(void* ptr);
void About_on_enter(void* context);
bool About_on_event(void* context, SceneManagerEvent event);
void About_on_exit(void* context);
