#include "theme.h"

#include "app_text.h"
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

const char* sort_label(const account_list& accounts)
{
    return accounts.custom_order ? "CUSTOM" : accounts.reverse_order ? "Z - A" : "A - Z";
}

bn::string<32> accounts_stored_label(const account_list& accounts)
{
    bn::string<32> text;
    append_padded(text, accounts.count, 1);
    text += accounts.count == 1 ? " account stored (max: " : " accounts stored (max: ";
    append_padded(text, max_accounts, 1);
    text += ')';
    return text;
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

bn::string<8> clock_cell_text(const clock_editor_state& state, int field)
{
    bn::string<8> text;
    const totp::civil_time& local = state.local;

    switch(field)
    {

    case clock_field_year:
        append_padded(text, local.year, 4);
        break;

    case clock_field_month:
        append_padded(text, local.month, 2);
        break;

    case clock_field_day:
        append_padded(text, local.day, 2);
        break;

    case clock_field_hour:
        append_padded(text, local.hour, 2);
        break;

    case clock_field_minute:
        append_padded(text, local.minute, 2);
        break;

    case clock_field_second:
        append_padded(text, local.second, 2);
        break;

    default:
        {
            int zone = state.zone_minutes;
            int abs_zone = zone < 0 ? -zone : zone;
            text += zone < 0 ? '-' : '+';
            append_padded(text, abs_zone / 60, 2);
            text += ':';
            append_padded(text, abs_zone % 60, 2);
        }
        break;
    }

    return text;
}

bn::string<32> clock_utc_readout(const clock_editor_state& state)
{
    int64_t utc = totp::unix_time(state.local) - int64_t(state.zone_minutes) * 60;
    bn::string<32> text = "= ";
    append_date_time(text, totp::civil_from_unix(utc));
    text += " UTC";
    return text;
}

namespace
{
    constexpr about_item how_it_works[about_item_count] = {
        { "Codes (TOTP, RFC 6238)", "HMAC-SHA-1, 6 digits, every 30 s" },
        { "Encryption", "XChaCha20-Poly1305" },
        { "Password key", "Argon2id, 128 KiB, 4 passes" },
        { "Saving", "2 SRAM slots, power-cut safe" },
    };

    constexpr about_item libraries[about_item_count] = {
        { "Game engine", "Butano 21.10.1 (zlib)" },
        { "Cryptography", "Monocypher 4.0.2 (BSD-2 / CC0)" },
        { "QR codes", "QR Code generator 1.8.0 (MIT)" },
        { "Toolchain", "devkitARM" },
    };
}

const char* const about_security_text[about_security_paragraph_count] = {
    "Your accounts are encrypted with your password, but AuthBoy hasn't had a professional "
    "security audit, so don't count on it being safer than other 2FA apps like, for example, "
    "Google Authenticator or Authy.",
    "A copied .sav / .srm lets someone guess passwords offline. Use a long, random, unique password.",
    "An emulator save state made while unlocked can contain unencrypted accounts, so avoid save states.",
    "Don't make this your only 2FA: keep a backup elsewhere, along with your recovery codes. "
    "AuthBoy can't recover a forgotten password.",
    "How it's encrypted: next page.",
};

const char* about_page_title(int page)
{
    switch(page)
    {

    case about_security_page:
    case about_security_page + 1:
        return "Security";

    case 3:
        return "How it works";

    case 4:
        return "Libraries";

    default:
        return "About";
    }
}

const about_item* about_page_items(int page)
{
    return page == 4 ? libraries : how_it_works;
}

bn::string<8> about_page_counter(int page)
{
    bn::string<8> text;
    append_padded(text, page + 1, 1);
    text += '/';
    append_padded(text, about_page_count, 1);
    return text;
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
