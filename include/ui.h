#ifndef UI_H
#define UI_H

// "Field Guide" UI (see the design handoff): everything is drawn in software into one
// full-screen 8bpp background canvas (30x20 unique tiles), so panels, text and modals can sit
// at exact pixel positions and overlap correctly. Changed tiles are copied to VRAM during V-Blank.
//
// Coordinates are native screen pixels: origin top-left, 240x160.

#include <cstdint>

#include "bn_string_view.h"

namespace ui
{
    // Palette indices. 0-15 follow the design's UI bank; the rest are its "extra" colors.
    enum color : uint8_t
    {
        transparent = 0,
        white = 1,              // #f8f8f8 panel fill, light text, key caps
        ink = 2,                // #404858 body text, borders, footer, pills
        bevel = 3,              // #c8d0d8 modal inner line, empty PIN slot
        info = 4,               // #d0e8f8 info box, keyboard panel, active case tab
        zebra = 5,              // #e0ecf8 alternate list rows
        stripe_a = 6,           // #f8f0d0 background stripe A
        stripe_b = 7,           // #f0e0b0 background stripe B
        red = 8,                // #d83828 header, danger, cursor, caret, key outline
        header_light = 9,       // #f87858 header top highlight
        header_shadow = 10,     // #982010 header shadow, title shadow
        selection = 11,         // #f8c838 selected row / item / key
        selection_edge = 12,    // #c87818 selection edge / bottom shade
        green = 13,             // #48d058 TIME bar (> 10 s), status dot
        secondary = 14,         // #6878a0 secondary text
        header_subtext = 15,    // #f8d0c0 date / UTC, "Welcome to"

        green_shine = 16,       // #a8f8a0
        yellow = 17,            // #f8c038 TIME bar 6-10 s
        bar_red = 18,           // #f05030 TIME bar <= 5 s
        red_shine = 19,         // #f8a890
        selection_shine = 20,   // #f8e8a0 selection shine, shadow under the big code
        light_shadow = 21,      // #d0d8e0 light text shadow, scrollbar track
        scroll_thumb = 22,      // #8890a0
        key_shade = 23,         // #8898b8 key cap bottom shade
        row_inner_shade = 24,   // #e8a020 selected row inner bottom
        footer_light = 25,      // #687890 footer top highlight
        danger_shadow = 26,     // #f8c8c0
        tab_inactive = 27,      // #a8b8c8
        tab_inactive_text = 28, // #68788f
        hero_accent = 29,       // #e85840
        input_shadow = 30,      // #c8d0e0 name input text shadow
        qr_dark = 31,           // #000000 QR Code modules (pure black for scanning contrast)

        badge_0 = 32,           // 8 badge colors: badge_0 + (hash % 8)
    };

    constexpr int no_shadow = -1;

    enum class font : uint8_t
    {
        body,       // Butano common variable 8x16 font glyphs (stands in for "font_body8")
        digits,     // Butano common fixed 8x16 font glyphs, 7 px advance (stands in for "font_digits8")
        code12,     // digits 0-9 only, 12 px tall / 10 px wide, built from "digits" (use scale 1)
    };

    enum class align : uint8_t
    {
        left,
        center,
        right,
    };

    // Allocates the canvas and background. Call once after bn::core::init().
    void init();

    // ---- Primitives (clipped to the screen) ----
    void fill(int x, int y, int width, int height, uint8_t color);
    void round_fill(int x, int y, int width, int height, int radius, uint8_t color);
    void stripes(int x, int y, int width, int height);
    void dotted_hline(int x, int y, int width, uint8_t color);

    // ---- Text ----
    // x is the left / center / right edge depending on alignment; y is the top of the 8 px line
    // (8 * scale tall). The shadow is the glyph shape offset by shadow_offset to the bottom right.
    int text_width(font text_font, const bn::string_view& text, int scale = 1);
    void text(font text_font, const bn::string_view& text, int x, int y, uint8_t color,
              int shadow = no_shadow, int scale = 1, align alignment = align::left, int shadow_offset = 1);

    // Shortens text with "..." so it fits in max_width pixels.
    bn::string_view fit_text(font text_font, const bn::string_view& text, int max_width, int scale,
                             bool& truncated);

    // ---- Widgets ----
    void background();                                  // full-screen stripes
    void header(const bn::string_view& title);          // band + title
    void header_band_clear(int x, int width);            // repaint part of the band (for changing text)
    void footer_bar(int top);                           // dark bar from top to the bottom of the screen

    struct hint
    {
        const char* button;     // "A" / "B" -> round button, anything else -> pill
        const char* label;
    };

    // One centered row of button hints in the footer; text_y is the label's top.
    void footer_hints(int text_y, const hint* hints, int count);

    // A centered message in the footer row (used for validation messages).
    void footer_message(int text_y, const bn::string_view& message);

    // 2 px ink border with rounded corners; modal adds the 1 px bevel line inside the border.
    void panel(int x, int y, int width, int height, uint8_t fill_color, bool modal);

    // Menu selection bar (15 px tall) and the bobbing cursor triangle.
    void selection_bar(int x, int y, int width);

    // One item of a menu inside a white modal box (account menu, settings).
    // bob is the cursor's 0/1 px animation offset.
    void menu_item(int box_x, int box_width, int item_y, const bn::string_view& label, uint8_t text_color,
                   int shadow, bool selected, int bob);

    // 0 or 1: the cursor triangle moves 1 px right for 16 frames, then back.
    int cursor_bob(int frame_counter);
    void cursor_right(int x, int y, uint8_t color);     // 5x8, pointing right
    void cursor_down(int x, int y, uint8_t color);      // 5x3, pointing down
    void cursor_up(int x, int y, uint8_t color);        // 5x3, pointing up

    void round_button(int x, int y, const char* letter, uint8_t fill_color, uint8_t letter_color);   // 10x10
    int pill(int x, int y, const char* label, uint8_t fill_color, uint8_t text_color);              // returns width
    int pill_width(const char* label);

    // Keyboard key cap; selected adds the yellow fill and red outline around it.
    void key_cap(int x, int y, int width, int height, char label, bool selected, uint8_t panel_color);

    // "Unlocking..." style box. The bar block is static (the CPU is busy while it's shown).
    void busy_box(const bn::string_view& title, const bn::string_view& subtitle);

    // Stable badge color for an account name.
    uint8_t badge_color(const char* name);
}

#endif
