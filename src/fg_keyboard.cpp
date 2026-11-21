// Field Guide theme: on-screen keyboard (design handoff screens 06, 08 and 09).

#include "theme.h"

#include "bn_string_view.h"

#include "app_text.h"
#include "ui.h"

namespace app::field_guide
{

namespace
{
    // Screen geometry per layout.
    struct layout_info
    {
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

    constexpr layout_info pin_info = { 22, 56, 82, 4 + 2 + 5, 56 + 2 + 4, 24, 18, 22, 16, 144, 147, -1 };
    constexpr layout_info name_info = { 22, 63, 67, 4 + 2 + 9, 63 + 2 + 3, 21, 15, 19, 13, 134, 137, 148 };
    constexpr layout_info base32_info = { 28, 62, 70, 4 + 2 + 4, 62 + 2 + 3, 27, 16, 25, 14, 136, 139, 150 };

    constexpr ui::hint type_hints[] = { { "A", "type" }, { "B", "delete" }, { "START", "done" } };
    constexpr ui::hint name_row_2_hints[] = { { "R", "space" }, { "L", "cancel" } };
    constexpr ui::hint base32_row_2_hints[] = { { "L", "cancel" } };

    constexpr int pin_slots = 16;
    constexpr int pin_slot_size = 9;
    constexpr int pin_slot_gap = 4;

    const layout_info& info(const keyboard_state& state)
    {
        return state.layout == keyboard_layout::pin ? pin_info :
               state.layout == keyboard_layout::base32 ? base32_info : name_info;
    }

    class keyboard_painter_impl final : public keyboard_painter
    {

    public:
        void draw_all(const keyboard_state& state) override
        {
            ui::background();
            ui::header(state.title);
            draw_counter(state);
            draw_input(state);

            if(state.layout == keyboard_layout::name)
            {
                int pill_width = ui::pill(8, 54, "SELECT", ui::white, ui::ink);
                ui::text(ui::font::body, "case", 8 + pill_width + 3, 55, ui::ink);
            }

            draw_keyboard(state);
            draw_footer(state);
        }

        void key_moved(const keyboard_state& state, int old_row, int old_column) override
        {
            draw_key(state, old_row, old_column, false);
            draw_key(state, state.row, state.column, true);
        }

        void case_changed(const keyboard_state& state) override
        {
            draw_keyboard(state);
        }

        void text_changed(const keyboard_state& state) override
        {
            draw_input(state);
            draw_counter(state);
        }

        void caret_changed(const keyboard_state& state) override
        {
            if(state.layout != keyboard_layout::pin)
            {
                draw_input(state);
            }
        }

        void message_changed(const keyboard_state& state) override
        {
            draw_footer(state);
        }

    private:
        static void draw_counter(const keyboard_state& state)
        {
            // Password screens show the count with their 16 dots; the long titles need the room.
            if(state.layout == keyboard_layout::pin)
            {
                return;
            }

            ui::header_band_clear(180, 60);

            bn::string<16> counter;
            append_padded(counter, state.value.size(), 1);
            counter += '/';
            append_padded(counter, state.max_chars, 1);
            ui::text(ui::font::digits, counter, 233, 8, ui::white, ui::no_shadow, 1, ui::align::right);
        }

        static void draw_input(const keyboard_state& state)
        {
            ui::panel(input_x, input_y, input_width, info(state).input_height, ui::white, false);

            switch(state.layout)
            {

            case keyboard_layout::pin:
                draw_pin_slots(state);
                break;

            case keyboard_layout::base32:
                draw_base32_text(state);
                break;

            default:
                draw_name_text(state);
                break;
            }
        }

        static void draw_keyboard(const keyboard_state& state)
        {
            const layout_info& layout = info(state);

            if(state.layout == keyboard_layout::name)
            {
                // Inactive tab first, then the panel over its bottom edge, then the active tab
                // merging into the panel.
                ui::stripes(170, 50, 66, 13);
                draw_tab(! state.lower_case, false);
            }

            ui::panel(panel_x, layout.panel_y, panel_width, layout.panel_height, ui::info, false);

            if(state.layout == keyboard_layout::name)
            {
                draw_tab(state.lower_case, true);
            }

            for(int r = 0; r < keyboard_state::rows; ++r)
            {
                for(int c = 0; c < state.columns(); ++c)
                {
                    if(r != state.row || c != state.column)
                    {
                        draw_key(state, r, c, false);
                    }
                }
            }

            // Last, so the neighbours don't paint over its outline.
            draw_key(state, state.row, state.column, true);
        }

