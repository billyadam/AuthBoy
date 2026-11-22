// Skinned themes: home screen (screens 01-04 of the theme handoffs) and helpers for skins.

#include "skin.h"

#include "bn_string.h"

#include "app_text.h"
#include "totp.h"

namespace app::skin
{

// ---- Helpers ----

bn::string<32> upper(const bn::string_view& text)
{
    bn::string<32> result;

    for(char c : text)
    {
        if(result.full())
        {
            break;
        }

        result += c >= 'a' && c <= 'z' ? char(c - 'a' + 'A') : c;
    }

    return result;
}

bn::string<40> fitted(ui::font text_font, const bn::string_view& text, int max_width)
{
    bool truncated;
    bn::string<40> result;
    result += ui::fit_text(text_font, text, max_width, 1, truncated);

    if(truncated)
    {
        result += "...";
    }

    return result;
}

void segments(int x, int y, int width, int height, int on, int period, uint8_t color)
{
    for(int i = 0; i < width; i += period)
    {
        ui::fill(x + i, y, on < width - i ? on : width - i, height, color);
    }
}

void triangle_down(int x, int y, uint8_t color)
{
    for(int row = 0; row < 4; ++row)
    {
        ui::fill(x + row, y + row, 7 - 2 * row, 1, color);
    }
}

void outline(int x, int y, int width, int height, uint8_t color)
{
    ui::fill(x, y, width, 1, color);
    ui::fill(x, y + height - 1, width, 1, color);
    ui::fill(x, y + 1, 1, height - 2, color);
    ui::fill(x + width - 1, y + 1, 1, height - 2, color);
}

void scroll_thumb(int first, int visible, int total, int& thumb_y, int& thumb_height)
{
    thumb_height = scrollbar_height * visible / total;

    if(thumb_height < 8)
    {
        thumb_height = 8;
    }

    int hidden = total - visible;
    thumb_y = scrollbar_top + (hidden > 0 ? (scrollbar_height - thumb_height) * first / hidden : 0);
}

void parallelogram(int x, int y, int width, int height, int slant, uint8_t color, bool left_straight)
{
    // Each row moves left by slant / (height - 1) px from top to bottom.
    for(int row = 0; row < height; ++row)
    {
        int shift = height > 1 ? (slant * (height - 1 - row) + (height - 1) / 2) / (height - 1) : 0;
        int left = left_straight ? x : x + shift;
        int right = x + width - slant + shift;     // exclusive
        ui::fill(left, y + row, right - left, 1, color);
    }
}

void skin::row_separator(int, int, int) const
{
}

bool skin::has_cursor() const
{
    return false;
}

void skin::row_cursor(int, int, bool) const
{
}

void skin::down_arrow(int x, int y, bool visible) const
{
    ui::fill(x, y, 7, 4, box_fill());

    if(visible)
    {
        triangle_down(x, y, colors.caret);
    }
}

void footer_hints(const skin& look, int label_y, const hint* hints, int count)
{
    constexpr int gap = 10;
    int total = 0;

    for(int i = 0; i < count; ++i)
    {
        total += look.pill_width(hints[i].button) + 3 + ui::text_width(ui::font::body, hints[i].label) + (i ? gap : 0);
    }

    int x = (240 - total) / 2;

    for(int i = 0; i < count; ++i)
    {
        int width = look.pill(x, label_y, hints[i].button, false);
        ui::text(ui::font::body, hints[i].label, x + width + 3, label_y, look.colors.hint);
        x += width + 3 + ui::text_width(ui::font::body, hints[i].label) + gap;
    }
}

void footer_message(const skin& look, int label_y, const bn::string_view& message)
{
    ui::text(ui::font::body, message, 120, label_y, look.colors.hint, ui::no_shadow, 1, ui::align::center);
}

void continue_hint(const skin& look, int right_x, int label_y, const char* label, bool arrow_visible)
{
    int width = look.pill_width("A") + 4 + ui::text_width(ui::font::body, label) + 6 + 7;
    int x = right_x - width;
    int pill_width = look.pill(x, label_y, "A", false);
    ui::text(ui::font::body, label, x + pill_width + 4, label_y, look.colors.box);
    look.down_arrow(right_x - 7, label_y + 2, arrow_visible);
}

// ---- Home painter ----

namespace
{
    constexpr int totp_period = home_state::totp_period;

