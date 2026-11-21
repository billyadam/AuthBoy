#include "keyboard.h"

#include "bn_keypad.h"

#include "entropy.h"

namespace app
{

namespace
{
    constexpr int blink_frames = 30;

    // One step on press, then repeats while held.
    bool repeat(bool held, int& held_frames)
    {
        if(! held)
        {
            held_frames = 0;
            return false;
        }

        ++held_frames;
        return held_frames == 1 || (held_frames > 18 && (held_frames - 18) % 4 == 0);
    }

    int wrap(int value, int count)
    {
        return value < 0 ? count - 1 : value >= count ? 0 : value;
    }
}

void wipe_string(bn::string<64>& value)
{
    for(int i = 0; i < value.size(); ++i)
    {
        volatile char* c = &value[i];
        *c = 0;
    }

    value.clear();
}

bool run_keyboard(keyboard_layout layout, const char* title, bn::string<64>& value, int max_chars,
                  const char* message)
{
    keyboard_painter& painter = painters().keyboard;
    keyboard_state state(layout, title, value, max_chars);
    state.message = message;

    int up_frames = 0, down_frames = 0, left_frames = 0, right_frames = 0;
    int blink_counter = 0;
    int columns = state.columns();

    painter.draw_all(state);

    // Let one frame pass so the button press that opened this screen isn't seen again.
    frame_update();

    while(true)
    {
        int old_row = state.row;
        int old_column = state.column;
        bool text_changed = false;
        bool any_key = false;

        if(repeat(bn::keypad::left_held(), left_frames))
        {
            state.column = wrap(state.column - 1, columns);
        }

        if(repeat(bn::keypad::right_held(), right_frames))
        {
            state.column = wrap(state.column + 1, columns);
        }

        if(repeat(bn::keypad::up_held(), up_frames))
        {
            state.row = wrap(state.row - 1, keyboard_state::rows);
        }

        if(repeat(bn::keypad::down_held(), down_frames))
        {
            state.row = wrap(state.row + 1, keyboard_state::rows);
        }

        if(bn::keypad::a_pressed())
        {
            if(value.size() < max_chars)
            {
                value.push_back(state.selected_key());
                text_changed = true;
            }

            any_key = true;
        }

        if(bn::keypad::l_pressed())
        {
            return false;
        }

        if(bn::keypad::b_pressed())
        {
            if(value.empty())
            {
                return false;
            }

            value.pop_back();
            text_changed = true;
        }

        if(layout == keyboard_layout::name)
        {
            if(bn::keypad::select_pressed())
            {
                state.lower_case = ! state.lower_case;
                painter.case_changed(state);
                any_key = true;
            }

            if(bn::keypad::r_pressed() && value.size() < max_chars && ! value.empty())
            {
                value.push_back(' ');
                text_changed = true;
            }
        }

        if(bn::keypad::start_pressed())
        {
            return true;
        }

        if(state.row != old_row || state.column != old_column)
        {
            painter.key_moved(state, old_row, old_column);
            any_key = true;
        }

        if(text_changed)
        {
            blink_counter = 0;
            state.caret_visible = true;
            painter.text_changed(state);
            any_key = true;
        }

        if(any_key && state.message)
        {
            state.message = nullptr;
            painter.message_changed(state);
        }

        // Caret: visible 30 frames, hidden 30.
        ++blink_counter;
        bool caret_visible = (blink_counter / blink_frames) % 2 == 0;

        if(caret_visible != state.caret_visible)
        {
            state.caret_visible = caret_visible;
            painter.caret_changed(state);
        }

        frame_update();
    }
}

}
