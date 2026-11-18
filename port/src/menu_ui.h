// In-world menu panel: cursor, buttons, keyboard, verification.
#pragma once

namespace menu_ui {

// Build / tear down the whole UI (cursor + panel). Idempotent.
void open();
void close();
bool is_open();

// Per-frame tick when open.
void tick();

} // namespace menu_ui
