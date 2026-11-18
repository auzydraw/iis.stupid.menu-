// Orchestrates the in-world menu.
// Hold right B = menu visible. Release = menu destroyed.

#include "menu.h"
#include "input.h"
#include "menu_ui.h"

#include <android/log.h>

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "iistupid/menu", __VA_ARGS__)

namespace menu {

void tick() {
    bool b_held = input::state().rightSecondary;

    if (b_held && !menu_ui::is_open()) {
        menu_ui::open();
    } else if (!b_held && menu_ui::is_open()) {
        menu_ui::close();
    }

    menu_ui::tick();
}

} // namespace menu
