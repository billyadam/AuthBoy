// Field Guide theme: busy, message, welcome (screen 10), show secret and settings (screen 05).

#include "theme.h"

#include "bn_string.h"

#include "about_layout.h"
#include "clock_layout.h"
#include "ui.h"
#include "version.h"

namespace app::field_guide
{

namespace
{
    constexpr const char* settings_names[settings_option_count] = { "Sort", "Theme", "Set clock", "Lock now", "Change password" };

    // Settings menu box (screen 05)
    constexpr int settings_box_x = 4, settings_box_y = 50, settings_box_width = 232;
    constexpr int settings_step = 16;     // 15 px bars, 1 px apart: five items fit above the footer

    // Logo colors: outline, shackle, body, body shade, screen, A button
    constexpr uint8_t logo_colors[7] = { 0, ui::ink, ui::bevel, ui::red, ui::header_shadow, ui::info, ui::selection };
    constexpr int busy_logo_x = 104, busy_logo_y = 32;     // 32x32, centered above the box

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
            ui::logo(ui::logo_frame::big, busy_logo_x, busy_logo_y, logo_colors);
        }

        void busy_logo(bool open, int shake) override
        {
            ui::stripes(busy_logo_x - 2, busy_logo_y, 36, 32);
            ui::logo(open ? ui::logo_frame::big_open : ui::logo_frame::big, busy_logo_x + shake, busy_logo_y,
                     logo_colors);
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

            if(a_label)
            {
                draw_continue(box.x + box.width - 8, box.button_y, a_label, true);
            }
        }

        void message_blink(const char* const*, int line_count, const char* a_label, bool arrow_visible) override
        {
            message_box box = message_geometry(line_count);
            if(a_label)
            {
                draw_continue(box.x + box.width - 8, box.button_y, a_label, arrow_visible);
            }
        }

        void welcome(const char* line_1, const char* line_2) override
        {
            ui::background();
            ui::fill(0, 0, 240, 76, ui::red);
            ui::fill(0, 1, 240, 1, ui::header_light);
            ui::fill(0, 64, 240, 4, ui::hero_accent);
            ui::fill(0, 76, 240, 2, ui::header_shadow);
            ui::text(ui::font::body, "Welcome to", 120, 14, ui::header_subtext, ui::no_shadow, 1, ui::align::center);

            // Logo and title in one row, centered as a group, top y 28.
            int title_width = ui::text_width(ui::font::body, "AuthBoy", 4);
            int x = (240 - (32 + 8 + title_width)) / 2;
            ui::logo(ui::logo_frame::big, x, 28, logo_colors);
            ui::text(ui::font::body, "AuthBoy", x + 40, 28 + (32 - 28) / 2, ui::white, ui::header_shadow, 4,
                     ui::align::left, 2);

            ui::panel(welcome_x, welcome_y, welcome_width, welcome_height, ui::white, true);
            ui::text(ui::font::body, line_1, welcome_x + 6, welcome_y + 8, ui::ink, ui::light_shadow);
            ui::text(ui::font::body, line_2, welcome_x + 6, welcome_y + 20, ui::ink, ui::light_shadow);
            welcome_blink(true);
        }

        void welcome_blink(bool arrow_visible) override
        {
            draw_continue(welcome_x + welcome_width - 8, welcome_y + 38, "continue", arrow_visible);
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
                // Include the rounded main-panel border outside the QR's quiet zone.
                panel_x = 4 + ui::qr_code(view.qrcode, 4, area_top, 160 - area_top, 4, true) + 8;
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
            constexpr ui::hint footer[] = { { "A", "choose" }, { "B", "back" }, { "SEL", "about" } };
            constexpr int box_height = 9 + settings_option_count * settings_step + 1;

            ui::background();
            ui::header("Settings");
            ui::text(ui::font::digits, app_version_label, 233, 8, ui::header_subtext, ui::no_shadow, 1, ui::align::right);

            ui::panel(4, 28, 232, 18, ui::info, false);
            ui::round_fill(8, 33, 8, 8, 4, ui::green);
            ui::fill(10, 34, 4, 2, ui::green_shine);
            ui::text(ui::font::body, accounts_stored_label(state.accounts), 18, 33, ui::ink);

            ui::panel(settings_box_x, settings_box_y, settings_box_width, box_height, ui::white, true);

            for(int i = 0; i < settings_option_count; ++i)
            {
                settings_item(state, i);
            }

            ui::footer_bar(144);
            ui::footer_hints(147, footer, 3);
        }

        void about(const about_state& state) override;
        void set_clock(const clock_editor_state& state) override;
        void set_clock_changed(const clock_editor_state& state) override;

