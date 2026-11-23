// Net Link theme (handoff design_handoff_theme_net_link): blue grid, cyan tabs and slanted pills,
// chips with a badge-color strip, green selection and cursor, faded modal backdrop.

#include "skin.h"

#include "bn_string.h"

#include "app_text.h"

namespace app::net_link
{

namespace
{
    using app::skin::text_colors;
    using app::skin::row_info;
    using app::skin::menu_item_info;
    using namespace app::skin;

    constexpr uint8_t text_white = ui::net_text;
    constexpr uint8_t deep = ui::net_deep;
    constexpr uint8_t grid_bg = ui::net_bg;
    constexpr uint8_t grid_line = ui::net_grid;
    constexpr uint8_t chip_dark = ui::net_chip_dark;
    constexpr uint8_t chip = ui::net_chip;
    constexpr uint8_t chip_highlight = ui::net_chip_hi;
    constexpr uint8_t chip_selected = ui::net_selected;
    constexpr uint8_t cyan = ui::net_cyan;
    constexpr uint8_t shine = ui::net_shine;
    constexpr uint8_t green = ui::net_green;
    constexpr uint8_t secondary = ui::net_secondary;
    constexpr uint8_t tag_ok = ui::net_tag;
    constexpr uint8_t tag_warn = ui::net_warn;
    constexpr uint8_t danger = ui::net_danger;
    constexpr uint8_t danger_text = ui::net_danger_text;

    constexpr text_colors net_colors = {
        text_white,     // text
        secondary,      // dim
        text_white,     // box
        secondary,      // box_dim
        danger_text,    // danger
        text_white,     // hint
        text_white,     // input
        green,          // caret
        grid_line,      // guide
    };

    constexpr int tab_title_max_width = 172;   // the counter sits right of the tab

    // Gauge: frame with rounded ends, 1 px padding, fill with a 2 px shine on top.
    void gauge(int x, int y, int width, int fill_x, int fill_width, uint8_t fill, uint8_t fill_shine)
    {
        ui::round_fill(x, y, width, 9, 4, cyan);
        ui::round_fill(x + 1, y + 1, width - 2, 7, 3, deep);

        if(fill_width > 0)
        {
            ui::round_fill(fill_x, y + 2, fill_width, 5, 2, fill);

            if(fill_width > 2)
            {
                ui::fill(fill_x + 1, y + 2, fill_width - 2, 2, fill_shine);
            }
        }
    }

    // Title tab text: title12, or the body font when it's too long.
    void tab_title(const bn::string_view& title, int x, int y, uint8_t color)
    {
        if(ui::text_width(ui::font::title12, title) <= tab_title_max_width)
        {
            ui::text(ui::font::title12, title, x, y, color);
        }
        else
        {
            ui::text(ui::font::body, fitted(ui::font::body, title, tab_title_max_width), x, y + 3, color);
        }
    }

    int tab_title_width(const bn::string_view& title)
    {
        int width = ui::text_width(ui::font::title12, title);
        return width <= tab_title_max_width ? width :
               ui::text_width(ui::font::body, fitted(ui::font::body, title, tab_title_max_width));
    }

    class net_skin final : public app::skin::skin
    {

    public:
        constexpr net_skin() :
            skin(net_colors)
        {
        }

        // #082050 with a 1 px grid line every 16 px both ways (fixed to the screen).
        void background(int x, int y, int width, int height) const override
        {
            ui::fill(x, y, width, height, grid_bg);

            for(int line_x = (x + 15) & ~15; line_x < x + width; line_x += 16)
            {
                ui::fill(line_x, y, 1, height, grid_line);
            }

            for(int line_y = (y + 15) & ~15; line_y < y + height; line_y += 16)
            {
                ui::fill(x, line_y, width, 1, grid_line);
            }
        }

        void modal_backdrop(int x, int y, int width, int height) const override
        {
            ui::fade(x, y, width, height, ui::fade_style::net_link);
        }

        // Deep band with a cyan title tab (slanted right end).
        void header(const bn::string_view& title, bool) const override
        {
            ui::fill(0, 0, 240, 22, deep);
            ui::fill(0, 22, 240, 1, cyan);
            parallelogram(0, 3, 6 + tab_title_width(title) + 16, 16, 8, cyan, true);
            tab_title(title, 6, 5, deep);
        }

