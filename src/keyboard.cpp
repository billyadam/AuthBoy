#include "keyboard.h"

#include "bn_keypad.h"
#include "bn_string_view.h"

#include "app_text.h"
#include "entropy.h"
#include "ui.h"

namespace app
{

namespace
{
    // Every layout is a full grid so rows line up.
    constexpr const char* name_upper_rows[] = { "ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ0123", "456789-._@" };
    constexpr const char* name_lower_rows[] = { "abcdefghij", "klmnopqrst", "uvwxyz0123", "456789-._@" };
    constexpr const char* base32_rows[] = { "ABCDEFGH", "IJKLMNOP", "QRSTUVWX", "YZ234567" };
    constexpr const char* pin_rows[] = { "ABCDEFGHI", "JKLMNOPQR", "STUVWXYZ0", "123456789" };

    constexpr int row_count = 4;

    // Screen geometry per layout, from the design handoff (screens 06, 08 and 09).
    struct layout_info
    {
        int columns;
        int input_height;
        int panel_y;
        int panel_height;
        int grid_x;         // top-left of the first key cap
        int grid_y;
        int pitch_x;
        int pitch_y;
        int cap_width;
        int cap_height;
        int footer_top;
        int footer_row_1_y;
        int footer_row_2_y; // -1: one-row footer
    };

    constexpr int input_x = 4;
    constexpr int input_y = 28;
    constexpr int input_width = 232;
    constexpr int panel_x = 4;
    constexpr int panel_width = 232;

    constexpr layout_info pin_info = { 9, 22, 56, 82, 4 + 2 + 5, 56 + 2 + 4, 24, 18, 22, 16, 144, 147, -1 };
    constexpr layout_info name_info = { 10, 22, 63, 67, 4 + 2 + 9, 63 + 2 + 3, 21, 15, 19, 13, 134, 137, 148 };
    constexpr layout_info base32_info = { 8, 28, 62, 70, 4 + 2 + 4, 62 + 2 + 3, 27, 16, 25, 14, 136, 139, 150 };

    constexpr ui::hint type_hints[] = { { "A", "type" }, { "B", "delete" }, { "START", "done" } };
    constexpr ui::hint name_row_2_hints[] = { { "R", "space" }, { "L", "cancel" } };
    constexpr ui::hint base32_row_2_hints[] = { { "L", "cancel" } };

    constexpr int pin_slots = 16;
    constexpr int pin_slot_size = 9;
    constexpr int pin_slot_gap = 4;

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

    class keyboard_screen
    {

    public:
        keyboard_screen(keyboard_layout layout, const char* title, bn::string<64>& value, int max_chars) :
            _layout(layout),
            _info(layout == keyboard_layout::pin ? pin_info :
                  layout == keyboard_layout::base32 ? base32_info : name_info),
            _title(title),
            _value(value),
            _max_chars(max_chars)
        {
        }

        const char* const* rows() const
        {
            switch(_layout)
            {

            case keyboard_layout::base32:
                return base32_rows;

            case keyboard_layout::pin:
                return pin_rows;

            default:
                return lower_case ? name_lower_rows : name_upper_rows;
            }
        }

        char selected_key() const
        {
            return rows()[row][column];
        }

        void draw_all()
        {
            ui::background();
            ui::header(_title);
            draw_counter();
            draw_input();

            if(_layout == keyboard_layout::name)
            {
                int pill_width = ui::pill(8, 54, "SELECT", ui::white, ui::ink);
                ui::text(ui::font::body, "case", 8 + pill_width + 3, 55, ui::ink);
            }

            draw_keyboard();
            draw_footer();
        }

        void draw_counter()
        {
            // Password screens show the count with their 16 dots; the long titles need the room.
            if(_layout == keyboard_layout::pin)
            {
                return;
            }

            ui::header_band_clear(180, 60);

            bn::string<16> counter;
            append_padded(counter, _value.size(), 1);
            counter += '/';
            append_padded(counter, _max_chars, 1);
            ui::text(ui::font::digits, counter, 233, 8, ui::white, ui::no_shadow, 1, ui::align::right);
        }

