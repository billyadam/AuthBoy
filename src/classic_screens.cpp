// Classic theme: busy, message, welcome, show secret and settings screens, as in the original
// release. Coordinates are the original sprite ones: (0, 0) is the screen center (see
// ui::classic_text).

#include "theme.h"

#include "bn_string.h"

#include "about_layout.h"
#include "ui.h"
#include "version.h"

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
    constexpr int welcome_logo_y = 14;        // with the lines below: centered on the screen
    constexpr int busy_logo_y = 43;           // logo + two lines centered on the screen

    constexpr int settings_first_y = -26;     // below the "Password on" line, gaps even with the items
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

    // Set clock: three monospace rows (8 px per character), centered; a field is a run of
    // characters in its row.
    constexpr int clock_date_y = -22, clock_time_y = -2, clock_zone_y = 18;
    constexpr int clock_readout_y = 36;

    struct clock_span
    {
        int y, first, length, row_length;
    };

    constexpr clock_span clock_spans[clock_field_count] = {
        { clock_date_y, 0, 4, 10 }, { clock_date_y, 5, 2, 10 }, { clock_date_y, 8, 2, 10 },
        { clock_time_y, 0, 2, 8 }, { clock_time_y, 3, 2, 8 }, { clock_time_y, 6, 2, 8 },
        { clock_zone_y, 3, 6, 9 },
    };

    // The selected field: the keyboard's key cursor behind its characters.
    void clock_cursor(int field)
    {
        const clock_span& span = clock_spans[field];
        int x = 120 - span.row_length * 4 + span.first * 8;
        int top = 72 + span.y;
        int width = span.length * 8;
        ui::fill(x - 2, top + 1, width + 4, 14, ui::classic_key_edge);
        ui::fill(x - 1, top + 2, width + 2, 12, ui::classic_key);
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

        if(a_label)
        {
            bn::string<32> buttons = "A: ";
            buttons += a_label;
            ui::classic_text(classic_font::small, buttons, 0, 50);
        }
    }

    class screen_painter_impl final : public screen_painter
    {

    public:
        void busy(const char* title) override
        {
            clear(0, 160);
            ui::logo(ui::logo_frame::big, logo_x, busy_logo_y, logo_colors);
            ui::classic_text(classic_font::small, title, 0, 9);
            ui::classic_text(classic_font::small, "This takes a few seconds", 0, 29);
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
            ui::classic_text(classic_font::small, "Welcome to AuthBoy", 0, -22);
            ui::classic_text(classic_font::small, line_1, 0, 0);
            ui::classic_text(classic_font::small, line_2, 0, 20);
            ui::classic_text(classic_font::small, "A: continue", 0, 58);
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
            ui::classic_text(classic_font::small, app_version_label, 116, -66, ui::align::right);
            ui::classic_text(classic_font::small, accounts_stored_label(state.accounts), 0, -46);

            for(int i = 0; i < settings_option_count; ++i)
            {
                settings_item(state, i);
            }

            ui::classic_text(classic_font::small, "A: choose   B: back   SEL: about", 0, 66);
        }

        // Header line with the page counter; page 1: QR Code on the left, the logo and text on
        // the right, the link under them; pages 2 and 3: "label:" / value lines.
        void about(const about_state& state) override
        {
            clear(0, 160);
            ui::classic_text(classic_font::small, about_page_title(state.page), 0, -66);
            ui::classic_text(classic_font::small, about_page_counter(state.page), 116, -66, ui::align::right);

            if(state.page == 0)
            {
                constexpr int column_x = 100;

                if(state.qrcode)
                {
                    ui::qr_code(state.qrcode, 8, 22, 96, 2);
                }

                ui::logo(ui::logo_frame::big, column_x, 22, logo_colors);
                bn::string<24> name = "AuthBoy ";
                name += app_version_label;
                text_at(classic_font::small, name, column_x, 58);
                text_at(classic_font::small, about_author, column_x, 74);
                text_at(classic_font::small, "TOTP authenticator", column_x, 90);
                text_at(classic_font::small, "for the GBA", column_x, 106);
                ui::classic_text(classic_font::small, "github.com/billyadam/AuthBoy", 0, 52);
            }
            else if(about_is_security_page(state.page))
            {
                int y = 20;
                int first, count;
                about_security_paragraphs(state.page, first, count);

                for(int i = first; i < first + count; ++i)
                {
                    about_layout::wrap(about_security_text[i], 224,
                        [](const bn::string_view& line) { return ui::classic_text_width(classic_font::small, line); },
                        [&](const bn::string_view& line) {
                            text_at(classic_font::small, line, 8, y);
                            y += 11;
                        });
                    y += 3;
                }
            }
            else
            {
                const about_item* list = about_page_items(state.page);

                for(int i = 0; i < about_item_count; ++i)
                {
                    bn::string<24> label = list[i].label;
                    label += ':';
                    int y = -46 + i * 26;
                    text_at(classic_font::small, label, 12, 72 + y);
                    text_at(classic_font::small, list[i].value, 24, 72 + y + 12);
                }
            }

            ui::classic_text(classic_font::small, "</>: page   B: back", 0, 66);
        }

        void set_clock(const clock_editor_state& state) override
        {
            clear(0, 160);
            set_clock_changed(state);

            if(state.running)
            {
                ui::classic_text(classic_font::small, "A: done   B: set again", 0, 66);
            }
            else
            {
                bn::string<40> buttons = "A: start clock   B: ";
                buttons += state.back_label;
                ui::classic_text(classic_font::small, "</>: field   Up/Down: +-1   L/R: +-10", 0, 50);
                ui::classic_text(classic_font::small, buttons, 0, 66);
            }
        }

        void set_clock_changed(const clock_editor_state& state) override
        {
            clear(0, 72 + clock_readout_y + 16);
            ui::classic_text(classic_font::small, "Set clock", 0, -66);
            ui::classic_text(classic_font::small, state.running ? "RUNNING" : "PAUSED", 116, -66, ui::align::right);
            ui::classic_text(classic_font::small, state.running ? "Running. Compare with phone." :
                                                                  "Press A when phone shows this", 0, -46);

            if(! state.running && state.arrows_visible)
            {
                clock_cursor(state.field);
            }

            bn::string<16> date = clock_cell_text(state, clock_field_year);
            date += '-';
            date += clock_cell_text(state, clock_field_month);
            date += '-';
            date += clock_cell_text(state, clock_field_day);

            bn::string<16> time = clock_cell_text(state, clock_field_hour);
            time += ':';
            time += clock_cell_text(state, clock_field_minute);
            time += ':';
            time += clock_cell_text(state, clock_field_second);

            bn::string<16> zone = "UTC";
            zone += clock_cell_text(state, clock_field_zone);

            ui::classic_text(classic_font::mono, date, 0, clock_date_y);
            ui::classic_text(classic_font::mono, time, 0, clock_time_y);
            ui::classic_text(classic_font::mono, zone, 0, clock_zone_y);
            ui::classic_text(classic_font::small, clock_utc_readout(state), 0, clock_readout_y);
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

            case settings_option_clock:
                line += "Set clock";
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
