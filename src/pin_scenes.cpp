#include "pin_scenes.h"

#include "bn_keypad.h"
#include "bn_string.h"

#include "app_text.h"
#include "clock_scene.h"
#include "entropy.h"
#include "keyboard.h"
#include "otp_export.h"
#include "theme.h"
#include "totp.h"
#include "vault.h"

namespace app
{

namespace
{
    constexpr int blink_frames = 30;

    // "Unlocking..." style screen, made visible before slow work (Argon2) starts.
    void show_busy(const char* title)
    {
        painters().screens.busy(title);
        frame_update();
        frame_update();
    }

    // After checking a password on the busy screen: the logo opens and holds for 20 frames, or
    // shakes 1 px left and right for 8 frames.
    void show_busy_result(pin_result result)
    {
        screen_painter& painter = painters().screens;

        if(result == pin_result::ok)
        {
            painter.busy_logo(true, 0);

            for(int frame = 0; frame < 20; ++frame)
            {
                frame_update();
            }
        }
        else if(result == pin_result::wrong_pin)
        {
            for(int frame = 0; frame < 8; ++frame)
            {
                painter.busy_logo(false, frame % 2 ? -1 : 1);
                frame_update();
            }
        }
    }

    // Message with up to three lines; waits for A (returns true) or B (returns false).
    bool message_screen(const char* line_1, const char* line_2, const char* line_3, const char* a_label)
    {
        screen_painter& painter = painters().screens;
        const char* lines[3] = { line_1, line_2, line_3 };
        int line_count = line_3 ? 3 : line_2 ? 2 : 1;
        painter.message(lines, line_count, a_label);
        frame_update();

        int blink_counter = 0;

        while(true)
        {
            if(bn::keypad::a_pressed())
            {
                return true;
            }

            if(bn::keypad::b_pressed())
            {
                return false;
            }

            ++blink_counter;

            if(blink_counter % blink_frames == 0)
            {
                painter.message_blink(lines, line_count, a_label, (blink_counter / blink_frames) % 2 == 0);
            }

            frame_update();
        }
    }

    // Title / welcome screen; waits for A or START.
    void welcome_screen(const char* line_1, const char* line_2)
    {
        screen_painter& painter = painters().screens;
        painter.welcome(line_1, line_2);
        frame_update();

        int blink_counter = 0;

        while(! bn::keypad::a_pressed() && ! bn::keypad::start_pressed())
        {
            ++blink_counter;

            if(blink_counter % blink_frames == 0)
            {
                painter.welcome_blink((blink_counter / blink_frames) % 2 == 0);
            }

            frame_update();
        }
    }

    bool strings_equal(const bn::string<64>& a, const bn::string<64>& b)
    {
        if(a.size() != b.size())
        {
            return false;
        }

        for(int i = 0; i < a.size(); ++i)
        {
            if(a[i] != b[i])
            {
                return false;
            }
        }

        return true;
    }

    template<int MaxSize>
    void append_seconds(bn::string<MaxSize>& text, int milliseconds)
    {
        int tenths = (milliseconds + 50) / 100;
        append_padded(text, tenths / 10, 1);
        text += '.';
        append_padded(text, tenths % 10, 1);
        text += " s";
    }

    // Asks for the current PIN and checks it. Returns false if the user gives up.
    bool confirm_current_pin()
    {
        bn::string<64> pin;
        const char* message = nullptr;

        while(true)
        {
            wipe_string(pin);

            if(! run_keyboard(keyboard_layout::pin, "Current password", pin, vault::max_pin_chars, message))
            {
                return false;
            }

            if(pin.size() < vault::min_pin_chars)
            {
                message = "At least 6 characters";
                continue;
            }

            show_busy("Checking...");
            pin_result result = verify_pin(pin.data(), pin.size());
            show_busy_result(result);

            if(result == pin_result::ok)
            {
                wipe_string(pin);
                return true;
            }

            message = result == pin_result::wrong_pin ? "Wrong password" : "Not enough memory";
        }
    }

