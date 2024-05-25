#include <gui/gui.h>
#include <demo_app_icons.h>

void on_draw(Canvas* canvas, void* context) {
    UNUSED(context);

    // clear canvas
    canvas_clear(canvas);

    // Set the font
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 32, 13, "Hello, World!");

    // draw dolphin first
    canvas_draw_icon(canvas, 32, 34, &I_dolphin_71x25);

    // write press back
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 15, 26, " press back to exit FAP");

    canvas_draw_line(canvas, 2, 16, 126, 16);
}

void on_input(InputEvent* event, void* context) {
    furi_assert(context);
    FuriMessageQueue* msg_queue = (FuriMessageQueue*)context;

    furi_message_queue_put(msg_queue, event, FuriWaitForever);
}