    // Account menu (command box) and its dialog, delete confirm.
    constexpr int dialog_x = 4, dialog_y = 114, dialog_width = 128, dialog_height = 44;
    constexpr int command_x = 134, command_width = 102, command_bottom = 158;
    constexpr int confirm_x = 4, confirm_y = 108, confirm_width = 232, confirm_height = 50;
    constexpr int menu_step = 18;

    int row_h(const home_state& s, int position)
    {
        return position == s.selected ? selected_row_height : row_height;
    }

    bool scrolling(const home_state& s)
    {
        int total = s.accounts.count * (row_height + row_gap) - row_gap + (selected_row_height - row_height);
        return s.accounts.count > 0 && total > rows_bottom - rows_top;
    }

    int row_width(const home_state& s)
    {
        return scrolling(s) ? row_width_scrolling : row_width_full;
    }

    int fitting_rows(const home_state& s, int first)
    {
        int y = rows_top;
        int count = 0;

        for(int position = first; position < s.accounts.count; ++position)
        {
            if(y + row_h(s, position) > rows_bottom)
            {
                break;
            }

            y += row_h(s, position) + row_gap;
            ++count;
        }

        return count;
    }

    int row_y(const home_state& s, int position)
    {
        int y = rows_top;

        for(int p = s.first_visible; p < position; ++p)
        {
            y += row_h(s, p) + row_gap;
        }

        return y;
    }

