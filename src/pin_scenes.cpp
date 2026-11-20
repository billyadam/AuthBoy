#include "pin_scenes.h"

#include "bn_keypad.h"
#include "bn_vector.h"
#include "bn_sprite_ptr.h"

#include "entropy.h"
#include "keyboard.h"
#include "vault.h"

namespace app
{

namespace
{
    using sprite_vector = bn::vector<bn::sprite_ptr, 64>;

    // Draws a "please wait" screen and makes sure it's on screen before slow work starts.
    void show_busy(text_generators& text, sprite_vector& sprites, const char* line)
    {
        sprites.clear();
        text.small.generate(0, -8, line, sprites);
        text.small.generate(0, 12, "This takes a few seconds", sprites);
        frame_update();
        frame_update();
    }

    // Shows up to three lines and waits for A (returns true) or B (returns false).
    bool message_screen(text_generators& text, const char* line_1, const char* line_2, const char* line_3,
                        const char* buttons)
    {
        sprite_vector sprites;
        text.small.generate(0, -40, line_1, sprites);

        if(line_2)
        {
            text.small.generate(0, -16, line_2, sprites);
        }

        if(line_3)
        {
            text.small.generate(0, 4, line_3, sprites);
        }

        text.small.generate(0, 50, buttons, sprites);
        frame_update();

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
    bool confirm_current_pin(text_generators& text)
    {
        bn::string<64> pin;
        const char* message = nullptr;

        while(true)
        {
            wipe_string(pin);

            if(! run_keyboard(text, keyboard_layout::pin, "Current PIN", pin, vault::max_pin_chars, message))
            {
                return false;
            }

            if(pin.size() < vault::min_pin_chars)
            {
                message = "At least 6 characters";
                continue;
            }

            pin_result result;

            {
                sprite_vector sprites;
                show_busy(text, sprites, "Checking PIN...");
                result = verify_pin(pin.data(), pin.size());
            }

            if(result == pin_result::ok)
            {
                wipe_string(pin);
                return true;
            }

            message = result == pin_result::wrong_pin ? "Wrong PIN" : "Not enough memory";
        }
    }

    // Asks for a new PIN twice and encrypts the accounts with it.
    // If required, cancelling just asks again.
    bool choose_new_pin(text_generators& text, account_list& accounts, bool required)
    {
        bn::string<64> pin;
        bn::string<64> confirmation;
        const char* message = "6-16 characters, 8+ recommended";

        while(true)
        {
            wipe_string(pin);
            wipe_string(confirmation);

            if(! run_keyboard(text, keyboard_layout::pin, "New PIN", pin, vault::max_pin_chars, message))
            {
                if(required)
                {
                    message = "A PIN is required";
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
                        "Short PIN: 8+ is much harder to crack" : nullptr;

            if(! run_keyboard(text, keyboard_layout::pin, "Confirm PIN", confirmation, vault::max_pin_chars,
                              confirm_message))
            {
                message = "6-16 characters, 8+ recommended";
                continue;
            }

            if(! strings_equal(pin, confirmation))
            {
                message = "PINs didn't match, try again";
                continue;
            }

            break;
        }

        pin_result result;

        {
            sprite_vector sprites;
            show_busy(text, sprites, "Encrypting...");
            result = set_pin(pin.data(), pin.size(), accounts);
        }

        wipe_string(pin);
        wipe_string(confirmation);

        if(result != pin_result::ok)
        {
            message_screen(text, "Couldn't set the PIN", "Not enough memory", nullptr, "A: OK");
            return false;
        }

        bn::string<48> timing = "Unlocking takes about ";
        append_seconds(timing, last_kdf_milliseconds());
        message_screen(text, "PIN set, accounts encrypted", timing.c_str(),
                       "Forgotten PINs can't be recovered!", "A: OK");
        return true;
    }
}

void run_pin_setup_scene(text_generators& text, account_list& accounts)
{
    message_screen(text, "Welcome to AuthBoy", "Set a PIN to encrypt your accounts", nullptr, "A: continue");

    while(! pin_enabled())
    {
        choose_new_pin(text, accounts, true);
    }
}

void run_unlock_scene(text_generators& text, account_list& accounts)
{
    bn::string<64> pin;
    const char* message = nullptr;

    while(locked())
    {
        wipe_string(pin);

        if(! run_keyboard(text, keyboard_layout::pin, "Enter PIN", pin, vault::max_pin_chars, message))
        {
            message = nullptr;
            continue;
        }

        if(pin.size() < vault::min_pin_chars)
        {
            message = "At least 6 characters";
            continue;
        }

        pin_result result;

        {
            sprite_vector sprites;
            show_busy(text, sprites, "Unlocking...");
            result = unlock_accounts(pin.data(), pin.size(), accounts);
        }

        if(result == pin_result::wrong_pin)
        {
            message = "Wrong PIN";
        }
        else if(result == pin_result::out_of_memory)
        {
            message = "Not enough memory";
        }
    }

    wipe_string(pin);
}

void run_settings_menu(text_generators& text, account_list& accounts)
{
    enum option
    {
        option_sort,
        option_lock,
        option_change,
        option_count
    };

    constexpr const char* option_names[option_count] = { nullptr, "Lock now", "Change PIN" };

    int selected = 0;
    bool redraw = true;

    sprite_vector sprites;
    frame_update();

    while(true)
    {
        if(bn::keypad::up_pressed())
        {
            selected = selected == 0 ? option_count - 1 : selected - 1;
            redraw = true;
        }

        if(bn::keypad::down_pressed())
        {
            selected = selected == option_count - 1 ? 0 : selected + 1;
            redraw = true;
        }

        if(bn::keypad::b_pressed())
        {
            return;
        }

        if(bn::keypad::a_pressed())
        {
            switch(selected)
            {

            case option_sort:
                // Only changes how the list is shown; the custom order is kept.
                accounts.custom_order = ! accounts.custom_order;
                save_accounts(accounts);
                redraw = true;
                break;

            case option_lock:
                lock_accounts(accounts);
                return;

            case option_change:
                sprites.clear();

                if(confirm_current_pin(text))
                {
                    choose_new_pin(text, accounts, false);
                }

                return;

            default:
                return;
            }
        }

        if(redraw)
        {
            redraw = false;
            sprites.clear();

            text.small.generate(0, -66, "Settings", sprites);

            for(int i = 0; i < option_count; ++i)
            {
                bn::string<32> line = i == selected ? "> " : "  ";

                if(i == option_sort)
                {
                    line += accounts.custom_order ? "Sort: Custom" : "Sort: A-Z";
                }
                else
                {
                    line += option_names[i];
                }

                text.small.generate(0, -30 + i * 18, line, sprites);
            }

            text.small.generate(0, 66, "A: choose   B: back", sprites);
        }

        frame_update();
    }
}
}
