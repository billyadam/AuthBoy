#ifndef SKIN_H
#define SKIN_H

// Skinned themes (Net Link, Pocket LCD, Phosphor): re-skins of Field Guide. They share its
// layout (the handoffs' "Shared geometry"), so the painters in skin_home.cpp,
// skin_keyboard.cpp and skin_screens.cpp do the layout and partial redraws, and a skin only
// draws the pieces.

#include "bn_string_view.h"

#include "theme.h"
#include "ui.h"

namespace app::skin
{
    // ---- Shared geometry (native pixels, top-left) ----
    constexpr int list_x = 4, list_y = 41, list_width = 232, list_height = 99;
    constexpr int rows_top = 46;
    constexpr int rows_bottom = 138;            // exclusive: rows must end above this
    constexpr int row_x = 7;
    constexpr int row_height = 15;
    constexpr int selected_row_height = 23;
    constexpr int row_gap = 1;
    constexpr int row_width_scrolling = 216;
    constexpr int row_width_full = 226;
    constexpr int scrollbar_top = 46, scrollbar_height = 87;
    constexpr int footer_top = 144;

    // Text colors (palette indices) used by the shared painters.
    struct text_colors
    {
        uint8_t text;           // on the screen background
        uint8_t dim;            // secondary text on the screen
        uint8_t box;            // inside boxes
        uint8_t box_dim;        // secondary text inside boxes
        uint8_t danger;         // danger labels (Delete)
        uint8_t hint;           // footer hint labels
        uint8_t input;          // typed text
        uint8_t caret;
        uint8_t guide;          // dotted guide lines of the secret key input
    };

    struct row_info
    {
        const account& item;
        const char* code;       // "123 456"
        int x;                  // row_x
        int y;
        int width;              // row_width_scrolling / row_width_full
        bool selected;
        bool moving;            // custom order: the selected row is being moved
        int bob;                // cursor animation offset (0 / 1)
    };

    struct menu_item_info
    {
        int box_x;              // the box the item is in
        int box_width;
        int y;                  // top of the 15 px bar
        const char* label;
        const char* value;      // right-aligned value (Sort / Theme), or nullptr
        bool selected;
        bool danger;
        int bob;
    };

    class skin
    {

    public:
        constexpr explicit skin(const text_colors& text) :
            colors(text)
        {
        }

        const text_colors colors;

        // ---- Screen ----
        virtual void background(int x, int y, int width, int height) const = 0;
        virtual void modal_backdrop(int x, int y, int width, int height) const = 0;

        // ---- Header (y 0-23) ----
        // home: the home screen header (title + clock), else a screen title (+ counter).
        virtual void header(const bn::string_view& title, bool home) const = 0;
        virtual void header_clock(const bn::string_view& time, const bn::string_view& date) const = 0;
        virtual void header_counter(const bn::string_view& counter) const = 0;

        // ---- Home ----
        // TIME row (y 26-40), background included. label_on: false hides the seconds label
        // (low time blink, only if blinks_low_time).
        virtual void time_bar(int remaining, bool label_on) const = 0;
        virtual bool blinks_low_time() const = 0;

        virtual void list_area() const = 0;                                    // 4,41 232x99
        virtual void list_band(int y, int height, bool scrolling) const = 0;   // clears rows only
        virtual void row(const row_info& info) const = 0;
        virtual void row_separator(int x, int y, int width) const;              // between unselected rows
        virtual bool has_cursor() const;                                        // animated row cursor
        virtual void row_cursor(int y, int bob, bool moving) const;            // selected row at y
        virtual void scrollbar(int first, int visible, int total) const = 0;

        // ---- Footer ----
        virtual void footer_bar(int top) const = 0;
        virtual int pill_width(const char* label) const = 0;
        virtual int pill(int x, int label_y, const char* label, bool danger) const = 0;   // returns width

        // ---- Boxes and menus ----
        virtual int box_border() const = 0;
        virtual uint8_t box_fill() const = 0;
        virtual void box(int x, int y, int width, int height) const = 0;     // with its shadow
        virtual int menu_inset() const = 0;          // bar x from the box's left edge
        virtual int menu_right_inset() const = 0;    // bar end from the box's right edge
        virtual void menu_item(const menu_item_info& info) const = 0;
        virtual void info_box(const bn::string_view& text) const = 0;     // settings, 4,28 232x18
        // Settings box: space between the border and the first / last item.
        virtual int settings_top_padding() const;       // 5
        virtual int settings_bottom_padding() const;    // 4
        virtual void down_arrow(int x, int y, bool visible) const;          // "continue" arrow, 7x4

        // ---- Unlocking / welcome ----
        virtual void busy(const bn::string_view& title, const bn::string_view& subtitle) const = 0;
        virtual void welcome_band() const = 0;          // everything above the dialog box, logo included

        // Logo colors (palette indices): unused, outline, shackle, body, body shade, screen, A button.
        virtual const uint8_t* logo_colors() const = 0;

        // ---- Set clock (layout in clock_layout.h; text in colors.box / box_dim) ----
        // Info box with the status dot (paused / running), 4,28 232x18.
        virtual void clock_info(const bn::string_view& text, bool running) const = 0;
        virtual void clock_panel(int x, int y, int width, int height) const = 0;     // 4,50 232x80
        // Draws a cell's background (and the selection rings around it); returns its text color.
        virtual uint8_t clock_cell(int x, int y, int width, int height, bool selected) const = 0;
        virtual uint8_t clock_arrow_color() const = 0;
        // PAUSED / RUNNING tag, right-aligned at x 234 in the header (clears its area first).
        // visible: false during the running tag's blink (themes that blink it).
        virtual void clock_tag(bool running, bool visible) const = 0;
        virtual bool clock_upper_labels() const;        // DATE / TIME / ZONE