    int command_height(const home_state& s)
    {
        return 10 + s.menu_count() * menu_step;
    }
}

void skin_home_painter::layout(home_state& s)
{
    s.prepare();

    if(s.first_visible > s.selected)
    {
        s.first_visible = s.selected;
    }

    while(s.first_visible < s.selected && s.first_visible + fitting_rows(s, s.first_visible) <= s.selected)
    {
        ++s.first_visible;
    }

    while(s.first_visible > 0 && s.first_visible - 1 + fitting_rows(s, s.first_visible - 1) >= s.accounts.count)
    {
        --s.first_visible;
    }

    s.visible_count = fitting_rows(s, s.first_visible);
}

void skin_home_painter::draw_all(home_state& s)
{
    layout(s);
    _skin.background(0, 0, 240, 160);
    _skin.header("AuthBoy", true);
    draw_clock(s);
    draw_time_bar(s);
    draw_list(s);
    draw_footer(s);

    if(s.modal != home_modal::none)
    {
        _skin.modal_backdrop(0, 0, 240, 160);
        draw_modal(s);
    }
}

void skin_home_painter::second_passed(home_state& s)
{
    if(s.rtc.status == rtc_status::ok && s.remaining_seconds() > 5)
    {
        _label_on = true;
    }

    draw_clock(s);
    draw_time_bar(s);

    if(s.modal != home_modal::none)
    {
        redraw_modal_over_top(s);
    }
}

// The header and TIME row (y 0-40) were redrawn behind an open modal: fade them again and
// redraw the boxes (the tallest menu reaches up to y 39 with its outline).
void skin_home_painter::redraw_modal_over_top(const home_state& s)
{
    _skin.modal_backdrop(0, 0, 240, 41);
    draw_modal(s);
}

void skin_home_painter::codes_changed(home_state& s)
{
    if(s.modal != home_modal::none)
    {
        draw_all(s);
        return;
    }

    layout(s);
    draw_list(s);
}

void skin_home_painter::list_changed(home_state& s)
{
    // A modal may have closed: everything behind it was faded.
    draw_all(s);
}

void skin_home_painter::selection_moved(home_state& s, int old_selected)
{
    if(s.modal != home_modal::none)
    {
        draw_all(s);
        return;
    }

    int old_first_visible = s.first_visible;
    layout(s);

    int first = old_selected < s.selected ? old_selected : s.selected;
    int last = old_selected < s.selected ? s.selected : old_selected;

    if(s.first_visible != old_first_visible || first < s.first_visible || last >= s.first_visible + s.visible_count)
    {
        draw_list(s);
        return;
    }

    // Rows keep their total height (one tall row either way), so only these rows and the
    // gaps around them change.
    int y = row_y(s, first);
    int height = -row_gap;

    for(int position = first; position <= last; ++position)
    {
        height += row_h(s, position) + row_gap;
    }

    _skin.list_band(y - 1, height + 2, scrolling(s));

    for(int position = first; position <= last; ++position)
    {
        draw_row(s, position, row_y(s, position));
    }

    draw_separators(s, first - 1, last);
}

void skin_home_painter::modal_opened(home_state& s)
{
    draw_footer(s);     // hidden while a modal is open
    _skin.modal_backdrop(0, 0, 240, 160);
    draw_modal(s);
}

void skin_home_painter::menu_moved(home_state& s, int old_menu_selected)
{
    draw_menu_item(s, old_menu_selected);
    draw_menu_item(s, s.menu_selected);
}

void skin_home_painter::footer_changed(home_state& s)
{
    if(s.modal == home_modal::none)
    {
        draw_footer(s);
    }
}

void skin_home_painter::animate(home_state& s)
{
    if(s.rtc.status != rtc_status::ok)
    {
        return;
    }

    // Low time: the seconds label blinks 30 frames on, 30 off.
    if(_skin.blinks_low_time() && s.remaining_seconds() <= 5)
    {
        if(++_blink_counter >= 30)
        {
            _blink_counter = 0;
            _label_on = ! _label_on;
            draw_time_bar(s);

            if(s.modal != home_modal::none)
            {
                redraw_modal_over_top(s);
            }
        }
    }

    if(! _skin.has_cursor() || s.frame_counter % 16 != 0 || s.accounts.count == 0)
    {
        return;
    }

    if(s.modal == home_modal::menu)
    {
        draw_menu_item(s, s.menu_selected);
    }
    else if(s.modal == home_modal::none && ! s.moving)
    {
        _skin.row_cursor(row_y(s, s.selected), ui::cursor_bob(s.frame_counter), false);
    }
}

void skin_home_painter::draw_clock(const home_state& s)
{
    if(s.rtc.status != rtc_status::ok)
    {
        _skin.header_clock("", "");
        return;
    }

    totp::civil_time civil = totp::civil_from_unix(s.rtc.local_time);
    bn::string<16> time_text;
    append_padded(time_text, civil.hour, 2);
    time_text += ':';
    append_padded(time_text, civil.minute, 2);
    time_text += ':';
    append_padded(time_text, civil.second, 2);

    bn::string<32> date_text;
    append_padded(date_text, civil.year, 4);
    date_text += '-';
    append_padded(date_text, civil.month, 2);
    date_text += '-';
    append_padded(date_text, civil.day, 2);
    date_text += " UTC";

    // Short form ("+7", "+5:30") so the date doesn't run into the title.
    int abs_offset = utc_offset_minutes < 0 ? -utc_offset_minutes : utc_offset_minutes;
    date_text += utc_offset_minutes < 0 ? '-' : '+';
    append_padded(date_text, abs_offset / 60, 1);

    if(abs_offset % 60)
    {
        date_text += ':';
        append_padded(date_text, abs_offset % 60, 2);
    }

    _skin.header_clock(time_text, date_text);
}

void skin_home_painter::draw_time_bar(const home_state& s)
{
    if(s.rtc.status != rtc_status::ok)
    {
        _skin.background(0, 24, 240, 17);
        return;
    }

    int remaining = s.remaining_seconds();
    _skin.time_bar(remaining, remaining > 5 || _label_on);
}

void skin_home_painter::draw_row(home_state& s, int position, int y)
{
    uint32_t code = s.code(position);
    bn::string<8> code_text;
    append_padded(code_text, int(code / 1000), 3);
    code_text += ' ';
    append_padded(code_text, int(code % 1000), 3);

    bool selected = position == s.selected;
    row_info info = { s.item(position), code_text.c_str(), row_x, y, row_width(s), selected, selected && s.moving,
                      selected ? ui::cursor_bob(s.frame_counter) : 0 };
    _skin.row(info);
}

// Separators in the gaps below rows first..last (between two unselected visible rows).
void skin_home_painter::draw_separators(const home_state& s, int first, int last)
{
    int first_gap = first < s.first_visible ? s.first_visible : first;
    int last_gap = s.first_visible + s.visible_count - 2;

    if(last < last_gap)
    {
        last_gap = last;
    }

    for(int position = first_gap; position <= last_gap; ++position)
    {
        if(position != s.selected && position + 1 != s.selected)
        {
            _skin.row_separator(row_x + 2, row_y(s, position) + row_height, row_width(s) - 4);
        }
    }
}

void skin_home_painter::draw_list(home_state& s)
{
    _skin.list_area();

    if(s.rtc.status != rtc_status::ok)
    {
        bool missing = s.rtc.status == rtc_status::missing;
        ui::text(ui::font::body, missing ? "RTC not found" : "RTC returned an invalid time", 120, 78,
                 _skin.colors.text, ui::no_shadow, 1, ui::align::center);
        ui::text(ui::font::body, missing ? "Needs a cart/emulator with a clock" : "Set it with your flashcart's tools",
                 120, 92, _skin.colors.dim, ui::no_shadow, 1, ui::align::center);
        return;
    }

    if(s.accounts.count == 0)
    {
        ui::text(ui::font::body, "No accounts yet", 120, 78, _skin.colors.text, ui::no_shadow, 1, ui::align::center);
        ui::text(ui::font::body, "STA: add an account", 120, 92, _skin.colors.dim, ui::no_shadow, 1,
                 ui::align::center);
        return;
    }

    for(int i = 0; i < s.visible_count; ++i)
    {
        int position = s.first_visible + i;
        draw_row(s, position, row_y(s, position));
    }

    draw_separators(s, s.first_visible, s.first_visible + s.visible_count - 1);

    if(scrolling(s))
    {
        _skin.scrollbar(s.first_visible, s.visible_count, s.accounts.count);
    }
}

void skin_home_painter::draw_footer(const home_state& s)
{
    _skin.background(0, list_y + list_height, 240, 160 - (list_y + list_height));

    if(s.modal != home_modal::none)
    {
        return;
    }

    _skin.footer_bar(footer_top);

    if(s.footer_message_frames > 0)
    {
        bn::string<32> message = "List full: ";
        append_padded(message, max_accounts, 1);
        message += " accounts";
        footer_message(_skin, footer_top + 3, message);
    }
    else if(s.moving)
    {
        constexpr hint hints[] = { { "UP/DOWN", "move" }, { "A", "done" } };
        footer_hints(_skin, footer_top + 3, hints, 2);
    }
    else
    {
        constexpr hint hints[] = { { "STA", "add" }, { "SEL", "settings" } };
        footer_hints(_skin, footer_top + 3, hints, 2);
    }
}

void skin_home_painter::draw_menu_item(const home_state& s, int index)
{
    int box_y = command_bottom - command_height(s);
    home_option option = s.menu_option(index);
    bool selected = index == s.menu_selected;
    menu_item_info info = { command_x, command_width, box_y + _skin.box_border() + 5 + index * menu_step,
                            home_option_names[option], nullptr, selected, option == home_option_delete,
                            selected ? ui::cursor_bob(s.frame_counter) : 0 };
    _skin.menu_item(info);
}

void skin_home_painter::draw_modal(const home_state& s)
{
    int border = _skin.box_border();
    const account& item = s.item(s.selected);

    if(s.modal == home_modal::menu)
    {
        _skin.box(dialog_x, dialog_y, dialog_width, dialog_height);

        int text_x = dialog_x + border + 7;
        ui::text(ui::font::body, "What to do with", text_x, dialog_y + border + 9, _skin.colors.box);

        bn::string<40> name = fitted(ui::font::body, item.label, dialog_width - 2 * border - 14 - 6);
        name += '?';
        ui::text(ui::font::body, name, text_x, dialog_y + border + 22, _skin.colors.box);

        int height = command_height(s);
        _skin.box(command_x, command_bottom - height, command_width, height);

        for(int i = 0; i < s.menu_count(); ++i)
        {
            draw_menu_item(s, i);
        }
    }
    else if(s.modal == home_modal::delete_confirm)
    {
        _skin.box(confirm_x, confirm_y, confirm_width, confirm_height);

        int x = confirm_x + border + 8;
        bn::string<48> question = "Delete \"";
        question += fitted(ui::font::body, item.label, confirm_width - 2 * border - 16 - 50);
        question += "\"?";
        ui::text(ui::font::body, question, x, confirm_y + border + 9, _skin.colors.box);

        int label_y = confirm_y + border + 28;
        x += _skin.pill(x, label_y, "A", true) + 3;
        ui::text(ui::font::body, "delete", x, label_y, _skin.colors.danger);
        x += ui::text_width(ui::font::body, "delete") + 14;
        x += _skin.pill(x, label_y, "B", false) + 3;
        ui::text(ui::font::body, "keep", x, label_y, _skin.colors.box);
    }
}

}