        void settings_item(const settings_state& state, int index) override
        {
            int item_y = settings_box_y + 9 + index * settings_step;
            bool is_selected = index == state.selected;
            ui::menu_item(settings_box_x, settings_box_width, item_y, settings_names[index], ui::ink, ui::light_shadow,
                          is_selected, is_selected ? ui::cursor_bob(state.frame_counter) : 0);

            const char* value = nullptr;

            if(index == settings_option_sort)
            {
                value = sort_label(state.accounts);
            }
            else if(index == settings_option_theme)
            {
                value = theme_value_label(state.accounts.theme);
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

void screen_painter_impl::about(const about_state& state)
{
    using namespace about_layout;

    ui::background();
    ui::header(about_page_title(state.page));
    ui::text(ui::font::digits, about_page_counter(state.page), 233, 8, ui::header_subtext, ui::no_shadow, 1,
             ui::align::right);
    ui::panel(panel_x, panel_y, panel_width, panel_height, ui::white, true);

    if(state.page == 0)
    {
        intro(state, ui::ink, ui::secondary, logo_colors);
    }
    else if(about_is_security_page(state.page))
    {
        security(state.page, ui::ink);
    }
    else
    {
        items(state.page, ui::ink, ui::secondary);
    }

    const ui::hint hints[] = { { ui::dpad_left_right, "page" }, { "B", "back" } };
    ui::footer_bar(144);
    ui::footer_hints(147, hints, 2);
}

void screen_painter_impl::set_clock(const clock_editor_state& state)
{
    using namespace clock_layout;

    ui::background();
    ui::header("Set clock");

    ui::panel(info_x, info_y, info_width, info_height, ui::info, false);
    ui::round_fill(dot_x, dot_y, dot_size, dot_size, 3, state.running ? ui::green : ui::selection);
    ui::text(ui::font::body, info_text(state), info_text_x, info_text_y, ui::ink);

    set_clock_changed(state);

    if(state.running)
    {
        const ui::hint hints[] = { { "A", "done" }, { "B", "set again" } };
        ui::footer_bar(running_footer_top);
        ui::footer_hints(running_row_y, hints, 2);
    }
    else
    {
        const ui::hint row_1[] = {
            { ui::dpad_left_right, "field" }, { ui::dpad_up_down, "adjust" }, { "L R", "\x05" "10" }
        };
        const ui::hint row_2[] = { { "A", "start clock" }, { "B", state.back_label } };
        ui::footer_bar(paused_footer_top);
        ui::footer_hints(paused_row_1_y, row_1, 3);
        ui::footer_hints(paused_row_2_y, row_2, 2);
    }
}

void screen_painter_impl::set_clock_changed(const clock_editor_state& state)
{
    using namespace clock_layout;

    // Status tag
    const char* tag = state.running ? "RUNNING" : "PAUSED";
    ui::header_band_clear(150, 90);
    ui::pill(tag_right - ui::pill_width(tag, ui::font::digits6), tag_y, tag, state.running ? ui::green : ui::selection,
             ui::ink, ui::font::digits6);

    // Panel
    layout cells = make_layout();
    ui::panel(panel_x, panel_y, panel_width, panel_height, ui::white, true);
    ui::text(ui::font::body, "Date", label_x, date_label_y, ui::ink);
    ui::text(ui::font::body, "Time", label_x, time_label_y, ui::ink);
    ui::text(ui::font::body, "Zone", label_x, zone_label_y, ui::ink);
    separators(cells, ui::secondary, ui::ink);

    for(int field = 0; field < clock_field_count; ++field)
    {
        const rect& cell = cells.cells[field];
        bool selected = ! state.running && field == state.field;

        if(selected)
        {
            // 1 px white ring, then 1 px red ring
            ui::round_fill(cell.x - 2, cell.y - 2, cell.width + 4, cell.height + 4, 2, ui::red);
            ui::fill(cell.x - 1, cell.y - 1, cell.width + 2, cell.height + 2, ui::white);
            ui::fill(cell.x, cell.y, cell.width, cell.height, ui::selection);
            ui::fill(cell.x, cell.y + cell.height - 1, cell.width, 1, ui::selection_edge);
        }
        else
        {
            ui::fill(cell.x, cell.y, cell.width, cell.height, ui::zebra);
            ui::fill(cell.x, cell.y + cell.height - 1, cell.width, 1, ui::key_shade);
        }

        cell_text(state, field, cell, ui::ink);
    }

    readout(state, ui::secondary);

    if(! state.running && state.arrows_visible)
    {
        arrows(cells.cells[state.field], ui::red);
    }
}


screen_painter& screens()
{
    return painter;
}

}