        void draw_input()
        {
            ui::panel(input_x, input_y, input_width, _info.input_height, ui::white, false);

            switch(_layout)
            {

            case keyboard_layout::pin:
                draw_pin_slots();
                break;

            case keyboard_layout::base32:
                draw_base32_text();
                break;

            default:
                draw_name_text();
                break;
            }
        }

        void draw_keyboard()
        {
            if(_layout == keyboard_layout::name)
            {
                // Inactive tab first, then the panel over its bottom edge, then the active tab
                // merging into the panel.
                ui::stripes(170, 50, 66, 13);
                draw_tab(! lower_case, false);
            }

            ui::panel(panel_x, _info.panel_y, panel_width, _info.panel_height, ui::info, false);

            if(_layout == keyboard_layout::name)
            {
                draw_tab(lower_case, true);
            }

            for(int r = 0; r < row_count; ++r)
            {
                for(int c = 0; c < _info.columns; ++c)
                {
                    if(r != row || c != column)
                    {
                        draw_key(r, c, false);
                    }
                }
            }

            // Last, so the neighbours don't paint over its outline.
            draw_key(row, column, true);
        }

        void draw_key(int r, int c, bool selected)
        {
            int x = _info.grid_x + c * _info.pitch_x;
            int y = _info.grid_y + r * _info.pitch_y;
            ui::key_cap(x, y, _info.cap_width, _info.cap_height, rows()[r][c], selected, ui::info);
        }

        void draw_footer(const char* message = nullptr)
        {
            ui::footer_bar(_info.footer_top);

            if(message)
            {
                ui::footer_message(_info.footer_row_1_y, message);
            }
            else
            {
                ui::footer_hints(_info.footer_row_1_y, type_hints, 3);
            }

            if(_layout == keyboard_layout::name)
            {
                ui::footer_hints(_info.footer_row_2_y, name_row_2_hints, 2);
            }
            else if(_layout == keyboard_layout::base32)
            {
                ui::footer_hints(_info.footer_row_2_y, base32_row_2_hints, 1);
            }
        }

        void set_caret_visible(bool visible)
        {
            if(caret_visible != visible)
            {
                caret_visible = visible;

                if(_layout != keyboard_layout::pin)
                {
                    draw_input();
                }
            }
        }

        bool lower_case = false;
        int column = 0;
        int row = 0;
        bool caret_visible = true;

    private:
        keyboard_layout _layout;
        const layout_info& _info;
        const char* _title;
        bn::string<64>& _value;
        int _max_chars;

        // Left slot: lower case tab, right slot: upper case tab.
        void draw_tab(bool lower_case_tab, bool active)
        {
            const char* label = lower_case_tab ? "abc" : "ABC";
            int slot_x = lower_case_tab ? 177 : 204;

            if(active)
            {
                ui::round_fill(slot_x, 53, 26, 12, 3, ui::ink);
                ui::fill(slot_x + 2, 55, 22, 10, ui::info);
                ui::text(ui::font::digits, label, slot_x + 13, 56, ui::ink, ui::no_shadow, 1, ui::align::center);
            }
            else
            {
                ui::round_fill(slot_x + 1, 55, 24, 9, 2, ui::tab_inactive);
                ui::text(ui::font::digits, label, slot_x + 13, 56, ui::tab_inactive_text, ui::no_shadow, 1,
                         ui::align::center);
            }
        }

        void draw_pin_slots()
        {
            int total = pin_slots * pin_slot_size + (pin_slots - 1) * pin_slot_gap;
            int x = input_x + (input_width - total) / 2;
            int y = input_y + (_info.input_height - pin_slot_size) / 2;

            for(int i = 0; i < pin_slots; ++i)
            {
                int slot_x = x + i * (pin_slot_size + pin_slot_gap);

                if(i < _value.size())
                {
                    ui::round_fill(slot_x, y, 8, 8, 4, ui::ink);
                }
                else
                {
                    ui::fill(slot_x + 3, y + 3, 3, 3, ui::bevel);
                }
            }
        }