        // ---- Keyboard ----
        virtual void input_box(int x, int y, int width, int height) const = 0;
        virtual void pin_slot(int x, int y, bool filled) const = 0;           // 9x9 cell
        virtual void name_input(int x, int y, int width, int height, const bn::string_view& text,
                                bool caret_visible) const = 0;
        virtual void key_panel(int x, int y, int width, int height) const = 0;
        virtual void key(int x, int y, int width, int height, char label, bool selected) const = 0;
        virtual void case_tabs(bool lower_case) const = 0;     // right-aligned at x 236, y 53

    protected:
        ~skin() = default;
    };

    // ---- Helpers for skins ----

    // Upper case copy (titles of the LCD / Phosphor themes).
    bn::string<32> upper(const bn::string_view& text);

    // Shortens with "..." to fit max_width.
    bn::string<40> fitted(ui::font text_font, const bn::string_view& text, int max_width);

    // Segmented bar: on px of color every period px, from x for width px.
    void segments(int x, int y, int width, int height, int on, int period, uint8_t color);

    // Down-pointing triangle, 7x4.
    void triangle_down(int x, int y, uint8_t color);

    // 1 px rectangle outline.
    void outline(int x, int y, int width, int height, uint8_t color);

    // Scrollbar thumb position and height in the shared scrollbar track.
    void scroll_thumb(int first, int visible, int total, int& thumb_y, int& thumb_height);

    // Parallelogram: top edge starts slant px right of x, bottom edge ends slant px before
    // x + width (or a right-slanted end only, with left_straight).
    void parallelogram(int x, int y, int width, int height, int slant, uint8_t color, bool left_straight = false);

    // ---- Painters ----

    class skin_home_painter final : public home_painter
    {

    public:
        constexpr explicit skin_home_painter(const skin& look) :
            _skin(look)
        {
        }

        void layout(home_state& state) override;
        void draw_all(home_state& state) override;
        void second_passed(home_state& state) override;
        void codes_changed(home_state& state) override;
        void list_changed(home_state& state) override;
        void selection_moved(home_state& state, int old_selected) override;
        void modal_opened(home_state& state) override;
        void menu_moved(home_state& state, int old_menu_selected) override;
        void footer_changed(home_state& state) override;
        void animate(home_state& state) override;

    private:
        const skin& _skin;
        int _blink_counter = 0;
        bool _label_on = true;

        void draw_clock(const home_state& state);
        void draw_time_bar(const home_state& state);
        void draw_row(home_state& state, int position, int y);
        void draw_separators(const home_state& state, int first, int last);
        void draw_list(home_state& state);
        void draw_footer(const home_state& state);
        void draw_menu_item(const home_state& state, int index);
        void draw_modal(const home_state& state);
        void redraw_modal_over_top(const home_state& state);
    };

    class skin_keyboard_painter final : public keyboard_painter
    {

    public:
        constexpr explicit skin_keyboard_painter(const skin& look) :
            _skin(look)
        {
        }

        void draw_all(const keyboard_state& state) override;
        void key_moved(const keyboard_state& state, int old_row, int old_column) override;
        void case_changed(const keyboard_state& state) override;
        void text_changed(const keyboard_state& state) override;
        void caret_changed(const keyboard_state& state) override;
        void message_changed(const keyboard_state& state) override;

    private:
        const skin& _skin;

        void draw_counter(const keyboard_state& state);
        void draw_input(const keyboard_state& state);
        void draw_keyboard(const keyboard_state& state);
        void draw_key(const keyboard_state& state, int row, int column);
        void draw_footer(const keyboard_state& state);
    };

    class skin_screen_painter final : public screen_painter
    {

    public:
        constexpr explicit skin_screen_painter(const skin& look) :
            _skin(look)
        {
        }

        void busy(const char* title) override;
        void busy_logo(bool open, int shake) override;
        void message(const char* const* lines, int line_count, const char* a_label) override;
        void message_blink(const char* const* lines, int line_count, const char* a_label,
                           bool arrow_visible) override;
        void welcome(const char* line_1, const char* line_2) override;
        void welcome_blink(bool arrow_visible) override;
        void show_secret(const secret_view& view) override;
        void settings(const settings_state& state) override;
        void settings_item(const settings_state& state, int index) override;
        void set_clock(const clock_editor_state& state) override;
        void set_clock_changed(const clock_editor_state& state) override;

    private:
        const skin& _skin;
    };

    // Footer hints: one centered row; button "A" / "START"... in a pill, then the label.
    struct hint
    {
        const char* button;
        const char* label;
    };

    void footer_hints(const skin& look, int label_y, const hint* hints, int count);
    void footer_message(const skin& look, int label_y, const bn::string_view& message);

    // Logo and title in one row centered as a group, top y 28 (Welcome of LCD / Phosphor).
    void logo_title_row(const skin& look, const char* title, int scale, uint8_t color);

    // "[A] label" + arrow, right-aligned to right_x (inside a box).
    void continue_hint(const skin& look, int right_x, int label_y, const char* label, bool arrow_visible);
}

#endif