        void header_clock(const bn::string_view& time, const bn::string_view& date) const override
        {
            ui::fill(112, 0, 128, 22, deep);
            ui::text(ui::font::digits, time, 233, 3, text_white, ui::no_shadow, 1, ui::align::right);
            ui::text(ui::font::digits6, date, 233, 12, secondary, ui::no_shadow, 1, ui::align::right);
        }

        void header_counter(const bn::string_view& counter) const override
        {
            ui::fill(180, 0, 60, 22, deep);
            ui::text(ui::font::digits, counter, 233, 7, text_white, ui::no_shadow, 1, ui::align::right);
        }

        void time_bar(int remaining, bool) const override
        {
            background(0, 24, 240, 17);

            bool low = remaining <= 5;
            gauge(4, 28, 196, 6, 192 * remaining / home_state::totp_period, low ? danger : cyan,
                  low ? danger_text : shine);

            // Seconds tag: orange, yellow from 10 s, red from 5 s.
            parallelogram(204, 27, 34, 11, 4, low ? danger : remaining <= 10 ? tag_warn : tag_ok);

            bn::string<8> seconds;
            append_padded(seconds, remaining, 1);
            seconds += 's';
            ui::text(ui::font::digits, seconds, 221, 29, deep, ui::no_shadow, 1, ui::align::center);
        }

        bool blinks_low_time() const override
        {
            return false;
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
            uint8_t badge = ui::badge_color(info.item.label);

            if(info.selected)
            {
                // Shifted 6 px right: green border, 5 px badge strip, code slot with the big code.
                int x = info.x + 6;
                int width = info.width - 6;
                ui::round_fill(x, info.y, width, selected_row_height, 2, green);
                ui::fill(x + 2, info.y + 2, width - 4, selected_row_height - 4, chip_selected);
                ui::fill(x + 5, info.y + 2, width - 7, 1, cyan);
                ui::fill(x, info.y + 1, 5, selected_row_height - 2, badge);
                ui::fill(x + 1, info.y, 4, selected_row_height, badge);

                int slot_x = x + width - 2 - 1 - 86;
                ui::round_fill(slot_x, info.y + 3, 86, 17, 2, deep);
                ui::text(ui::font::code12, info.code, slot_x + 43, info.y + 5, green, ui::no_shadow, 1,
                         ui::align::center);

                int name_x = x + 5 + 4;
                ui::text(ui::font::body, fitted(ui::font::body, info.item.label, slot_x - 4 - name_x), name_x,
                         info.y + 8, text_white);
                row_cursor(info.y, info.bob, info.moving);
            }
            else
            {
                ui::round_fill(info.x, info.y, info.width, row_height, 2, chip);
                ui::fill(info.x + 2, info.y, info.width - 4, 1, chip_highlight);
                ui::fill(info.x, info.y + 1, 4, row_height - 2, badge);
                ui::fill(info.x + 1, info.y, 3, row_height, badge);

                int slot_x = info.x + info.width - 2 - 54;
                ui::round_fill(slot_x, info.y + 2, 54, 11, 2, deep);
                ui::text(ui::font::digits, info.code, slot_x + 27, info.y + 4, cyan, ui::no_shadow, 1,
                         ui::align::center);

                int name_x = info.x + 4 + 5;
                ui::text(ui::font::body, fitted(ui::font::body, info.item.label, slot_x - 4 - name_x), name_x,
                         info.y + 4, text_white);
            }
        }

        bool has_cursor() const override
        {
            return true;
        }

        // Green triangle left of the selected chip (up / down arrows while moving).
        void row_cursor(int y, int bob, bool moving) const override
        {
            background(4, y, 9, selected_row_height);

            if(moving)
            {
                ui::cursor_up(5, y + 5, green);
                ui::cursor_down(5, y + 15, green);
            }
            else
            {
                ui::cursor_right(5 + bob, y + 8, green);
            }
        }

        void scrollbar(int first, int visible, int total) const override
        {
            ui::round_fill(227, scrollbar_top, 4, scrollbar_height, 2, deep);

            int thumb_y, thumb_height;
            scroll_thumb(first, visible, total, thumb_y, thumb_height);
            ui::round_fill(227, thumb_y, 4, thumb_height, 2, cyan);
        }

        void footer_bar(int top) const override
        {
            ui::fill(0, top - 1, 240, 1, cyan);
            ui::fill(0, top, 240, 160 - top, deep);
        }

        int pill_width(const char* label) const override
        {
            return ui::text_width(ui::font::digits, label) + 10;
        }

