// Field Guide theme: busy, message, welcome (screen 10), show secret and settings (screen 05).

#include "theme.h"

#include "bn_string.h"

#include "ui.h"

namespace app::field_guide
{

namespace
{
    constexpr const char* settings_names[settings_option_count] = { "Sort", "Theme", "Lock now", "Change password" };

    // Settings menu box (screen 05)
    constexpr int settings_box_x = 4, settings_box_y = 52, settings_box_width = 232;

    // Welcome dialogue box
    constexpr int welcome_x = 4, welcome_y = 100, welcome_width = 232, welcome_height = 56;

    // "(A) label" followed by the blinking "next" arrow, right-aligned to right_x.
    void draw_continue(int right_x, int y, const char* label, bool arrow_visible)
    {
        int label_width = ui::text_width(ui::font::body, label);
        int x = right_x - 5 - 4 - label_width - 3 - 10;
        ui::round_button(x, y - 1, "A", ui::ink, ui::white);
        ui::text(ui::font::body, label, x + 13, y, ui::ink, ui::light_shadow);
        ui::fill(right_x - 5, y + 2, 5, 3, ui::white);

        if(arrow_visible)
        {
            ui::cursor_down(right_x - 5, y + 2, ui::red);
        }
    }

    struct message_box
    {
        int x, y, width, height, button_y;
    };

    message_box message_geometry(int line_count)
    {
        int height = 10 + line_count * 12 + 22;
        int y = 156 - height;
        return { 4, y, 232, height, y + height - 18 };
    }

    class screen_painter_impl final : public screen_painter
    {

    public:
        void busy(const char* title) override
        {
            ui::background();
            ui::header("AuthBoy");
            ui::busy_box(title, "This takes a few seconds");
        }

        void message(const char* const* lines, int line_count, const char* a_label) override
        {
            message_box box = message_geometry(line_count);
            ui::background();
            ui::header("AuthBoy");
            ui::panel(box.x, box.y, box.width, box.height, ui::white, true);

            for(int i = 0; i < line_count; ++i)
            {
                ui::text(ui::font::body, lines[i], box.x + 6, box.y + 10 + i * 12, ui::ink, ui::light_shadow);
            }

            draw_continue(box.x + box.width - 8, box.button_y, a_label, true);
        }

        void message_blink(const char* const*, int line_count, const char* a_label, bool arrow_visible) override
        {
            message_box box = message_geometry(line_count);
            draw_continue(box.x + box.width - 8, box.button_y, a_label, arrow_visible);
        }

        void welcome() override
        {
            ui::background();
            ui::fill(0, 0, 240, 76, ui::red);
            ui::fill(0, 1, 240, 1, ui::header_light);
            ui::fill(0, 64, 240, 4, ui::hero_accent);
            ui::fill(0, 76, 240, 2, ui::header_shadow);
            ui::text(ui::font::body, "Welcome to", 120, 18, ui::header_subtext, ui::no_shadow, 1, ui::align::center);
            ui::text(ui::font::body, "AuthBoy", 120, 28, ui::white, ui::header_shadow, 4, ui::align::center, 2);

            ui::panel(welcome_x, welcome_y, welcome_width, welcome_height, ui::white, true);
            ui::text(ui::font::body, "Set a password to encrypt", welcome_x + 6, welcome_y + 10, ui::ink,
                     ui::light_shadow);
            ui::text(ui::font::body, "your accounts", welcome_x + 6, welcome_y + 22, ui::ink, ui::light_shadow);
            welcome_blink(true);
        }

        void welcome_blink(bool arrow_visible) override
        {
            draw_continue(welcome_x + welcome_width - 8, welcome_y + 34, "continue", arrow_visible);
        }

        // Header with the account name, no footer: the QR Code uses the 136 px below the header.
        void show_secret(const secret_view& view) override
        {
            bool truncated;
            bn::string<32> title;
            title += ui::fit_text(ui::font::body, view.label, 227, 2, truncated);

            if(truncated)
            {
                title += "...";
            }

            ui::background();
            ui::header(title);

            constexpr int area_top = 24;
            int panel_x = 8;

            if(view.qrcode)
            {
                // 4 px modules up to 25x25, 3 px up to 37x37, 2 px for 41x41 and 45x45.
                panel_x = 4 + ui::qr_code(view.qrcode, 4, area_top, 160 - area_top) + 8;
            }

            // Secret in groups of 4 characters, as many groups per line as fit.
            int panel_width = 236 - panel_x;
            ui::text(ui::font::body, view.qrcode ? "Secret key" : "Link too long for a QR code", panel_x, 30,
                     ui::secondary);

            int chars_per_line = (panel_width + 1) / 7;
            int y = 44;
            bn::string<48> line;

            for(int start = 0; int used = secret_line(view, start, chars_per_line, line); start += used)
            {
                ui::text(ui::font::digits, line, panel_x, y, ui::ink);
                y += 11;
            }

            ui::round_button(panel_x, 147, "B", ui::ink, ui::white);
            ui::text(ui::font::body, "back", panel_x + 13, 148, ui::ink);
        }

        void settings(const settings_state& state) override
        {
            constexpr ui::hint footer[] = { { "A", "choose" }, { "B", "back" } };
            constexpr int box_height = 9 + settings_option_count * 18 + 1;

            ui::background();
            ui::header("Settings");

            ui::panel(4, 28, 232, 18, ui::info, false);
            ui::round_fill(8, 33, 8, 8, 4, ui::green);
            ui::fill(10, 34, 4, 2, ui::green_shine);
            ui::text(ui::font::body, "Password on: accounts encrypted", 18, 33, ui::ink);

            ui::panel(settings_box_x, settings_box_y, settings_box_width, box_height, ui::white, true);

            for(int i = 0; i < settings_option_count; ++i)
            {
                settings_item(state, i);
            }

            ui::footer_bar(144);
            ui::footer_hints(147, footer, 2);
        }

        void settings_item(const settings_state& state, int index) override
        {
            int item_y = settings_box_y + 9 + index * 18;
            bool is_selected = index == state.selected;
            ui::menu_item(settings_box_x, settings_box_width, item_y, settings_names[index], ui::ink, ui::light_shadow,
                          is_selected, is_selected ? ui::cursor_bob(state.frame_counter) : 0);

            const char* value = nullptr;

            if(index == settings_option_sort)
            {
                value = state.accounts.custom_order ? "CUSTOM" : "A-Z";
            }
            else if(index == settings_option_theme)
            {
                value = theme_short_name(state.accounts.theme);
            }

            if(value)
            {
                int pill_x = settings_box_x + settings_box_width - 10 - ui::pill_width(value);
                ui::pill(pill_x, item_y - 1, value, ui::ink, ui::white);
            }
        }
    };

    screen_painter_impl painter;
}

screen_painter& screens()
{
    return painter;
}

}
