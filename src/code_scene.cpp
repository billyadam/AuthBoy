#include "code_scene.h"

#include "bn_core.h"
#include "bn_keypad.h"
#include "bn_vector.h"
#include "bn_sprite_ptr.h"

#include "bn_sprite_items_popup_panel.h"

#include "app_clock.h"
#include "entropy.h"
#include "totp.h"

namespace app
{

namespace
{
    constexpr int totp_period = 30;
    constexpr int totp_digits = 6;
    constexpr int bar_slots = 15;

    // List layout: 5 rows of "> label (16 chars)  123 456" in the 8px mono font.
    constexpr int visible_rows = 5;
    constexpr int first_row_y = -30;
    constexpr int row_spacing = 18;
    constexpr int row_chars = 2 + max_label_chars + 2 + 7;
    constexpr int row_left_x = -(row_chars * 8) / 2;

    // Popup menu shown with A over the list. Move only exists in custom order.
    enum popup_option
    {
        popup_rename,
        popup_change_key,
        popup_move,
        popup_delete,
        popup_cancel,
    };

    constexpr const char* popup_option_names[] = { "Rename", "Change key", "Move", "Delete", "Cancel" };
    constexpr popup_option custom_order_options[] = {
        popup_rename, popup_change_key, popup_move, popup_delete, popup_cancel
    };
    constexpr popup_option sorted_options[] = { popup_rename, popup_change_key, popup_delete, popup_cancel };

    // Draw order: lower z_order is drawn on top.
    constexpr int popup_text_z_order = 0;
    constexpr int popup_panel_z_order = 1;
    constexpr int list_z_order = 2;

    using popup_sprite_vector = bn::vector<bn::sprite_ptr, 48>;

    // One step on press, then repeats while held.
    bool repeat(bool held, int& held_frames)
    {
        if(! held)
        {
            held_frames = 0;
            return false;
        }

        ++held_frames;
        return held_frames == 1 || (held_frames > 18 && (held_frames - 18) % 5 == 0);
    }

    template<int MaxSize>
    void append_clock(bn::string<MaxSize>& text, const rtc_reading& rtc)
    {
        append_date_time(text, totp::civil_from_unix(rtc.local_time));
        text += "  ";
        append_utc_offset(text, utc_offset_minutes);
    }

    template<int MaxSize>
    void append_code(bn::string<MaxSize>& text, const account& account, int64_t utc)
    {
        uint32_t code = totp::totp(account.secret, account.secret_size, utc, totp_digits, totp_period);
        append_padded(text, int(code / 1000), 3);
        text += ' ';
        append_padded(text, int(code % 1000), 3);
    }

