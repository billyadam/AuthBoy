#include "keyboard.h"

#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_vector.h"
#include "bn_sprite_ptr.h"
#include "bn_string_view.h"

#include "bn_sprite_items_key_cursor.h"

#include "entropy.h"

namespace app
{

namespace
{
    // Every layout is a full grid so rows line up.
    constexpr const char* name_upper_rows[] = { "ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ0123", "456789-._@" };
    constexpr const char* name_lower_rows[] = { "abcdefghij", "klmnopqrst", "uvwxyz0123", "456789-._@" };
    constexpr const char* base32_rows[] = { "ABCDEFGH", "IJKLMNOP", "QRSTUVWX", "YZ234567" };
    constexpr const char* pin_rows[] = { "ABCDEFGHI", "JKLMNOPQR", "STUVWXYZ0", "123456789" };

    constexpr int name_columns = 10;
    constexpr int base32_columns = 8;
    constexpr int pin_columns = 9;

    constexpr int row_count = 4;
    constexpr int cell_width = 16;      // one 8px character + one 8px gap in the mono font
    constexpr int first_row_y = -26;
    constexpr int row_spacing = 18;
    constexpr int input_y = -48;
    constexpr int visible_input_chars = 26;

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

bool run_keyboard(text_generators& text, keyboard_layout layout, const char* title,
                  bn::string<64>& value, int max_chars, const char* message)
{
    bool lower_case = false;
    int column = 0;
    int row = 0;
    int up_frames = 0, down_frames = 0, left_frames = 0, right_frames = 0;
    bool redraw = true;
    const char* shown_message = message;

    bn::vector<bn::sprite_ptr, 96> sprites;
    bn::sprite_ptr cursor = bn::sprite_items::key_cursor.create_sprite(0, 0);
    cursor.set_z_order(1);      // behind the key text

    // Let one frame pass so the button press that opened this screen isn't seen again.
    frame_update();

    while(true)
    {
        const char* const* rows;
        int column_count;

        switch(layout)
        {

        case keyboard_layout::base32:
            rows = base32_rows;
            column_count = base32_columns;
            break;

        case keyboard_layout::pin:
            rows = pin_rows;
            column_count = pin_columns;
            break;

        default:
            rows = lower_case ? name_lower_rows : name_upper_rows;
            column_count = name_columns;
            break;
        }

        if(column >= column_count)
        {
            column = column_count - 1;
        }
        bool key_pressed = false;

        if(repeat(bn::keypad::left_held(), left_frames))
        {
            column = wrap(column - 1, column_count);
            key_pressed = true;
        }

        if(repeat(bn::keypad::right_held(), right_frames))
        {
            column = wrap(column + 1, column_count);
            key_pressed = true;
        }

        if(repeat(bn::keypad::up_held(), up_frames))
        {
            row = wrap(row - 1, row_count);
            key_pressed = true;
        }

        if(repeat(bn::keypad::down_held(), down_frames))
        {
            row = wrap(row + 1, row_count);
            key_pressed = true;
        }

        if(bn::keypad::a_pressed())
        {
            if(value.size() < max_chars)
            {
                value.push_back(rows[row][column]);
            }

            key_pressed = true;
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
            key_pressed = true;
        }

        if(layout == keyboard_layout::name)
        {
            if(bn::keypad::select_pressed())
            {
                lower_case = ! lower_case;
                key_pressed = true;
            }

            if(bn::keypad::r_pressed() && value.size() < max_chars && ! value.empty())
            {
                value.push_back(' ');
                key_pressed = true;
            }
        }

        if(bn::keypad::start_pressed())
        {
            return true;
        }

        if(key_pressed)
        {
            shown_message = nullptr;
            redraw = true;
        }

        if(redraw)
        {
            redraw = false;
            sprites.clear();

            int grid_width = column_count * cell_width - (cell_width / 2);
            int grid_left = -grid_width / 2;

            // Title with character count
            bn::string<48> title_text = title;
            title_text += "  ";
            append_padded(title_text, value.size(), 1);
            title_text += '/';
            append_padded(title_text, max_chars, 1);
            text.small.generate(0, -68, title_text, sprites);

            // Typed text (only the end if it's long) with a trailing cursor
            bn::string<32> input_text;
            int start = value.size() > visible_input_chars ? value.size() - visible_input_chars : 0;

            if(start > 0)
            {
                input_text += "..";
                start += 2;
            }

            if(layout == keyboard_layout::pin)
            {
                for(int i = start; i < value.size(); ++i)
                {
                    input_text += '*';
                }
            }
            else
            {
                input_text += bn::string_view(value.data() + start, value.size() - start);
            }

            input_text += '_';
            text.mono.generate(0, input_y, input_text, sprites);

            // Key grid: characters separated by spaces so each key is a 16px cell
            text.mono.set_left_alignment();

            for(int r = 0; r < row_count; ++r)
            {
                bn::string<32> row_text;

                for(int c = 0; c < column_count; ++c)
                {
                    if(c)
                    {
                        row_text += ' ';
                    }

                    row_text += rows[r][c];
                }

                text.mono.generate(grid_left, first_row_y + r * row_spacing, row_text, sprites);
            }

            text.mono.set_center_alignment();
            cursor.set_position(grid_left + column * cell_width + 4, first_row_y + row * row_spacing);

            // Help / message lines
            if(shown_message)
            {
                text.small.generate(0, 52, shown_message, sprites);
            }
            else
            {
                text.small.generate(0, 52, "A: type   B: delete   START: done", sprites);
            }

            if(layout == keyboard_layout::name)
            {
                text.small.generate(0, 70, "SELECT: abc/ABC   R: space   L: cancel", sprites);
            }
            else if(layout == keyboard_layout::base32)
            {
                text.small.generate(0, 70, "L: cancel", sprites);
            }
        }

        frame_update();
    }
}

}
