#include <stdio.h>
#include <furi.h>
#include <gui/gui.h>
#include "events.h"
#include "utils/gui.h"

int main() {
    // 1. Provision the InputHandlers
    // Handle input event
    InputEvent event;
    GUISetupData* gui_setup = setup_gui(on_draw, on_input);

    // 2. Main EventLoop
    while(true) {
        // 4.1. Read input event from the message queue
        furi_check(
            furi_message_queue_get(gui_setup->msg_queue, &event, FuriWaitForever) == FuriStatusOk);

        // 4.2. check if the event is a quit event
        if(event.key == InputKeyBack) {
            break;
        }
    }

    // 3. Free the resources
    free_gui(gui_setup);
}