        // Slanted (3 px) cyan pill, red for danger.
        int pill(int x, int label_y, const char* label, bool is_danger) const override
        {
            int width = pill_width(label);
            parallelogram(x, label_y - 2, width, 11, 3, is_danger ? danger : cyan);
            ui::text(ui::font::digits, label, x + 5, label_y, deep);
            return width;
        }

        int box_border() const override
        {
            return 2;
        }

        uint8_t box_fill() const override
        {
            return chip;
        }

        // Cyan border, 1 px deep outline outside it, 3 px deep hard shadow.
        void box(int x, int y, int width, int height) const override
        {
            ui::round_fill(x + 3, y + 3, width, height, 3, deep);
            ui::round_fill(x - 1, y - 1, width + 2, height + 2, 4, deep);
            ui::round_fill(x, y, width, height, 3, cyan);
            ui::round_fill(x + 2, y + 2, width - 4, height - 4, 1, chip);
        }

        int menu_inset() const override
        {
            return 14;
        }

        int menu_right_inset() const override
        {
            return 6;
        }

        void menu_item(const menu_item_info& info) const override
        {
            int x = info.box_x + 14;
            int width = info.box_width - 20;
            ui::fill(info.box_x + 2, info.y, info.box_width - 4, 15, chip);

            if(info.selected)
            {
                ui::round_fill(x, info.y, width, 15, 2, green);
                ui::round_fill(x + 1, info.y + 1, width - 2, 13, 1, chip_selected);
                ui::cursor_right(info.box_x + 2 + 4 + info.bob, info.y + 4, green);
            }
            else
            {
                ui::round_fill(x, info.y, width, 15, 2, chip_dark);
            }

            int text_x = x + 4;

            if(info.danger)
            {
                ui::fill(x, info.y + 1, 3, 13, danger);
                ui::fill(x + 1, info.y, 2, 15, danger);
                text_x += 3;
            }

            ui::text(ui::font::body, info.label, text_x, info.y + 4, info.danger ? danger_text : text_white);

            if(info.value)
            {
                int pill_width = ui::text_width(ui::font::digits, info.value) + 8;
                int pill_x = x + width - 4 - pill_width;
                ui::round_fill(pill_x, info.y + 2, pill_width, 11, 2, deep);
                ui::text(ui::font::digits, info.value, pill_x + 4, info.y + 4, cyan);
            }
        }

        // Tighter than the other skins: every item is a filled chip.
        int settings_top_padding() const override
        {
            return 3;
        }

        int settings_bottom_padding() const override
        {
            return 3;
        }

        void info_box(const bn::string_view& text) const override
        {
            ui::round_fill(4, 28, 232, 18, 3, cyan);
            ui::round_fill(5, 29, 230, 16, 2, deep);
            ui::round_fill(10, 33, 7, 7, 3, green);
            ui::text(ui::font::body, text, 22, 33, text_white);
        }

        void busy(const bn::string_view& title, const bn::string_view& subtitle) const override
        {
            constexpr int x = 24, y = 70;     // below the 32x32 logo
            box(x, y, 192, 62);
            ui::text(ui::font::title12, title, 120, y + 2 + 7, text_white, ui::no_shadow, 1, ui::align::center);

            // Static block (30 % wide, 40 % in): the CPU is busy while this is shown.
            int bar_x = x + 2 + 14;
            gauge(bar_x, y + 2 + 28, 160, bar_x + 2 + 156 * 4 / 10, 156 * 3 / 10, cyan, shine);
            ui::text(ui::font::body, subtitle, 120, y + 2 + 44, secondary, ui::no_shadow, 1, ui::align::center);
        }

        void welcome_band() const override
        {
            ui::fill(0, 0, 240, 76, deep);
            ui::fill(0, 76, 240, 1, cyan);
            ui::text(ui::font::body, "Welcome to", 10, 13, secondary);
            parallelogram(0, 26, 176, 36, 14, cyan, true);
            ui::text(ui::font::body, "AuthBoy", 10, 33, deep, ui::no_shadow, 3);
            ui::logo(ui::logo_frame::big, 192, 28, logo_colors());     // on the band right of the tab
        }

        const uint8_t* logo_colors() const override
        {
            static constexpr uint8_t logo[7] = { 0, deep, secondary, chip, chip_dark, cyan, green };
            return logo;
        }

