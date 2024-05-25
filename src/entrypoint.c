#include <furi.h>
#include "main.h"

int32_t main_entrypoint(void* p) {
    UNUSED(p);
    main();
    return 0;
}