    // Asks for a new PIN twice and encrypts the accounts with it.
    // If required, cancelling just asks again.
    bool choose_new_pin(account_list& accounts, bool required)
    {
        bn::string<64> pin;
        bn::string<64> confirmation;
        const char* message = "6-16 characters, 8+ recommended";

        while(true)
        {
            wipe_string(pin);
            wipe_string(confirmation);

            if(! run_keyboard(keyboard_layout::pin, "New password", pin, vault::max_pin_chars, message))
            {
                if(required)
                {
                    message = "A password is required";
                    continue;
                }

                return false;
            }

            if(pin.size() < vault::min_pin_chars)
            {
                message = "At least 6 characters";
                continue;
            }

            const char* confirm_message = pin.size() < vault::recommended_pin_chars ?
                        "Short password: 8+ is much safer" : nullptr;

            if(! run_keyboard(keyboard_layout::pin, "Confirm password", confirmation, vault::max_pin_chars,
                              confirm_message))
            {
                message = "6-16 characters, 8+ recommended";
                continue;
            }

            if(! strings_equal(pin, confirmation))
            {
                message = "Passwords didn't match, try again";
                continue;
            }

            break;
        }

        show_busy("Encrypting...");
        pin_result result = set_pin(pin.data(), pin.size(), accounts);
        wipe_string(pin);
        wipe_string(confirmation);

        if(result != pin_result::ok)
        {
            message_screen("Couldn't set the password", "Not enough memory", nullptr, "OK");
            return false;
        }

        bn::string<48> timing = "Unlocking takes about ";
        append_seconds(timing, last_kdf_milliseconds());
        message_screen("Password set, accounts encrypted", timing.c_str(), "Passwords can't be recovered!",
                       "OK");
        return true;
    }
}

void run_pin_setup_scene(account_list& accounts)
{
    welcome_screen("Set a password to encrypt", "your accounts");

    while(! pin_enabled())
    {
        choose_new_pin(accounts, true);
    }

    // Like setting the clock at the start of a new game. Skipped if there's no working RTC.
    run_set_clock_scene(accounts, true);
}

void show_message(const char* line_1, const char* line_2, const char* line_3, const char* a_label)
{
    message_screen(line_1, line_2, line_3, a_label);
}

void run_title_scene()
{
    welcome_screen("Welcome back!", "Enter your password to unlock");
}

void run_unlock_scene(account_list& accounts)
{
    bn::string<64> pin;
    const char* message = nullptr;

    while(locked())
    {
        wipe_string(pin);

        if(! run_keyboard(keyboard_layout::pin, "Enter password", pin, vault::max_pin_chars, message))
        {
            message = nullptr;
            continue;
        }

        if(pin.size() < vault::min_pin_chars)
        {
            message = "At least 6 characters";
            continue;
        }

        show_busy("Unlocking...");
        pin_result result = unlock_accounts(pin.data(), pin.size(), accounts);
        show_busy_result(result);

        if(result == pin_result::wrong_pin)
        {
            message = "Wrong password";
        }
        else if(result == pin_result::out_of_memory)
        {
            message = "Not enough memory";
        }
    }

    wipe_string(pin);
}

void run_show_secret(const account& item)
{
    if(! confirm_current_pin())
    {
        return;
    }

    // Static: kept off the small stack, and wiped as soon as the screen is drawn.
    static char uri[otp_export::max_uri_size];
    static uint8_t qrcode[otp_export::qr_buffer_size];
    static uint8_t temp[otp_export::qr_buffer_size];
    static char secret_text[72];

    int uri_size = otp_export::build_uri(item.label, item.secret, item.secret_size, uri, sizeof(uri));
    bool has_qr = uri_size > 0 && otp_export::make_qr(uri, uri_size, qrcode, temp);
    int secret_size = totp::base32_encode(item.secret, item.secret_size, secret_text, sizeof(secret_text));

    secret_view view = { item.label, has_qr ? qrcode : nullptr, secret_text, secret_size > 0 ? secret_size : 0 };
    painters().screens.show_secret(view);

    // Only the screen keeps it now.
    vault::wipe(uri, sizeof(uri));
    vault::wipe(qrcode, sizeof(qrcode));
    vault::wipe(temp, sizeof(temp));
    vault::wipe(secret_text, sizeof(secret_text));

    frame_update();

    while(! bn::keypad::b_pressed() && ! bn::keypad::a_pressed())
    {
        frame_update();
    }
}

void run_settings_menu(account_list& accounts)
{
    settings_state state = { accounts };
    painters().screens.settings(state);
    frame_update();

    while(true)
    {
        screen_painter& painter = painters().screens;
        int old_selected = state.selected;

        if(bn::keypad::up_pressed())
        {
            state.selected = state.selected == 0 ? settings_option_count - 1 : state.selected - 1;
        }

        if(bn::keypad::down_pressed())
        {
            state.selected = state.selected == settings_option_count - 1 ? 0 : state.selected + 1;
        }

        if(bn::keypad::b_pressed())
        {
            return;
        }

        if(bn::keypad::a_pressed())
        {
            switch(state.selected)
            {

            case settings_option_sort:
                // Only changes how the list is shown; the custom order is kept.
                // A-Z -> Z-A -> Custom -> A-Z
                if(accounts.custom_order)
                {
                    accounts.custom_order = false;
                    accounts.reverse_order = false;
                }
                else if(! accounts.reverse_order)
                {
                    accounts.reverse_order = true;
                }
                else
                {
                    accounts.custom_order = true;
                    accounts.reverse_order = false;
                }

                save_accounts(accounts);
                painter.settings_item(state, settings_option_sort);
                break;

            case settings_option_theme:
                accounts.theme = next_theme(accounts.theme);
                set_theme(accounts.theme);
                save_accounts(accounts);
                state.frame_counter = 0;
                painters().screens.settings(state);     // the whole screen in the new look
                frame_update();
                continue;

            case settings_option_clock:
                run_set_clock_scene(accounts, false);
                painters().screens.settings(state);
                frame_update();
                continue;

            case settings_option_lock:
                lock_accounts(accounts);
                return;

            case settings_option_change_password:
                if(confirm_current_pin())
                {
                    choose_new_pin(accounts, false);
                }

                return;

            default:
                return;
            }
        }

        if(state.selected != old_selected)
        {
            state.frame_counter = 0;
            painter.settings_item(state, old_selected);
            painter.settings_item(state, state.selected);
        }

        ++state.frame_counter;

        if(state.frame_counter % 16 == 0)
        {
            painter.settings_item(state, state.selected);
        }

        frame_update();
    }
}

}
