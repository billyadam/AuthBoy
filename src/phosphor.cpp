// Phosphor theme (handoff design_handoff_theme_phosphor): amber on black, dashed rules, dot
// leaders between names and codes, double-bordered boxes, faded modal backdrop.

#include "skin.h"

#include "bn_string.h"

#include "app_text.h"

namespace app::phosphor
{

namespace
{
    using app::skin::text_colors;
    using app::skin::row_info;
    using app::skin::menu_item_info;
    using namespace app::skin;

    constexpr uint8_t bg = ui::amber_bg;
    constexpr uint8_t track = ui::amber_track;
    constexpr uint8_t frame = ui::amber_frame;
    constexpr uint8_t dim = ui::amber_dim;
    constexpr uint8_t amber = ui::amber_text;

    constexpr text_colors amber_colors = {
        amber,      // text
        dim,        // dim
        amber,      // box
        dim,        // box_dim
        amber,      // danger (with a trailing "!")
        amber,      // hint
        amber,      // input
        amber,      // caret
        track,      // guide
    };

    // Block bars: 5 px on, 1 px off; fills are whole 6 px steps.
    int segment_width(int width, int remaining)
    {
        return (width * remaining / home_state::totp_period + 3) / 6 * 6;
    }

    void dashed_rule(int y)
    {
        ui::pattern_hline(4, y, 232, 2, 4, frame);
    }

    // Dots every 4 px between two x positions (hidden if they touch).
    void leader(int x0, int x1, int y, uint8_t color)
    {
        if(x1 - x0 > 1)
        {
            ui::pattern_hline(x0, y, x1 - x0, 1, 4, color);
        }
    }

    class amber_skin final : public app::skin::skin
    {

    public:
        constexpr amber_skin() :
            skin(amber_colors)
        {
        }

        void background(int x, int y, int width, int height) const override
        {
            ui::fill(x, y, width, height, bg);
        }

        void modal_backdrop(int x, int y, int width, int height) const override
        {
            ui::fade(x, y, width, height, ui::fade_style::phosphor);
        }

        // No band: "> TITLE" over a dashed rule.
        void header(const bn::string_view& title, bool home) const override
        {
            background(0, 0, 240, 23);

            if(home)
            {
                ui::text(ui::font::body, upper(title), 6, 3, amber);
            }
            else
            {
                bn::string<40> text = "> ";
                text += fitted(ui::font::body, upper(title), 160);
                ui::text(ui::font::body, text, 6, 7, amber);
            }

            dashed_rule(23);
        }

        void header_clock(const bn::string_view& time, const bn::string_view& date) const override
        {
            background(104, 0, 136, 22);
            ui::text(ui::font::digits, time, 234, 3, amber, ui::no_shadow, 1, ui::align::right);
            ui::text(ui::font::digits6, date, 234, 13, dim, ui::no_shadow, 1, ui::align::right);
        }

        void header_counter(const bn::string_view& counter) const override
        {
            background(180, 0, 60, 22);
            ui::text(ui::font::digits, counter, 234, 7, dim, ui::no_shadow, 1, ui::align::right);
        }

        void time_bar(int remaining, bool label_on) const override
        {
            background(0, 24, 240, 17);
            ui::text(ui::font::body, "TIME", 6, 29, dim);
            segments(42, 28, 156, 9, 5, 6, track);
            segments(42, 28, segment_width(156, remaining), 9, 5, 6, amber);

            bn::string<8> seconds;
            append_padded(seconds, remaining, 1);
            seconds += 's';

            if(remaining > 5)
            {
                ui::text(ui::font::digits, seconds, 234, 29, amber, ui::no_shadow, 1, ui::align::right);
            }
            else if(label_on)
            {
                // Inverted, blinking
                ui::fill(214, 27, 22, 11, amber);
                ui::text(ui::font::digits, seconds, 225, 29, bg, ui::no_shadow, 1, ui::align::center);
            }
        }

        bool blinks_low_time() const override
        {
            return true;
        }

        void list_area() const override
        {
            background(list_x, list_y, list_width, list_height);
            outline(list_x, list_y, list_width, list_height, frame);
        }

        void list_band(int y, int height, bool scrolling) const override
        {
            background(5, y, scrolling ? 222 : 230, height);
        }

