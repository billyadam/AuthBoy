#ifndef THEME_H
#define THEME_H

// Themes: the screens' logic (input, state, saving) lives in code_scene.cpp, keyboard.cpp and
// pin_scenes.cpp and is shared. How each screen looks is a theme's painters:
//   fg_*.cpp      Field Guide (the default)
//   classic_*.cpp Classic (the original look)
//   skin_*.cpp    Net Link, Pocket LCD and Phosphor: Field Guide's layout drawn by a skin
//                 (include/skin.h; net_link.cpp, pocket_lcd.cpp, phosphor.cpp)
// The logic tells the active theme's painter what changed; the painter redraws what it needs.

#include "bn_string.h"

#include "account_store.h"
#include "app_clock.h"

namespace app
{
    enum class keyboard_layout
    {
        name,       // letters (SELECT toggles case), digits, - . _ @ and R for space
        base32,     // A-Z and 2-7 only
        pin,        // A-Z and 0-9, typed characters hidden
    };

    // ---- Home screen (list of codes) ----

    enum class home_modal : uint8_t
    {
        none,
        menu,               // account options popup
        delete_confirm,
    };

    enum home_option : uint8_t
    {
        home_option_rename,
        home_option_change_key,
        home_option_show_secret,
        home_option_move,       // custom order only
        home_option_delete,
        home_option_cancel,
    };

    extern const char* const home_option_names[];

    struct home_state
    {
        static constexpr int totp_period = 30;
        static constexpr int totp_digits = 6;

        home_state(account_list& list, int& selected_position) :
            accounts(list),
            selected(selected_position)
        {
        }

        account_list& accounts;
        int& selected;                      // display position
        int order[max_accounts];            // display position -> stored index (see prepare)
        int first_visible = 0;              // set by the painter's layout
        int visible_count = 0;              // set by the painter's layout
        home_modal modal = home_modal::none;
        int menu_selected = 0;
        bool moving = false;                // custom order: Up/Down move the selected account
        int frame_counter = 0;              // restarts when the selection moves (cursor animation)
        int footer_message_frames = 0;      // > 0: "List full" is shown
        rtc_reading rtc = {};

        // Refreshes order and keeps selected in range. Painters call it from layout.
        void prepare();

        int64_t utc() const
        {
            return utc_time(rtc);
        }

        // Seconds left in the current 30 s period.
        int remaining_seconds() const
        {
            return totp_period - int(utc() % totp_period);
        }

        const account& item(int position) const
        {
            return accounts.items[order[position]];
        }

        // TOTP code of the account at a display position (computed once per period).
        uint32_t code(int position);
        void invalidate_codes();

        int menu_count() const;
        home_option menu_option(int index) const;

    private:
        uint32_t _codes[max_accounts];
        bool _code_known[max_accounts];
        int64_t _codes_period = -1;
    };

    class home_painter
    {

    public:
        // Sets first_visible / visible_count so the selected row is visible (calls prepare).
        virtual void layout(home_state& state) = 0;
        virtual void draw_all(home_state& state) = 0;
        virtual void second_passed(home_state& state) = 0;              // clock and countdown
        virtual void codes_changed(home_state& state) = 0;              // a new 30 s period
        virtual void list_changed(home_state& state) = 0;               // accounts, mode or modal closed
        virtual void selection_moved(home_state& state, int old_selected) = 0;  // or two rows swapped
        virtual void modal_opened(home_state& state) = 0;
        virtual void menu_moved(home_state& state, int old_menu_selected) = 0;
        virtual void footer_changed(home_state& state) = 0;             // "List full" shown / hidden
        virtual void animate(home_state& state) = 0;                    // every frame

    protected:
        ~home_painter() = default;
    };

    // ---- On-screen keyboard ----

    struct keyboard_state
    {
        static constexpr int rows = 4;

        keyboard_state(keyboard_layout keyboard, const char* screen_title, const bn::string<64>& text,
                       int max_text_chars) :
            layout(keyboard),
            title(screen_title),
            value(text),
            max_chars(max_text_chars)
        {
        }

