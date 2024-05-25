#pragma once
#include <gui/gui.h>

typedef struct GUISetupData {
    FuriMessageQueue* msg_queue;
    ViewPort* viewport;
    Gui* gui;
} GUISetupData;

GUISetupData* setup_gui(ViewPortDrawCallback on_draw, ViewPortInputCallback on_input);
void free_gui(GUISetupData* data);