        static void draw_key(const keyboard_state& state, int r, int c, bool selected)
        {
            const layout_info& layout = info(state);
            int x = layout.grid_x + c * layout.pitch_x;
            int y = layout.grid_y + r * layout.pitch_y;
            ui::key_cap(x, y, layout.cap_width, layout.cap_height, state.key(r, c), selected, ui::info);
        }

        static void draw_footer(const keyboard_state& state)
        {
            const layout_info& layout = info(state);
            ui::footer_bar(layout.footer_top);

            if(state.message)
            {
                ui::footer_message(layout.footer_row_1_y, state.message);
            }
            else
            {
                ui::footer_hints(layout.footer_row_1_y, type_hints, 3);
            }

            if(state.layout == keyboard_layout::name)
            {
                ui::footer_hints(layout.footer_row_2_y, name_row_2_hints, 2);
            }
            else if(state.layout == keyboard_layout::base32)
            {
                ui::footer_hints(layout.footer_row_2_y, base32_row_2_hints, 1);
            }
        }

        // Left slot: lower case tab, right slot: upper case tab.
        static void draw_tab(bool lower_case_tab, bool active)
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

        static void draw_pin_slots(const keyboard_state& state)
        {
            int total = pin_slots * pin_slot_size + (pin_slots - 1) * pin_slot_gap;
            int x = input_x + (input_width - total) / 2;
            int y = input_y + (info(state).input_height - pin_slot_size) / 2;

            for(int i = 0; i < pin_slots; ++i)
            {
                int slot_x = x + i * (pin_slot_size + pin_slot_gap);

                if(i < state.value.size())
                {
                    ui::round_fill(slot_x, y, 8, 8, 4, ui::ink);
                }
                else
                {
                    ui::fill(slot_x + 3, y + 3, 3, 3, ui::bevel);
                }
            }
        }

        static void draw_name_text(const keyboard_state& state)
        {
            bn::string_view text(state.value.data(), state.value.size());

            // 2x like the design; very wide names (e.g. 16 x "M") fall back to 1x so they fit.
            int scale = ui::text_width(ui::font::body, text, 2) + 12 <= input_width - 12 ? 2 : 1;
            int x = input_x + 6;
            int y = input_y + (info(state).input_height - 8 * scale) / 2;
            ui::text(ui::font::body, text, x, y, ui::ink, ui::input_shadow, scale);

            if(state.caret_visible)
            {
                int width = ui::text_width(ui::font::body, text, scale);
                ui::fill(x + width + (state.value.empty() ? 0 : 1 + scale), y + 1, 2, 8 * scale - 2, ui::red);
            }
        }

        static void draw_base32_text(const keyboard_state& state)
        {
            constexpr int chars_per_line = 32;
            constexpr int text_x = input_x + 5;
            const int line_y[2] = { input_y + 2, input_y + 13 };
            int size = state.value.size();

            for(int line = 0; line < 2; ++line)
            {
                ui::dotted_hline(input_x + 4, input_y + 11 + line * 11, input_width - 8, ui::light_shadow);

                int start = line * chars_per_line;

                if(size > start)
                {
                    int count = size - start;

                    if(count > chars_per_line)
                    {
                        count = chars_per_line;
                    }

                    ui::text(ui::font::digits, bn::string_view(state.value.data() + start, count), text_x,
                             line_y[line], ui::ink);
                }
            }

            if(state.caret_visible && size < state.max_chars)
            {
                int line = size / chars_per_line;
                int caret_column = size % chars_per_line;
                ui::fill(text_x + caret_column * 7 - 1, line_y[line], 2, 8, ui::red);
            }
        }
    };

    keyboard_painter_impl painter;
}

keyboard_painter& keyboard()
{
    return painter;
}

}