        void row(const row_info& info) const override
        {
            int right = info.x + info.width;

            if(info.selected)
            {
                // Inverted bar, 1 px left of the other rows: "> name ..... CODE"
                ui::fill(info.x - 1, info.y, info.width + 1, selected_row_height, amber);

                if(info.moving)
                {
                    ui::cursor_up(info.x + 3, info.y + 6, bg);
                    ui::cursor_down(info.x + 3, info.y + 14, bg);
                }
                else
                {
                    ui::text(ui::font::body, ">", info.x + 3, info.y + 8, bg);
                }

                int code_width = ui::text_width(ui::font::code12, info.code);
                int code_x = right - 4 - code_width;
                int name_x = info.x + 15;
                bn::string<40> name = fitted(ui::font::body, info.item.label, code_x - 8 - name_x);
                ui::text(ui::font::body, name, name_x, info.y + 8, bg);
                leader(name_x + ui::text_width(ui::font::body, name) + 4, code_x - 4, info.y + 14, bg);
                ui::text(ui::font::code12, info.code, right - 4, info.y + 5, bg, ui::no_shadow, 1, ui::align::right);
            }
            else
            {
                int code_width = ui::text_width(ui::font::digits, info.code);
                int code_x = right - 4 - code_width;
                int name_x = info.x + 12;
                bn::string<40> name = fitted(ui::font::body, info.item.label, code_x - 8 - name_x);
                ui::text(ui::font::body, name, name_x, info.y + 4, amber);
                leader(name_x + ui::text_width(ui::font::body, name) + 4, code_x - 4, info.y + 10, frame);
                ui::text(ui::font::digits, info.code, right - 4, info.y + 4, amber, ui::no_shadow, 1,
                         ui::align::right);
            }
        }

        void scrollbar(int first, int visible, int total) const override
        {
            ui::fill(229, scrollbar_top, 4, scrollbar_height, track);

            int thumb_y, thumb_height;
            scroll_thumb(first, visible, total, thumb_y, thumb_height);
            ui::fill(229, thumb_y, 4, thumb_height, amber);
        }

        void footer_bar(int top) const override
        {
            dashed_rule(top);
        }

        int pill_width(const char* label) const override
        {
            return ui::text_width(ui::font::digits, label) + 6;
        }

        // Square corners
        int pill(int x, int label_y, const char* label, bool) const override
        {
            int width = pill_width(label);
            ui::fill(x, label_y - 2, width, 11, amber);
            ui::text(ui::font::digits, label, x + 3, label_y, bg);
            return width;
        }

        int box_border() const override
        {
            return 3;
        }

        uint8_t box_fill() const override
        {
            return bg;
        }

        // Double border: 1 px line, 1 px gap, 1 px line.
        void box(int x, int y, int width, int height) const override
        {
            background(x, y, width, height);
            outline(x, y, width, height, amber);
            outline(x + 2, y + 2, width - 4, height - 4, amber);
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
            uint8_t text = info.selected ? bg : amber;
            ui::fill(x, info.y, width, 15, info.selected ? amber : bg);
            ui::text(ui::font::body, info.label, x + 4, info.y + 4, text);

            if(info.value)
            {
                bn::string<32> value = "[";
                value += info.value;
                value += ']';
                ui::text(ui::font::digits, value, x + width - 4, info.y + 4, text, ui::no_shadow, 1, ui::align::right);
            }
            else if(info.danger)
            {
                ui::text(ui::font::body, "!", x + width - 4, info.y + 4, text, ui::no_shadow, 1, ui::align::right);
            }
        }

        void info_box(const bn::string_view& text) const override
        {
            background(4, 28, 232, 18);
            outline(4, 28, 232, 18, frame);
            ui::fill(10, 34, 6, 6, amber);
            ui::text(ui::font::body, text, 21, 33, amber);
        }

        void busy(const bn::string_view& title, const bn::string_view& subtitle) const override
        {
            constexpr int x = 24, y = 70;     // below the 32x32 logo
            box(x, y, 192, 62);
            ui::text(ui::font::body, upper(title), 120, y + 3 + 9, amber, ui::no_shadow, 1, ui::align::center);

            // Static: the CPU is busy while this is shown.
            segments(x + 3 + 13, y + 3 + 27, 160, 8, 5, 6, track);
            segments(x + 3 + 61, y + 3 + 27, 48, 8, 5, 6, amber);
            ui::text(ui::font::body, subtitle, 120, y + 3 + 44, dim, ui::no_shadow, 1, ui::align::center);
        }

