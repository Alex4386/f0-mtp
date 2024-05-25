#include <furi.h>
#include <gui/gui.h>
#include "gui.h"

GUISetupData* setup_gui(ViewPortDrawCallback on_draw, ViewPortInputCallback on_input) {
    GUISetupData* data = malloc(sizeof(GUISetupData));

    data->msg_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    data->viewport = view_port_alloc();

    view_port_draw_callback_set(data->viewport, on_draw, NULL);
    view_port_input_callback_set(data->viewport, on_input, data->msg_queue);
    data->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(data->gui, data->viewport, GuiLayerFullscreen);
    return data;
}

void free_gui(GUISetupData* data) {
    // nullguard!
    if(data == NULL) return;

    if(data->msg_queue != NULL) {
        furi_message_queue_free(data->msg_queue);
    }

    if(data->viewport != NULL) {
        if(data->gui != NULL) {
            gui_remove_view_port(data->gui, data->viewport);
        }

        view_port_enabled_set(data->viewport, false);
        view_port_free(data->viewport);
    }

    furi_record_close(RECORD_GUI);
    free(data);
}