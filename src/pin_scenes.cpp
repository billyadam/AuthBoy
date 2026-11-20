#include "pin_scenes.h"

#include "bn_keypad.h"
#include "bn_string.h"

#include "app_text.h"
#include "entropy.h"
#include "keyboard.h"
#include "ui.h"
#include "vault.h"

namespace app
{

namespace
{
    constexpr int blink_frames = 30;

    // Header + "Unlocking..." style box for slow work (Argon2). The box is put on screen first,
    // then its bar animates from an interrupt until this object goes out of scope.
    class busy_screen
    {

    public:
        explicit busy_screen(const char* title)
        {
            ui::background();
            ui::header("AuthBoy");
            ui::busy_box(title, "This takes a few seconds");
            frame_update();
            frame_update();
            ui::busy_animation_start();
        }

        ~busy_screen()
        {
            ui::busy_animation_stop();
        }

        busy_screen(const busy_screen&) = delete;
        busy_screen& operator=(const busy_screen&) = delete;
    };

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

    // Dialogue box with up to three lines; waits for A (returns true) or B (returns false).
    bool message_screen(const char* line_1, const char* line_2, const char* line_3, const char* a_label)
    {
        const char* lines[3] = { line_1, line_2, line_3 };
        int line_count = line_3 ? 3 : line_2 ? 2 : 1;
        int height = 10 + line_count * 12 + 22;
        int x = 4, y = 156 - height, width = 232;
        int button_y = y + height - 18;

        ui::background();
        ui::header("AuthBoy");
        ui::panel(x, y, width, height, ui::white, true);

        for(int i = 0; i < line_count; ++i)
        {
            ui::text(ui::font::body, lines[i], x + 6, y + 10 + i * 12, ui::ink, ui::light_shadow);
        }

        draw_continue(x + width - 8, button_y, a_label, true);
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
                draw_continue(x + width - 8, button_y, a_label, (blink_counter / blink_frames) % 2 == 0);
            }

            frame_update();
        }
    }

    // Screen 10: first-run welcome.
    void welcome_screen()
    {
        ui::background();
        ui::fill(0, 0, 240, 76, ui::red);
        ui::fill(0, 1, 240, 1, ui::header_light);
        ui::fill(0, 64, 240, 4, ui::hero_accent);
        ui::fill(0, 76, 240, 2, ui::header_shadow);
        ui::text(ui::font::body, "Welcome to", 120, 18, ui::header_subtext, ui::no_shadow, 1, ui::align::center);
        ui::text(ui::font::body, "AuthBoy", 120, 28, ui::white, ui::header_shadow, 4, ui::align::center, 2);

        constexpr int x = 4, y = 100, width = 232, height = 56;
        ui::panel(x, y, width, height, ui::white, true);
        ui::text(ui::font::body, "Set a PIN to encrypt your accounts", x + 6, y + 10, ui::ink, ui::light_shadow);
        draw_continue(x + width - 8, y + 34, "continue", true);
        frame_update();

        int blink_counter = 0;

        while(! bn::keypad::a_pressed())
        {
            ++blink_counter;

            if(blink_counter % blink_frames == 0)
            {
                draw_continue(x + width - 8, y + 34, "continue", (blink_counter / blink_frames) % 2 == 0);
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

            if(! run_keyboard(keyboard_layout::pin, "Current PIN", pin, vault::max_pin_chars, message))
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
                busy_screen busy("Checking PIN...");
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
    bool choose_new_pin(account_list& accounts, bool required)
    {
        bn::string<64> pin;
        bn::string<64> confirmation;
        const char* message = "6-16 characters, 8+ recommended";

        while(true)
        {
            wipe_string(pin);
            wipe_string(confirmation);

            if(! run_keyboard(keyboard_layout::pin, "New PIN", pin, vault::max_pin_chars, message))
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

            if(! run_keyboard(keyboard_layout::pin, "Confirm PIN", confirmation, vault::max_pin_chars,
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
            busy_screen busy("Encrypting...");
            result = set_pin(pin.data(), pin.size(), accounts);
        }
        wipe_string(pin);
        wipe_string(confirmation);

        if(result != pin_result::ok)
        {
            message_screen("Couldn't set the PIN", "Not enough memory", nullptr, "OK");
            return false;
        }

        bn::string<48> timing = "Unlocking takes about ";
        append_seconds(timing, last_kdf_milliseconds());
        message_screen("PIN set, accounts encrypted", timing.c_str(), "Forgotten PINs can't be recovered!", "OK");
        return true;
    }
}

void run_pin_setup_scene(account_list& accounts)
{
    welcome_screen();

    while(! pin_enabled())
    {
        choose_new_pin(accounts, true);
    }
}

void run_unlock_scene(account_list& accounts)
{
    bn::string<64> pin;
    const char* message = nullptr;

    while(locked())
    {
        wipe_string(pin);

        if(! run_keyboard(keyboard_layout::pin, "Enter PIN", pin, vault::max_pin_chars, message))
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
            busy_screen busy("Unlocking...");
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

void run_settings_menu(account_list& accounts)
{
    enum option
    {
        option_sort,
        option_lock,
        option_change,
        option_count
    };

    constexpr const char* option_names[option_count] = { "Sort", "Lock now", "Change PIN" };
    constexpr ui::hint footer[] = { { "A", "choose" }, { "B", "back" } };

    // Screen 05 geometry
    constexpr int box_x = 4, box_y = 52, box_width = 232;
    constexpr int box_height = 9 + option_count * 18 + 1;

    int selected = 0;
    int frame_counter = 0;

    auto draw_item = [&](int index)
    {
        int item_y = box_y + 9 + index * 18;
        bool is_selected = index == selected;
        ui::menu_item(box_x, box_width, item_y, option_names[index], ui::ink, ui::light_shadow, is_selected,
                      is_selected ? ui::cursor_bob(frame_counter) : 0);

        if(index == option_sort)
        {
            const char* value = accounts.custom_order ? "CUSTOM" : "A-Z";
            int pill_x = box_x + box_width - 10 - ui::pill_width(value);
            ui::pill(pill_x, item_y - 1, value, ui::ink, ui::white);
        }
    };

    ui::background();
    ui::header("Settings");

    ui::panel(4, 28, 232, 18, ui::info, false);
    ui::round_fill(8, 33, 8, 8, 4, ui::green);
    ui::fill(10, 34, 4, 2, ui::green_shine);
    ui::text(ui::font::body, "PIN lock on: accounts encrypted", 18, 33, ui::ink);

    ui::panel(box_x, box_y, box_width, box_height, ui::white, true);

    for(int i = 0; i < option_count; ++i)
    {
        draw_item(i);
    }

    ui::footer_bar(144);
    ui::footer_hints(147, footer, 2);
    frame_update();

    while(true)
    {
        int old_selected = selected;

        if(bn::keypad::up_pressed())
        {
            selected = selected == 0 ? option_count - 1 : selected - 1;
        }

        if(bn::keypad::down_pressed())
        {
            selected = selected == option_count - 1 ? 0 : selected + 1;
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
                draw_item(option_sort);
                break;

            case option_lock:
                lock_accounts(accounts);
                return;

            case option_change:
                if(confirm_current_pin())
                {
                    choose_new_pin(accounts, false);
                }

                return;

            default:
                return;
            }
        }

        if(selected != old_selected)
        {
            frame_counter = 0;
            draw_item(old_selected);
            draw_item(selected);
        }

        ++frame_counter;

        if(frame_counter % 16 == 0)
        {
            draw_item(selected);
        }

        frame_update();
    }
}

}