    void draw_popup(text_generators& text, const account& item, const popup_option* options, int option_count,
                    int selected_option, popup_sprite_vector& sprites)
    {
        sprites.clear();

        // 3x4 tiles of 64x32 = a 192x128 panel centered on screen
        for(int y = -48; y <= 48; y += 32)
        {
            for(int x = -64; x <= 64; x += 64)
            {
                bn::sprite_ptr tile = bn::sprite_items::popup_panel.create_sprite(x, y);
                tile.set_z_order(popup_panel_z_order);
                sprites.push_back(bn::move(tile));
            }
        }

        text.small.set_z_order(popup_text_z_order);
        text.small.generate(0, -50, item.label, sprites);
        text.small.set_left_alignment();

        for(int i = 0; i < option_count; ++i)
        {
            bn::string<24> line = i == selected_option ? "> " : "   ";
            line += popup_option_names[options[i]];
            text.small.generate(-36, -26 + i * 16, line, sprites);
        }

        text.small.set_center_alignment();
    }
}

code_scene_action run_code_scene(text_generators& text, account_list& accounts, int& selected)
{
    bn::vector<bn::sprite_ptr, 96> sprites;
    popup_sprite_vector popup_sprites;

    int64_t last_drawn_time = -1;
    rtc_status last_status = rtc_status::ok;
    bool redraw = true;
    bool confirming_delete = false;
    bool moving = false;
    bool popup_open = false;
    bool popup_redraw = false;
    int popup_selected = 0;
    int first_visible = 0;
    int up_frames = 0;
    int down_frames = 0;
    int order[max_accounts];

    // Let one frame pass so the button that closed the previous screen isn't seen again here.
    frame_update();

    while(true)
    {
        if(selected >= accounts.count)
        {
            selected = accounts.count - 1;
        }

        if(selected < 0)
        {
            selected = 0;
        }

        display_order(accounts, order);

        const popup_option* popup_options = accounts.custom_order ? custom_order_options : sorted_options;
        int popup_option_count = accounts.custom_order ? 5 : 4;

        // Input
        if(confirming_delete)
        {
            if(bn::keypad::a_pressed())
            {
                remove_account(accounts, order[selected]);
                save_accounts(accounts);
                confirming_delete = false;
                redraw = true;
            }
            else if(bn::keypad::b_pressed())
            {
                confirming_delete = false;
                redraw = true;
            }
        }
        else if(moving)
        {
            // Custom order only: the display order is the stored order.
            if(repeat(bn::keypad::up_held(), up_frames) && selected > 0)
            {
                swap_accounts(accounts, selected, selected - 1);
                --selected;
                redraw = true;
            }

            if(repeat(bn::keypad::down_held(), down_frames) && selected < accounts.count - 1)
            {
                swap_accounts(accounts, selected, selected + 1);
                ++selected;
                redraw = true;
            }

            if(bn::keypad::a_pressed() || bn::keypad::b_pressed())
            {
                moving = false;
                save_accounts(accounts);
                redraw = true;
            }
        }
        else if(popup_open)
        {
            if(bn::keypad::up_pressed())
            {
                popup_selected = popup_selected == 0 ? popup_option_count - 1 : popup_selected - 1;
                popup_redraw = true;
            }

            if(bn::keypad::down_pressed())
            {
                popup_selected = popup_selected == popup_option_count - 1 ? 0 : popup_selected + 1;
                popup_redraw = true;
            }

            bool close = bn::keypad::b_pressed();

            if(bn::keypad::a_pressed())
            {
                switch(popup_options[popup_selected])
                {

                case popup_rename:
                    return code_scene_action::rename_account;

                case popup_change_key:
                    return code_scene_action::change_secret;

                case popup_move:
                    moving = true;
                    redraw = true;
                    break;

                case popup_delete:
                    confirming_delete = true;
                    redraw = true;
                    break;

                default:
                    break;
                }

                close = true;
            }

            if(close)
            {
                popup_open = false;
                popup_sprites.clear();
            }
        }
        else
        {
            if(bn::keypad::start_pressed() && accounts.count < max_accounts)
            {
                return code_scene_action::add_account;
            }

            if(bn::keypad::select_pressed())
            {
                return code_scene_action::settings_menu;
            }

            if(accounts.count > 0)
            {
                if(repeat(bn::keypad::up_held(), up_frames))
                {
                    selected = selected == 0 ? accounts.count - 1 : selected - 1;
                    redraw = true;
                }

                if(repeat(bn::keypad::down_held(), down_frames))
                {
                    selected = selected == accounts.count - 1 ? 0 : selected + 1;
                    redraw = true;
                }

                if(bn::keypad::a_pressed())
                {
                    popup_open = true;
                    popup_selected = 0;
                    popup_redraw = true;
                }
            }
        }

        // Keep the selected row on screen
        if(selected < first_visible)
        {
            first_visible = selected;
        }
        else if(selected >= first_visible + visible_rows)
        {
            first_visible = selected - visible_rows + 1;
        }

        if(first_visible > accounts.count - visible_rows)
        {
            first_visible = accounts.count > visible_rows ? accounts.count - visible_rows : 0;
        }

        rtc_reading rtc = read_rtc();

        if(rtc.status != last_status)
        {
            redraw = true;
            last_status = rtc.status;
        }

        if(rtc.status == rtc_status::ok && utc_time(rtc) != last_drawn_time)
        {
            redraw = true;
        }

        if(popup_open && popup_redraw && accounts.count > 0)
        {
            popup_redraw = false;
            draw_popup(text, accounts.items[order[selected]], popup_options, popup_option_count, popup_selected,
                       popup_sprites);
        }

        if(! redraw)
        {
            frame_update();
            continue;
        }

        redraw = false;
        sprites.clear();
        display_order(accounts, order);

        if(rtc.status != rtc_status::ok)
        {
            if(rtc.status == rtc_status::missing)
            {
                text.small.generate(0, -8, "RTC not found", sprites);
                text.small.generate(0, 12, "Needs a cart/emulator with a clock", sprites);
            }
            else
            {
                text.small.generate(0, -8, "RTC returned an invalid time", sprites);
                text.small.generate(0, 12, "Set it with your flashcart's tools", sprites);
            }

            frame_update();
            continue;
        }

        if(confirming_delete)
        {
            bn::string<48> question = "Delete \"";
            question += accounts.items[order[selected]].label;
            question += "\"?";

            text.small.generate(0, -12, question, sprites);
            text.small.generate(0, 20, "A: delete   B: keep", sprites);
            frame_update();
            continue;
        }

        int64_t utc = utc_time(rtc);
        last_drawn_time = utc;

        // Header: clock and shared countdown
        bn::string<40> clock_text;
        append_clock(clock_text, rtc);
        text.small.generate(0, -70, clock_text, sprites);

        int remaining = totp_period - int(utc % totp_period);
        int filled = (remaining * bar_slots + totp_period - 1) / totp_period;
        bn::string<32> bar_text;

        for(int i = 0; i < bar_slots; ++i)
        {
            bar_text += i < filled ? '#' : '-';
        }

        bar_text += ' ';
        append_padded(bar_text, remaining, 2);
        bar_text += 's';
        text.mono.generate(0, -52, bar_text, sprites);

        if(accounts.count == 0)
        {
            text.small.generate(0, -4, "No accounts yet", sprites);
            text.small.generate(0, 16, "START: add an account", sprites);
            text.small.generate(0, 66, "SELECT: settings", sprites);
            frame_update();
            continue;
        }

        // Account rows
        text.mono.set_left_alignment();

        for(int row = 0; row < visible_rows; ++row)
        {
            int position = first_visible + row;

            if(position >= accounts.count)
            {
                break;
            }

            const account& item = accounts.items[order[position]];
            bn::string<row_chars + 4> row_text;

            if(position != selected)
            {
                row_text += "  ";
            }
            else
            {
                row_text += moving ? "* " : "> ";
            }

            int label_chars = 0;

            for(; label_chars < max_label_chars && item.label[label_chars]; ++label_chars)
            {
                row_text += item.label[label_chars];
            }

            for(; label_chars < max_label_chars; ++label_chars)
            {
                row_text += ' ';
            }

            row_text += "  ";
            append_code(row_text, item, utc);
            text.mono.generate(row_left_x, first_row_y + row * row_spacing, row_text, sprites);
        }

        text.mono.set_center_alignment();

        // Footer: scroll hints and controls
        bn::string<48> footer_text;
        footer_text += first_visible > 0 ? "^   " : "    ";
        footer_text += moving ? "Up/Down: move   A: done" : "START: add   SELECT: settings";
        footer_text += first_visible + visible_rows < accounts.count ? "   v" : "    ";
        text.small.generate(0, 66, footer_text, sprites);

        // Keep the list behind the popup
        for(bn::sprite_ptr& sprite : sprites)
        {
            sprite.set_z_order(list_z_order);
        }

        frame_update();
    }
}

}