        void clock_info(const bn::string_view& text, bool running) const override
        {
            ui::round_fill(4, 28, 232, 18, 3, cyan);
            ui::round_fill(5, 29, 230, 16, 2, deep);
            ui::round_fill(9, 34, 7, 7, 3, running ? green : tag_ok);
            ui::text(ui::font::body, text, 21, 33, text_white);
        }

        // Cyan border, 1 px deep outline (no shadow: the footer is right below).
        void clock_panel(int x, int y, int width, int height) const override
        {
            ui::round_fill(x - 1, y - 1, width + 2, height + 2, 4, deep);
            ui::round_fill(x, y, width, height, 3, cyan);
            ui::round_fill(x + 2, y + 2, width - 4, height - 4, 1, chip);
        }

        uint8_t clock_cell(int x, int y, int width, int height, bool selected) const override
        {
            if(selected)
            {
                ui::round_fill(x - 1, y - 1, width + 2, height + 2, 2, text_white);
                ui::round_fill(x, y, width, height, 1, green);
                return deep;
            }

            ui::round_fill(x, y, width, height, 1, deep);
            return cyan;
        }

        uint8_t clock_arrow_color() const override
        {
            return green;
        }

        void clock_tag(bool running, bool) const override
        {
            const char* label = running ? "RUNNING" : "PAUSED";
            int width = ui::text_width(ui::font::digits6, label) + 12;
            ui::fill(150, 0, 90, 22, deep);
            parallelogram(234 - width, 6, width, 11, 3, running ? green : tag_ok);
            ui::text(ui::font::digits6, label, 234 - width + 6, 8, deep);
        }

        void input_box(int x, int y, int width, int height) const override
        {
            ui::round_fill(x, y, width, height, 3, cyan);
            ui::round_fill(x + 1, y + 1, width - 2, height - 2, 2, deep);
        }

        void pin_slot(int x, int y, bool filled) const override
        {
            if(filled)
            {
                ui::round_fill(x, y, 8, 8, 4, green);
            }
            else
            {
                ui::fill(x + 3, y + 3, 3, 3, chip_highlight);
            }
        }

        // 2x text like the design's 16 px font; very wide names fall back to 1x.
        void name_input(int x, int y, int width, int height, const bn::string_view& text,
                        bool caret_visible) const override
        {
            int scale = ui::text_width(ui::font::body, text, 2) + 12 + 8 <= width - 12 ? 2 : 1;
            int text_x = x + 6;
            int text_y = y + (height - 7 * scale) / 2;
            ui::text(ui::font::body, text, text_x, text_y, text_white, ui::no_shadow, scale);

            if(caret_visible)
            {
                int text_width = ui::text_width(ui::font::body, text, scale);
                ui::fill(text_x + text_width + (text.empty() ? 0 : 1 + scale), y + (height - 14) / 2, 7, 14, green);
            }
        }

        void key_panel(int x, int y, int width, int height) const override
        {
            ui::round_fill(x, y, width, height, 3, cyan);
            ui::round_fill(x + 1, y + 1, width - 2, height - 2, 2, chip);
        }

        void key(int x, int y, int width, int height, char label, bool selected) const override
        {
            ui::fill(x - 2, y - 2, width + 4, height + 4, chip);

            if(selected)
            {
                ui::round_fill(x, y, width, height, 2, text_white);
                ui::round_fill(x + 1, y + 1, width - 2, height - 2, 1, green);
            }
            else
            {
                ui::round_fill(x, y, width, height, 2, chip_dark);
                ui::fill(x + 2, y, width - 4, 1, chip_highlight);
            }

            char text[2] = { label, 0 };
            ui::text(ui::font::digits, text, x + width / 2, y + (height - 7) / 2, selected ? deep : text_white,
                     ui::no_shadow, 1, ui::align::center);
        }

        void case_tabs(bool lower_case) const override
        {
            constexpr int tab_width = 32;
            constexpr int lower_x = 236 - tab_width;
            constexpr int upper_x = lower_x - 2 - tab_width;
            background(upper_x - 2, 52, 236 - upper_x + 2, 11);

            for(int i = 0; i < 2; ++i)
            {
                bool lower_tab = i == 1;
                int x = lower_tab ? lower_x : upper_x;
                bool active = lower_tab == lower_case;
                parallelogram(x, 53, tab_width, 10, 3, active ? green : chip_dark);
                ui::text(ui::font::digits, lower_tab ? "abc" : "ABC", x + tab_width / 2, 55, active ? deep : secondary,
                         ui::no_shadow, 1, ui::align::center);
            }
        }
    };

    constexpr net_skin look;
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
