#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "bn_string.h"

#include "theme.h"      // keyboard_layout

namespace app
{
    // Overwrites the characters before clearing, so secrets don't linger in memory.
    void wipe_string(bn::string<64>& value);

    // On-screen keyboard screen, drawn by the current theme. Edits value in place.
    // Returns true when the user presses START, false if they cancel (L, or B with empty text).
    // message, if given, replaces the button hints until the next key press (e.g. a validation error).
    bool run_keyboard(keyboard_layout layout, const char* title, bn::string<64>& value, int max_chars,
                      const char* message = nullptr);
}

#endif
