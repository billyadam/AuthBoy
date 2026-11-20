#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "bn_string.h"

#include "app_text.h"

namespace app
{
    enum class keyboard_layout
    {
        name,       // letters (SELECT toggles case), digits, - . _ @ and R for space
        base32,     // A-Z and 2-7 only
        pin,        // A-Z and 0-9, typed characters shown as '*'
    };

    // Overwrites the characters before clearing, so secrets don't linger in memory.
    void wipe_string(bn::string<64>& value);

    // On-screen keyboard. Edits value in place.
    // Returns true when the user presses START, false if they cancel (L, or B with empty text).
    // message, if given, is shown until the next key press (e.g. a validation error).
    bool run_keyboard(text_generators& text, keyboard_layout layout, const char* title,
                      bn::string<64>& value, int max_chars, const char* message = nullptr);
}

#endif