        keyboard_layout layout;
        const char* title;
        const bn::string<64>& value;
        int max_chars;
        int row = 0;
        int column = 0;
        bool lower_case = false;
        bool caret_visible = true;
        const char* message = nullptr;      // replaces the button hints until the next key press

        int columns() const;
        char key(int key_row, int key_column) const;

        char selected_key() const
        {
            return key(row, column);
        }
    };

    class keyboard_painter
    {

    public:
        virtual void draw_all(const keyboard_state& state) = 0;
        virtual void key_moved(const keyboard_state& state, int old_row, int old_column) = 0;
        virtual void case_changed(const keyboard_state& state) = 0;
        virtual void text_changed(const keyboard_state& state) = 0;
        virtual void caret_changed(const keyboard_state& state) = 0;
        virtual void message_changed(const keyboard_state& state) = 0;

    protected:
        ~keyboard_painter() = default;
    };

    // ---- Other screens ----

    enum settings_option : uint8_t
    {
        settings_option_sort,
        settings_option_theme,
        settings_option_lock,
        settings_option_change_password,
        settings_option_count
    };

    struct settings_state
    {
        const account_list& accounts;
        int selected = 0;
        int frame_counter = 0;
    };

    // Show secret screen: the otpauth:// QR Code (if it fits) and the Base32 secret.
    struct secret_view
    {
        const char* label;
        const uint8_t* qrcode;      // nullptr: no QR Code
        const char* secret_text;
        int secret_size;
    };

    // Splits the secret into lines of 4-character groups, as many groups as fit in chars_per_line.
    // Returns the number of characters used from start (0 when done).
    int secret_line(const secret_view& view, int start, int chars_per_line, bn::string<48>& line);

    class screen_painter
    {

    public:
        // "Unlocking..." style screen shown during slow work (the CPU is busy, nothing animates).
        virtual void busy(const char* title) = 0;

        // The busy screen's logo after the work: open (password accepted), or closed and moved
        // shake px sideways (wrong password).
        virtual void busy_logo(bool open, int shake) = 0;

        // Message with 1-3 lines and an A button label; blink(false) hides the "next" arrow.
        virtual void message(const char* const* lines, int line_count, const char* a_label) = 0;
        virtual void message_blink(const char* const* lines, int line_count, const char* a_label,
                                   bool arrow_visible) = 0;

        // Title / welcome screen: the big AuthBoy title over a two-line dialog ("Set a password..."
        // on first run, "Welcome back!" at boot).
        virtual void welcome(const char* line_1, const char* line_2) = 0;
        virtual void welcome_blink(bool arrow_visible) = 0;

        virtual void show_secret(const secret_view& view) = 0;

        virtual void settings(const settings_state& state) = 0;
        virtual void settings_item(const settings_state& state, int index) = 0;

    protected:
        ~screen_painter() = default;
    };

    // ---- Theme selection ----

    struct theme_painters
    {
        home_painter& home;
        keyboard_painter& keyboard;
        screen_painter& screens;
    };

    void set_theme(ui_theme theme);
    ui_theme current_theme();
    const theme_painters& painters();

    const char* theme_name(ui_theme theme);         // "Field Guide", "Classic Original", ...
    const char* theme_value_label(ui_theme theme);  // "FIELD GUIDE", "CLASSIC ORIGINAL", ... (Settings value)

    // The theme after this one in Settings: Classic, Field Guide, Net Link, Pocket LCD, Phosphor.
    ui_theme next_theme(ui_theme theme);

    // Settings value for the sort order: "A - Z", "Z - A" or "CUSTOM".
    const char* sort_label(const account_list& accounts);

    namespace field_guide
    {
        home_painter& home();
        keyboard_painter& keyboard();
        screen_painter& screens();
    }

    namespace classic
    {
        home_painter& home();
        keyboard_painter& keyboard();
        screen_painter& screens();
    }

    namespace net_link
    {
        home_painter& home();
        keyboard_painter& keyboard();
        screen_painter& screens();
    }

    namespace pocket_lcd
    {
        home_painter& home();
        keyboard_painter& keyboard();
        screen_painter& screens();
    }

    namespace phosphor
    {
        home_painter& home();
        keyboard_painter& keyboard();
        screen_painter& screens();
    }
}

#endif
