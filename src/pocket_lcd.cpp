// Pocket LCD theme (handoff design_handoff_theme_pocket_lcd): four shades of green, flat
// drawing, dotted row separators, inverted selection, checkerboard modal backdrop.

#include "skin.h"

#include "bn_string.h"

#include "app_text.h"

namespace app::pocket_lcd
{

namespace
{
    using app::skin::text_colors;
    using app::skin::row_info;
    using app::skin::menu_item_info;
    using namespace app::skin;

    constexpr uint8_t screen = ui::lcd_screen;
    constexpr uint8_t light = ui::lcd_light;
    constexpr uint8_t mid = ui::lcd_mid;
    constexpr uint8_t dark = ui::lcd_dark;

    constexpr text_colors lcd_colors = {
        dark,       // text
        mid,        // dim
        dark,       // box
        mid,        // box_dim
        dark,       // danger (with a trailing "!")
        dark,       // hint
        dark,       // input
        dark,       // caret
        light,      // guide
    };

    // Segmented bars: 4 px on, 2 px off; fills are whole 6 px steps.
    int segment_width(int width, int remaining)
    {
        return (width * remaining / home_state::totp_period + 3) / 6 * 6;
    }

    class lcd_skin final : public app::skin::skin
    {

    public:
        constexpr lcd_skin() :
            skin(lcd_colors)
        {
        }

        void background(int x, int y, int width, int height) const override
        {
            ui::fill(x, y, width, height, screen);
        }

        void modal_backdrop(int x, int y, int width, int height) const override
        {
            ui::checker(x, y, width, height, screen);
        }

        void header(const bn::string_view& title, bool home) const override
        {
            ui::fill(0, 0, 240, 22, dark);
            ui::fill(0, 22, 240, 1, mid);
            ui::text(ui::font::body, fitted(ui::font::body, upper(title), 160), 6, home ? 3 : 7, screen);
        }

        void header_clock(const bn::string_view& time, const bn::string_view& date) const override
        {
            ui::fill(104, 2, 136, 20, dark);
            ui::text(ui::font::digits, time, 233, 3, screen, ui::no_shadow, 1, ui::align::right);
            ui::text(ui::font::digits6, date, 233, 12, light, ui::no_shadow, 1, ui::align::right);
        }

        void header_counter(const bn::string_view& counter) const override
        {
            ui::fill(180, 2, 60, 20, dark);
            ui::text(ui::font::digits, counter, 233, 7, light, ui::no_shadow, 1, ui::align::right);
        }

        void time_bar(int remaining, bool label_on) const override
        {
            background(0, 24, 240, 17);

            // Tag sized to its text (the font is wider than the mockup's); the track fills up to x 204.
            int tag_width = ui::text_width(ui::font::body, "TIME") + 6;
            int track_x = 6 + tag_width + 4;
            int track_width = (204 - track_x) / 6 * 6;
            ui::round_fill(6, 28, tag_width, 10, 2, dark);
            ui::text(ui::font::body, "TIME", 9, 29, screen);
            segments(track_x, 30, track_width, 5, 4, 6, light);
            segments(track_x, 30, segment_width(track_width, remaining), 5, 4, 6, dark);

            bn::string<8> seconds;
            append_padded(seconds, remaining, 1);
            seconds += 's';

            if(remaining > 5)
            {
                ui::text(ui::font::digits, seconds, 234, 29, dark, ui::no_shadow, 1, ui::align::right);
            }
            else if(label_on)
            {
                // Inverted, blinking
                ui::round_fill(214, 27, 22, 12, 2, dark);
                ui::text(ui::font::digits, seconds, 225, 29, screen, ui::no_shadow, 1, ui::align::center);
            }
        }

        bool blinks_low_time() const override
        {
            return true;
        }

        void list_area() const override
        {
            background(list_x, list_y, list_width, list_height);
        }

        void list_band(int y, int height, bool scrolling) const override
        {
            background(4, y, scrolling ? 223 : 232, height);
        }

