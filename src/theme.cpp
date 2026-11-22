#include "theme.h"

#include "totp.h"

namespace app
{

namespace
{
    ui_theme active = ui_theme::field_guide;

    theme_painters field_guide_painters = { field_guide::home(), field_guide::keyboard(), field_guide::screens() };
    theme_painters classic_painters = { classic::home(), classic::keyboard(), classic::screens() };
    theme_painters net_link_painters = { net_link::home(), net_link::keyboard(), net_link::screens() };
    theme_painters pocket_lcd_painters = { pocket_lcd::home(), pocket_lcd::keyboard(), pocket_lcd::screens() };
    theme_painters phosphor_painters = { phosphor::home(), phosphor::keyboard(), phosphor::screens() };

    constexpr ui_theme settings_order[] = {
        ui_theme::classic, ui_theme::field_guide, ui_theme::net_link, ui_theme::pocket_lcd, ui_theme::phosphor
    };

    static_assert(sizeof(settings_order) / sizeof(settings_order[0]) == ui_theme_count);

    // Every layout is a full grid so rows line up.
    constexpr const char* name_upper_rows[] = { "ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ0123", "456789-._@" };
    constexpr const char* name_lower_rows[] = { "abcdefghij", "klmnopqrst", "uvwxyz0123", "456789-._@" };
    constexpr const char* base32_rows[] = { "ABCDEFGH", "IJKLMNOP", "QRSTUVWX", "YZ234567" };
    constexpr const char* pin_rows[] = { "ABCDEFGHI", "JKLMNOPQR", "STUVWXYZ0", "123456789" };

    constexpr home_option custom_order_options[] = {
        home_option_rename, home_option_change_key, home_option_show_secret, home_option_move, home_option_delete,
        home_option_cancel
    };

    constexpr home_option sorted_options[] = {
        home_option_rename, home_option_change_key, home_option_show_secret, home_option_delete, home_option_cancel
    };
}

const char* const home_option_names[] = { "Rename", "Change key", "Show secret", "Move", "Delete", "Cancel" };

void set_theme(ui_theme theme)
{
    active = int(theme) < ui_theme_count ? theme : ui_theme::field_guide;
}

ui_theme current_theme()
{
    return active;
}

const theme_painters& painters()
{
    switch(active)
    {

    case ui_theme::classic:
        return classic_painters;

    case ui_theme::net_link:
        return net_link_painters;

    case ui_theme::pocket_lcd:
        return pocket_lcd_painters;

    case ui_theme::phosphor:
        return phosphor_painters;

    default:
        return field_guide_painters;
    }
}

const char* theme_name(ui_theme theme)
{
    switch(theme)
    {

    case ui_theme::classic:
        return "Classic Original";

    case ui_theme::net_link:
        return "Net Link";

    case ui_theme::pocket_lcd:
        return "Pocket LCD";

    case ui_theme::phosphor:
        return "Phosphor";

    default:
        return "Field Guide";
    }
}

const char* theme_value_label(ui_theme theme)
{
    switch(theme)
    {

    case ui_theme::classic:
        return "CLASSIC ORIGINAL";

    case ui_theme::net_link:
        return "NET LINK";

    case ui_theme::pocket_lcd:
        return "POCKET LCD";

    case ui_theme::phosphor:
        return "PHOSPHOR";

    default:
        return "FIELD GUIDE";
    }
}

ui_theme next_theme(ui_theme theme)
{
    for(int i = 0; i < ui_theme_count; ++i)
    {
        if(settings_order[i] == theme)
        {
            return settings_order[(i + 1) % ui_theme_count];
        }
    }

    return ui_theme::field_guide;
}

void home_state::prepare()
{
    display_order(accounts, order);

    if(selected >= accounts.count)
    {
        selected = accounts.count - 1;
    }

    if(selected < 0)
    {
        selected = 0;
    }
}

uint32_t home_state::code(int position)
{
    int64_t period = utc() / totp_period;

    if(period != _codes_period)
    {
        _codes_period = period;

        for(int i = 0; i < accounts.count; ++i)
        {
            _code_known[i] = false;
        }
    }

    int index = order[position];

    if(! _code_known[index])
    {
        const account& account_item = accounts.items[index];
        _codes[index] = totp::totp(account_item.secret, account_item.secret_size, utc(), totp_digits, totp_period);
        _code_known[index] = true;
    }

    return _codes[index];
}

void home_state::invalidate_codes()
{
    _codes_period = -1;
}

int home_state::menu_count() const
{
    return accounts.custom_order ? 6 : 5;
}

home_option home_state::menu_option(int index) const
{
    return accounts.custom_order ? custom_order_options[index] : sorted_options[index];
}

int keyboard_state::columns() const
{
    switch(layout)
    {

    case keyboard_layout::base32:
        return 8;

    case keyboard_layout::pin:
        return 9;

    default:
        return 10;
    }
}

char keyboard_state::key(int key_row, int key_column) const
{
    switch(layout)
    {

    case keyboard_layout::base32:
        return base32_rows[key_row][key_column];

    case keyboard_layout::pin:
        return pin_rows[key_row][key_column];

    default:
        return (lower_case ? name_lower_rows : name_upper_rows)[key_row][key_column];
    }
}

int secret_line(const secret_view& view, int start, int chars_per_line, bn::string<48>& line)
{
    line.clear();

    int groups_per_line = (chars_per_line + 1) / 5;

    if(groups_per_line < 1)
    {
        groups_per_line = 1;
    }

    int used = 0;

    for(int group = 0; group < groups_per_line; ++group)
    {
        int group_start = start + group * 4;

        if(group_start >= view.secret_size)
        {
            break;
        }

        if(group)
        {
            line += ' ';
        }

        for(int i = group_start; i < group_start + 4 && i < view.secret_size; ++i)
        {
            line += view.secret_text[i];
            ++used;
        }
    }

    return used;
}

}