        void draw_name_text()
        {
            bn::string_view text(_value.data(), _value.size());

            // 2x like the design; very wide names (e.g. 16 x "M") fall back to 1x so they fit.
            int scale = ui::text_width(ui::font::body, text, 2) + 12 <= input_width - 12 ? 2 : 1;
            int x = input_x + 6;
            int y = input_y + (_info.input_height - 8 * scale) / 2;
            ui::text(ui::font::body, text, x, y, ui::ink, ui::input_shadow, scale);

            if(caret_visible)
            {
                int width = ui::text_width(ui::font::body, text, scale);
                ui::fill(x + width + (_value.empty() ? 0 : 1 + scale), y + 1, 2, 8 * scale - 2, ui::red);
            }
        }

        void draw_base32_text()
        {
            constexpr int chars_per_line = 32;
            constexpr int text_x = input_x + 5;
            const int line_y[2] = { input_y + 2, input_y + 13 };

            for(int line = 0; line < 2; ++line)
            {
                ui::dotted_hline(input_x + 4, input_y + 11 + line * 11, input_width - 8, ui::light_shadow);

                int start = line * chars_per_line;

                if(_value.size() > start)
                {
                    int count = _value.size() - start;

                    if(count > chars_per_line)
                    {
                        count = chars_per_line;
                    }

                    ui::text(ui::font::digits, bn::string_view(_value.data() + start, count), text_x, line_y[line],
                             ui::ink);
                }
            }

            if(caret_visible && _value.size() < _max_chars)
            {
                int line = _value.size() / chars_per_line;
                int caret_column = _value.size() % chars_per_line;
                ui::fill(text_x + caret_column * 7 - 1, line_y[line], 2, 8, ui::red);
            }
        }
    };
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
    keyboard_screen screen(layout, title, value, max_chars);
    const layout_info& info = layout == keyboard_layout::pin ? pin_info :
                              layout == keyboard_layout::base32 ? base32_info : name_info;
    int up_frames = 0, down_frames = 0, left_frames = 0, right_frames = 0;
    int blink_counter = 0;
    const char* shown_message = message;

    screen.draw_all();

    if(shown_message)
    {
        screen.draw_footer(shown_message);
    }

    // Let one frame pass so the button press that opened this screen isn't seen again.
    frame_update();

    while(true)
    {
        int old_row = screen.row;
        int old_column = screen.column;
        bool text_changed = false;
        bool any_key = false;

        if(repeat(bn::keypad::left_held(), left_frames))
        {
            screen.column = wrap(screen.column - 1, info.columns);
        }

        if(repeat(bn::keypad::right_held(), right_frames))
        {
            screen.column = wrap(screen.column + 1, info.columns);
        }

        if(repeat(bn::keypad::up_held(), up_frames))
        {
            screen.row = wrap(screen.row - 1, row_count);
        }

        if(repeat(bn::keypad::down_held(), down_frames))
        {
            screen.row = wrap(screen.row + 1, row_count);
        }

        if(bn::keypad::a_pressed())
        {
            if(value.size() < max_chars)
            {
                value.push_back(screen.selected_key());
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
                screen.lower_case = ! screen.lower_case;
                screen.draw_keyboard();
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

        if(screen.row != old_row || screen.column != old_column)
        {
            screen.draw_key(old_row, old_column, false);
            screen.draw_key(screen.row, screen.column, true);
            any_key = true;
        }

        if(text_changed)
        {
            blink_counter = 0;
            screen.caret_visible = true;
            screen.draw_input();
            screen.draw_counter();
            any_key = true;
        }

        if(any_key && shown_message)
        {
            shown_message = nullptr;
            screen.draw_footer();
        }

        // Caret: visible 30 frames, hidden 30.
        ++blink_counter;
        screen.set_caret_visible((blink_counter / blink_frames) % 2 == 0);

        frame_update();
    }
}

}