        void row(const row_info& info) const override
        {
            int right = info.x + info.width;

            if(info.selected)
            {
                // Shifted 3 px left, flipped to dark.
                ui::round_fill(info.x - 3, info.y, info.width + 3, selected_row_height, 2, dark);

                if(info.moving)
                {
                    ui::cursor_up(info.x - 1, info.y + 6, light);
                    ui::cursor_down(info.x - 1, info.y + 14, light);
                }

                int code_width = ui::text_width(ui::font::code12, info.code);
                int name_x = info.x + 6;
                ui::text(ui::font::body, fitted(ui::font::body, info.item.label, right - 5 - code_width - 6 - name_x),
                         name_x, info.y + 8, light);
                ui::text(ui::font::code12, info.code, right - 5, info.y + 5, screen, ui::no_shadow, 1,
                         ui::align::right);
            }
            else
            {
                int code_width = ui::text_width(ui::font::digits, info.code);
                int name_x = info.x + 6;
                ui::text(ui::font::body, fitted(ui::font::body, info.item.label, right - 4 - code_width - 6 - name_x),
                         name_x, info.y + 4, dark);
                ui::text(ui::font::digits, info.code, right - 4, info.y + 4, mid, ui::no_shadow, 1, ui::align::right);
            }
        }

        void row_separator(int x, int y, int width) const override
        {
            ui::pattern_hline(x, y, width, 1, 2, light);
        }

        void scrollbar(int first, int visible, int total) const override
        {
            for(int i = 0; i < scrollbar_height; i += 2)
            {
                ui::fill(229, scrollbar_top + i, 3, 1, light);
            }

            int thumb_y, thumb_height;
            scroll_thumb(first, visible, total, thumb_y, thumb_height);
            ui::round_fill(228, thumb_y, 5, thumb_height, 2, mid);
        }

        void footer_bar(int top) const override
        {
            ui::fill(0, top, 240, 1, light);
        }

        int pill_width(const char* label) const override
        {
            return ui::text_width(ui::font::digits, label) + 6;
        }

        int pill(int x, int label_y, const char* label, bool) const override
        {
            int width = pill_width(label);
            ui::round_fill(x, label_y - 2, width, 11, 2, dark);
            ui::text(ui::font::digits, label, x + 3, label_y, screen);
            return width;
        }

        int box_border() const override
        {
            return 2;
        }

        uint8_t box_fill() const override
        {
            return screen;
        }

        // 2 px dark border, 3 px hard shadow to the bottom right.
        void box(int x, int y, int width, int height) const override
        {
            ui::round_fill(x + 3, y + 3, width, height, 2, dark);
            ui::round_fill(x, y, width, height, 2, dark);
            ui::fill(x + 2, y + 2, width - 4, height - 4, screen);
        }

        int menu_inset() const override
        {
            return 6;
        }

        int menu_right_inset() const override
        {
            return 6;
        }

        void menu_item(const menu_item_info& info) const override
        {
            int x = info.box_x + 6;
            int width = info.box_width - 12;
            uint8_t bar = info.selected ? dark : screen;
            uint8_t text = info.selected ? screen : dark;
            ui::fill(x, info.y, width, 15, screen);
            ui::round_fill(x, info.y, width, 15, 1, bar);
            ui::text(ui::font::body, info.label, x + 4, info.y + 4, text);

            if(info.value)
            {
                // Dark pill, inverted on the selected row
                int pill_width = ui::text_width(ui::font::digits, info.value) + 6;
                int pill_x = x + width - 4 - pill_width;
                ui::round_fill(pill_x, info.y + 2, pill_width, 11, 2, text);
                ui::text(ui::font::digits, info.value, pill_x + 3, info.y + 4, bar);
            }
            else if(info.danger)
            {
                ui::text(ui::font::body, "!", x + width - 4, info.y + 4, text, ui::no_shadow, 1, ui::align::right);
            }
        }

        void info_box(const bn::string_view& text) const override
        {
            ui::round_fill(4, 28, 232, 18, 2, dark);
            ui::round_fill(5, 29, 230, 16, 1, screen);
            ui::fill(10, 34, 6, 6, dark);
            ui::text(ui::font::body, text, 21, 33, dark);
        }