        void welcome_band() const override
        {
            ui::text(ui::font::body, "WELCOME TO", 120, 18, dim, ui::no_shadow, 1, ui::align::center);
            logo_title_row(*this, "AUTHBOY", 3, amber);
            dashed_rule(66);
        }

        const uint8_t* logo_colors() const override
        {
            static constexpr uint8_t logo[7] = { 0, track, dim, frame, track, amber, amber };
            return logo;
        }

        void clock_info(const bn::string_view& text, bool) const override
        {
            background(4, 28, 232, 18);
            outline(4, 28, 232, 18, frame);
            ui::fill(9, 34, 7, 7, amber);
            ui::text(ui::font::body, text, 21, 33, amber);
        }

        void clock_panel(int x, int y, int width, int height) const override
        {
            background(x, y, width, height);
            outline(x, y, width, height, frame);
        }

        uint8_t clock_cell(int x, int y, int width, int height, bool selected) const override
        {
            if(selected)
            {
                ui::fill(x, y, width, height, amber);
                return bg;
            }

            outline(x, y, width, height, frame);
            return amber;
        }

        uint8_t clock_arrow_color() const override
        {
            return amber;
        }

        // "[PAUSED]" in dim text; RUNNING inverted.
        void clock_tag(bool running, bool) const override
        {
            background(150, 0, 90, 22);

            if(running)
            {
                int width = ui::text_width(ui::font::digits6, "RUNNING") + 6;
                ui::fill(234 - width, 6, width, 10, amber);
                ui::text(ui::font::digits6, "RUNNING", 234 - width + 3, 8, bg);
            }
            else
            {
                ui::text(ui::font::digits6, "[PAUSED]", 234, 8, dim, ui::no_shadow, 1, ui::align::right);
            }
        }

        bool clock_upper_labels() const override
        {
            return true;
        }

        void input_box(int x, int y, int width, int height) const override
        {
            background(x, y, width, height);
            outline(x, y, width, height, frame);
        }

        void pin_slot(int x, int y, bool filled) const override
        {
            if(filled)
            {
                ui::fill(x, y, 8, 8, amber);
            }
            else
            {
                ui::fill(x + 3, y + 3, 2, 2, frame);
            }
        }

        void name_input(int x, int y, int, int, const bn::string_view& text, bool caret_visible) const override
        {
            ui::pattern_hline(x + 21, y + 16, 128, 6, 8, frame);
            ui::text(ui::font::body, "> ", x + 5, y + 6, amber);

            int text_x = x + 5 + ui::text_width(ui::font::body, "> ") + 1;
            ui::text(ui::font::body, text, text_x, y + 6, amber);

            if(caret_visible)
            {
                int width = ui::text_width(ui::font::body, text);
                ui::fill(text_x + width + (text.empty() ? 0 : 1), y + 6, 8, 8, amber);
            }
        }

        void key_panel(int x, int y, int width, int height) const override
        {
            background(x, y, width, height);
            outline(x, y, width, height, frame);
        }

        // No cap graphics: the selected key's cell fills amber.
        void key(int x, int y, int width, int height, char label, bool selected) const override
        {
            background(x - 2, y - 2, width + 4, height + 4);

            if(selected)
            {
                ui::fill(x, y, width, height, amber);
            }

            char text[2] = { label, 0 };
            ui::text(ui::font::digits, text, x + width / 2, y + (height - 7) / 2, selected ? bg : amber,
                     ui::no_shadow, 1, ui::align::center);
        }

        void case_tabs(bool lower_case) const override
        {
            constexpr int tab_width = 26;
            constexpr int lower_x = 236 - tab_width;
            constexpr int upper_x = lower_x - 4 - tab_width;
            background(upper_x - 2, 52, 236 - upper_x + 2, 11);

            for(int i = 0; i < 2; ++i)
            {
                bool lower_tab = i == 1;
                int x = lower_tab ? lower_x : upper_x;
                bool active = lower_tab == lower_case;

                if(active)
                {
                    ui::fill(x, 53, tab_width, 10, amber);
                }

                ui::text(ui::font::digits, lower_tab ? "abc" : "ABC", x + tab_width / 2, 55, active ? bg : dim,
                         ui::no_shadow, 1, ui::align::center);
            }
        }
    };

    constexpr amber_skin look;
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
