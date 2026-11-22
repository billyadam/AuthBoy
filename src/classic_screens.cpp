// Classic theme: busy, message, welcome, show secret and settings screens, as in the original
// release. Coordinates are the original sprite ones: (0, 0) is the screen center (see
// ui::classic_text).

#include "theme.h"

#include "bn_string.h"

#include "ui.h"

namespace app::classic
{

namespace
{
    using ui::classic_font;

    // Lock Boy logo in the Classic colors: black outline and white shackle (like the text),
    // key-cursor blue body with the popup navy as its shade, light blue screen, white A button.
    constexpr uint8_t logo_colors[7] = {
        0, ui::black, ui::white, ui::classic_key, ui::classic_panel, ui::classic_key_edge, ui::white
    };
    constexpr int logo_x = 104;             // 32x32, centered
    constexpr int welcome_logo_y = 6;
    constexpr int busy_logo_y = 26;

    constexpr int settings_first_y = -30;
    constexpr int settings_spacing = 18;

    void clear(int top, int bottom)
    {
        ui::fill(0, top, 240, bottom - top, ui::classic_backdrop);
    }

    // Text at a screen position: left edge x, cell top y.
    void text_at(classic_font text_font, const bn::string_view& text, int x, int y)
    {
        ui::classic_text(text_font, text, x - 120, y - 72, ui::align::left);
    }

    void message_lines(const char* line_1, const char* line_2, const char* line_3, const char* a_label)
    {
        clear(0, 160);
        ui::classic_text(classic_font::small, line_1, 0, -40);

        if(line_2)
        {
            ui::classic_text(classic_font::small, line_2, 0, -16);
        }

        if(line_3)
        {
            ui::classic_text(classic_font::small, line_3, 0, 4);
        }

        bn::string<32> buttons = "A: ";
        buttons += a_label;
        ui::classic_text(classic_font::small, buttons, 0, 50);
    }

    class screen_painter_impl final : public screen_painter
    {

    public:
        void busy(const char* title) override
        {
            clear(0, 160);
            ui::logo(ui::logo_frame::big, logo_x, busy_logo_y, logo_colors);
            ui::classic_text(classic_font::small, title, 0, -8);
            ui::classic_text(classic_font::small, "This takes a few seconds", 0, 12);
        }

        void busy_logo(bool open, int shake) override
        {
            ui::fill(logo_x - 2, busy_logo_y, 36, 32, ui::classic_backdrop);
            ui::logo(open ? ui::logo_frame::big_open : ui::logo_frame::big, logo_x + shake, busy_logo_y, logo_colors);
        }

        void message(const char* const* lines, int line_count, const char* a_label) override
        {
            message_lines(lines[0], line_count > 1 ? lines[1] : nullptr, line_count > 2 ? lines[2] : nullptr,
                          a_label);
        }

        void message_blink(const char* const*, int, const char*, bool) override
        {
        }

        // The message screen's layout, lines moved down a little under the logo.
        void welcome(const char* line_1, const char* line_2) override
        {
            clear(0, 160);
            ui::logo(ui::logo_frame::big, logo_x, welcome_logo_y, logo_colors);
            ui::classic_text(classic_font::small, "Welcome to AuthBoy", 0, -30);
            ui::classic_text(classic_font::small, line_1, 0, -8);
            ui::classic_text(classic_font::small, line_2, 0, 12);
            ui::classic_text(classic_font::small, "A: continue", 0, 50);
        }

        void welcome_blink(bool) override
        {
        }

        // Account name on top, the QR Code on the left, the secret on the right.
        void show_secret(const secret_view& view) override
        {
            clear(0, 160);
            ui::classic_text(classic_font::small, view.label, 0, -70);

            int x = 8;

            if(view.qrcode)
            {
                x = 4 + ui::qr_code(view.qrcode, 4, 20, 140) + 8;
            }

            text_at(classic_font::small, view.qrcode ? "Secret key" : "Link too long for a QR code", x, 22);

            int chars_per_line = (236 - x) / 8;
            int y = 40;
            bn::string<48> line;

            for(int start = 0; int used = secret_line(view, start, chars_per_line, line); start += used)
            {
                text_at(classic_font::mono, line, x, y);
                y += 12;
            }

            text_at(classic_font::small, "B: back", x, 140);
        }

        void settings(const settings_state& state) override
        {
            clear(0, 160);
            ui::classic_text(classic_font::small, "Settings", 0, -66);

            for(int i = 0; i < settings_option_count; ++i)
            {
                settings_item(state, i);
            }

            ui::classic_text(classic_font::small, "A: choose   B: back", 0, 66);
        }

        void settings_item(const settings_state& state, int index) override
        {
            int y = settings_first_y + index * settings_spacing;
            clear(72 + y, 72 + y + 16);

            bn::string<32> line = index == state.selected ? "> " : "  ";

            switch(index)
            {

            case settings_option_sort:
                line += "Sort: ";
                line += state.accounts.custom_order ? "Custom" : sort_label(state.accounts);
                break;

            case settings_option_theme:
                line += "Theme: ";
                line += theme_name(state.accounts.theme);
                break;

            case settings_option_lock:
                line += "Lock now";
                break;

            default:
                line += "Change password";
                break;
            }

            ui::classic_text(classic_font::small, line, 0, y);
        }
    };

    screen_painter_impl painter;
}

screen_painter& screens()
{
    return painter;
}

}