        void busy(const bn::string_view& title, const bn::string_view& subtitle) const override
        {
            constexpr int x = 24, y = 70;     // below the 32x32 logo
            box(x, y, 192, 62);
            ui::text(ui::font::body, upper(title), 120, y + 2 + 9, dark, ui::no_shadow, 1, ui::align::center);

            // Static: the CPU is busy while this is shown.
            segments(x + 2 + 14, y + 2 + 27, 160, 7, 4, 6, light);
            segments(x + 2 + 62, y + 2 + 27, 48, 7, 4, 6, dark);
            ui::text(ui::font::body, subtitle, 120, y + 2 + 44, mid, ui::no_shadow, 1, ui::align::center);
        }

        void welcome_band() const override
        {
            ui::fill(0, 0, 240, 76, dark);
            ui::text(ui::font::body, "Welcome to", 120, 16, light, ui::no_shadow, 1, ui::align::center);
            logo_title_row(*this, "AUTHBOY", 3, screen);
            segments(36, 64, 168, 4, 4, 6, mid);
        }

        const uint8_t* logo_colors() const override
        {
            static constexpr uint8_t logo[7] = { 0, dark, light, mid, dark, light, screen };
            return logo;
        }

        void input_box(int x, int y, int width, int height) const override
        {
            ui::round_fill(x, y, width, height, 2, dark);
            ui::fill(x + 2, y + 2, width - 4, height - 4, screen);
        }

        void pin_slot(int x, int y, bool filled) const override
        {
            if(filled)
            {
                ui::round_fill(x, y, 8, 8, 4, dark);
            }
            else
            {
                ui::fill(x + 3, y + 3, 3, 3, light);
            }
        }

        void name_input(int x, int y, int, int, const bn::string_view& text, bool caret_visible) const override
        {
            ui::pattern_hline(x + 6, y + 14, 128, 7, 8, light);
            ui::text(ui::font::body, text, x + 6, y + 5, dark);

            if(caret_visible)
            {
                int width = ui::text_width(ui::font::body, text);
                ui::fill(x + 6 + width + (text.empty() ? 0 : 1), y + 5, 8, 8, dark);
            }
        }

        void key_panel(int x, int y, int width, int height) const override
        {
            background(x, y, width, height);
        }

        void key(int x, int y, int width, int height, char label, bool selected) const override
        {
            background(x - 2, y - 2, width + 4, height + 4);

            if(selected)
            {
                // Dark cap with a light ring, then a dark ring
                ui::round_fill(x - 2, y - 2, width + 4, height + 4, 3, dark);
                ui::round_fill(x - 1, y - 1, width + 2, height + 2, 2, screen);
            }

            ui::round_fill(x, y, width, height, 2, selected ? dark : light);

            char text[2] = { label, 0 };
            ui::text(ui::font::digits, text, x + width / 2, y + (height - 7) / 2, selected ? screen : dark,
                     ui::no_shadow, 1, ui::align::center);
        }

        void case_tabs(bool lower_case) const override
        {
            constexpr int tab_width = 28;
            constexpr int lower_x = 236 - tab_width;
            constexpr int upper_x = lower_x - 2 - tab_width;
            background(upper_x - 2, 52, 236 - upper_x + 2, 11);

            for(int i = 0; i < 2; ++i)
            {
                bool lower_tab = i == 1;
                int x = lower_tab ? lower_x : upper_x;
                bool active = lower_tab == lower_case;

                if(active)
                {
                    // Top corners rounded
                    ui::round_fill(x, 53, tab_width, 10, 2, dark);
                    ui::fill(x, 58, tab_width, 5, dark);
                }

                ui::text(ui::font::digits, lower_tab ? "abc" : "ABC", x + tab_width / 2, 55, active ? screen : mid,
                         ui::no_shadow, 1, ui::align::center);
            }
        }
    };

    constexpr lcd_skin look;
    skin_home_painter home_impl(look);
    skin_keyboard_painter keyboard_impl(look);
    skin_screen_painter screens_impl(look);
}

app::home_painter& home()
{
    return home_impl;
}

app::keyboard_painter& keyboard()
{
    return keyboard_impl;
}

app::screen_painter& screens()
{
    return screens_impl;
}

}
