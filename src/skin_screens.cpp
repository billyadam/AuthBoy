// Skinned themes: unlocking, messages, welcome, show secret and settings (screens 05, 07 and 10 of
// the theme handoffs; the others in the same style).

#include "skin.h"

#include "bn_string.h"

namespace app::skin
{

namespace
{
    constexpr const char* settings_names[settings_option_count] = { "Sort", "Theme", "Lock now", "Change password" };

    // Settings menu box
    constexpr int settings_x = 4, settings_y = 50, settings_width = 232, settings_step = 16;

    // Logo above the unlocking box
    constexpr int busy_logo_x = 104, busy_logo_y = 32;     // 32x32, centered above the box

    // Welcome dialog box
    constexpr int welcome_x = 4, welcome_y = 100, welcome_width = 232, welcome_height = 56;

    struct message_box
    {
        int x, y, width, height, label_y;
    };

    message_box message_geometry(const skin& look, int line_count)
    {
        int height = 10 + line_count * 12 + 22;
        int y = 156 - height;
        return { 4, y, 232, height, y + look.box_border() + 8 + line_count * 12 };
    }

    int settings_height(const skin& look)
    {
        return 2 * look.box_border() + 8 + settings_option_count * settings_step;
    }
}

void skin_screen_painter::busy(const char* title)
{
    _skin.background(0, 0, 240, 160);
    _skin.header("AuthBoy", false);
    _skin.busy(title, "This takes a few seconds");
    ui::logo(ui::logo_frame::big, busy_logo_x, busy_logo_y, _skin.logo_colors());
}

void skin_screen_painter::busy_logo(bool open, int shake)
{
    _skin.background(busy_logo_x - 2, busy_logo_y, 36, 32);
    ui::logo(open ? ui::logo_frame::big_open : ui::logo_frame::big, busy_logo_x + shake, busy_logo_y,
             _skin.logo_colors());
}

void logo_title_row(const skin& look, const char* title, int scale, uint8_t color)
{
    int title_width = ui::text_width(ui::font::body, title, scale);
    int x = (240 - (32 + 8 + title_width)) / 2;
    ui::logo(ui::logo_frame::big, x, 28, look.logo_colors());
    ui::text(ui::font::body, title, x + 40, 28 + (32 - 7 * scale) / 2, color, ui::no_shadow, scale);
}

void skin_screen_painter::message(const char* const* lines, int line_count, const char* a_label)
{
    message_box box = message_geometry(_skin, line_count);
    _skin.background(0, 0, 240, 160);
    _skin.header("AuthBoy", false);
    _skin.box(box.x, box.y, box.width, box.height);

    for(int i = 0; i < line_count; ++i)
    {
        ui::text(ui::font::body, lines[i], box.x + _skin.box_border() + 8, box.y + _skin.box_border() + 10 + i * 12,
                 _skin.colors.box);
    }

    continue_hint(_skin, box.x + box.width - _skin.box_border() - 8, box.label_y, a_label, true);
}

void skin_screen_painter::message_blink(const char* const*, int line_count, const char* a_label, bool arrow_visible)
{
    message_box box = message_geometry(_skin, line_count);
    continue_hint(_skin, box.x + box.width - _skin.box_border() - 8, box.label_y, a_label, arrow_visible);
}

void skin_screen_painter::welcome(const char* line_1, const char* line_2)
{
    int border = _skin.box_border();
    _skin.background(0, 0, 240, 160);
    _skin.welcome_band();
    _skin.box(welcome_x, welcome_y, welcome_width, welcome_height);
    ui::text(ui::font::body, line_1, welcome_x + border + 8, welcome_y + border + 7, _skin.colors.box);
    ui::text(ui::font::body, line_2, welcome_x + border + 8, welcome_y + border + 19, _skin.colors.box);
    welcome_blink(true);
}

void skin_screen_painter::welcome_blink(bool arrow_visible)
{
    int border = _skin.box_border();
    continue_hint(_skin, welcome_x + welcome_width - border - 8, welcome_y + welcome_height - border - 12, "continue",
                  arrow_visible);
}

// Header with the account name, no footer: the QR Code uses the 136 px below the header.
void skin_screen_painter::show_secret(const secret_view& view)
{
    _skin.background(0, 0, 240, 160);
    _skin.header(view.label, false);

    constexpr int area_top = 24;
    int panel_x = 8;

    if(view.qrcode)
    {
        // 4 px modules up to 25x25, 3 px up to 37x37, 2 px for 41x41 and 45x45. Always black on
        // white, whatever the theme, so it scans reliably.
        panel_x = 4 + ui::qr_code(view.qrcode, 4, area_top, 160 - area_top) + 8;
    }

    ui::text(ui::font::body, view.qrcode ? "Secret key" : "Link too long for a QR code", panel_x, 30,
             _skin.colors.dim);

    int chars_per_line = (236 - panel_x + 1) / 7;
    int y = 44;
    bn::string<48> line;

    for(int start = 0; int used = secret_line(view, start, chars_per_line, line); start += used)
    {
        ui::text(ui::font::digits, line, panel_x, y, _skin.colors.text);
        y += 11;
    }

    int width = _skin.pill(panel_x, 148, "B", false);
    ui::text(ui::font::body, "back", panel_x + width + 3, 148, _skin.colors.hint);
}

void skin_screen_painter::settings(const settings_state& state)
{
    constexpr hint footer[] = { { "A", "choose" }, { "B", "back" } };

    _skin.background(0, 0, 240, 160);
    _skin.header("Settings", false);
    _skin.info_box("Password on: accounts encrypted");
    _skin.box(settings_x, settings_y, settings_width, settings_height(_skin));

    for(int i = 0; i < settings_option_count; ++i)
    {
        settings_item(state, i);
    }

    _skin.footer_bar(footer_top);
    footer_hints(_skin, footer_top + 3, footer, 2);
}

void skin_screen_painter::settings_item(const settings_state& state, int index)
{
    const char* value = nullptr;

    if(index == settings_option_sort)
    {
        value = sort_label(state.accounts);
    }
    else if(index == settings_option_theme)
    {
        value = theme_value_label(state.accounts.theme);
    }

    bool selected = index == state.selected;
    menu_item_info info = { settings_x, settings_width, settings_y + _skin.box_border() + 5 + index * settings_step,
                            settings_names[index], value, selected, false,
                            selected ? ui::cursor_bob(state.frame_counter) : 0 };
    _skin.menu_item(info);
}

}
