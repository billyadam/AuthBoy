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

        // 40..: Classic theme (the original look)
        classic_backdrop = 40,  // #101840 screen
        classic_panel = 41,     // #223678 popup panel
        classic_key = 42,       // #2850aa key cursor fill
        classic_key_edge = 43,  // #6ea0ff key cursor border
        black = qr_dark,        // Classic text outline

        // 44..: Pocket LCD
        lcd_screen = 44,          // #e8f0d0
        lcd_light = 45,           // #a8c890
        lcd_mid = 46,             // #507850
        lcd_dark = 47,            // #183020
        // Phosphor, then the same colors faded for the modal backdrop
        amber_bg = 48,            // #081008
        amber_track = 49,         // #302008
        amber_frame = 50,         // #806020
        amber_dim = 51,           // #a07828
        amber_text = 52,          // #f8b030
        amber_bg_faded = 53,      // #081008
        amber_track_faded = 54,   // #101008
        amber_frame_faded = 55,   // #202010
        amber_dim_faded = 56,     // #282810
        amber_text_faded = 57,    // #403010
        // Net Link, then the same colors (and the badges) faded for the modal backdrop
        net_text = 58,            // #f8f8f8
        net_deep = 59,            // #001830
        net_bg = 60,              // #082050
        net_grid = 61,            // #103878
        net_chip_dark = 62,       // #083878
        net_chip = 63,            // #0850a0
        net_chip_hi = 64,         // #2878d0
        net_selected = 65,        // #1878e0
        net_cyan = 66,            // #38d8f8
        net_shine = 67,           // #c0f8f8
        net_green = 68,           // #78f858
        net_secondary = 69,       // #78b8e8
        net_tag = 70,             // #f89830
        net_warn = 71,            // #f8d838
        net_danger = 72,          // #f85838
        net_danger_text = 73,     // #f89878
        net_text_faded = 74,      // #586878
        net_deep_faded = 75,      // #001830
        net_bg_faded = 76,        // #001838
        net_grid_faded = 77,      // #082048
        net_chip_dark_faded = 78, // #002048
        net_chip_faded = 79,      // #002858
        net_chip_hi_faded = 80,   // #103868
        net_selected_faded = 81,  // #083870
        net_cyan_faded = 82,      // #105878
        net_shine_faded = 83,     // #406878
        net_green_faded = 84,     // #286840
        net_secondary_faded = 85, // #285070
        net_tag_faded = 86,       // #584830
        net_warn_faded = 87,      // #585830
        net_danger_faded = 88,    // #583030
        net_danger_text_faded = 89,// #584848
        badge_0_net_faded = 90,   // #103870
        badge_1_net_faded = 91,   // #303070
        badge_2_net_faded = 92,   // #104878
        badge_3_net_faded = 93,   // #503830
        badge_4_net_faded = 94,   // #502830
        badge_5_net_faded = 95,   // #204838
        badge_6_net_faded = 96,   // #483058
        badge_7_net_faded = 97,   // #105058
        palette_size = 98,
    };

    constexpr int no_shadow = -1;

    enum class font : uint8_t
    {
        body,       // Butano common variable 8x16 font glyphs (stands in for "font_body8")
        digits,     // Butano common fixed 8x16 font glyphs, 7 px advance (stands in for "font_digits8")
        code12,     // digits 0-9 only, 12 px tall / 10 px wide, built from "digits" (use scale 1)
        body6,      // "body" with 6 px caps (one row dropped); footer labels (use scale 1)
        digits6,    // "digits" with 6 px caps; footer pills / buttons, header clock and date (use scale 1)
        title12,    // "body" at 12 px caps (between 1x and 2x); header titles (use scale 1)
    };

    enum class align : uint8_t
    {
        left,
        center,
        right,
    };

    // Symbols that no font has, drawn with fills by text() in every font (6 px tall, 1 px gap):
    // d-pad triangles for button pills and the plus-minus sign.
    constexpr char symbol_left = '\x01';            // 3x5
    constexpr char symbol_right = '\x02';           // 3x5
    constexpr char symbol_up = '\x03';              // 5x3
    constexpr char symbol_down = '\x04';            // 5x3
    constexpr char symbol_plus_minus = '\x05';      // 5x5
    constexpr const char* dpad_left_right = "\x01\x02";
    constexpr const char* dpad_up_down = "\x03\x04";

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

    void round_button(int x, int y, const char* letter, uint8_t fill_color, uint8_t letter_color,
                      font letter_font = font::digits);                                           // 10x10
    int pill(int x, int y, const char* label, uint8_t fill_color, uint8_t text_color,
             font label_font = font::digits);                                                     // returns width
    int pill_width(const char* label, font label_font = font::digits);

    // Keyboard key cap; selected adds the yellow fill and red outline around it.
    void key_cap(int x, int y, int width, int height, char label, bool selected, uint8_t panel_color);

    // "Unlocking..." style box. The bar block is static (the CPU is busy while it's shown).
    void busy_box(const bn::string_view& title, const bn::string_view& subtitle, int y = 70);

    // Stable badge color for an account name.
    uint8_t badge_color(const char* name);

    // Draws a QR Code (black modules, white quiet zone of 4 modules) with its left edge at x,
    // centered in [area_top, area_top + area_height) using the biggest whole-pixel module size
    // that fits (max_module px at most). Returns the side in pixels.
    int qr_code(const uint8_t* qrcode, int x, int area_top, int area_height, int max_module = 4);

    // Patterned lines: on pixels drawn every period pixels, starting at x / y.
    void pattern_hline(int x, int y, int width, int on, int period, uint8_t color);
    void pattern_vline(int x, int y, int height, int on, int period, uint8_t color);

    // Modal backdrops: fade = every theme color in the area replaced by its faded version;
    // checker = every other pixel (x + y odd) set to color.
    enum class fade_style : uint8_t
    {
        phosphor,
        net_link,
    };

    void fade(int x, int y, int width, int height, fade_style style);
    void checker(int x, int y, int width, int height, uint8_t color);

    // ---- Logo ("Lock Boy": a padlock whose body is a handheld) ----
    enum class logo_frame : uint8_t
    {
        closed,     // 16x16
        open,       // 16x16, shackle lifted
        big,        // 32x32, closed
        big_open,   // 32x32, shackle lifted
    };

    // Top-left at x, y. colors[1..6]: outline, shackle, body, body shade, screen, A button
    // (palette indices; colors[0] is unused, the background shows through).
    void logo(logo_frame frame, int x, int y, const uint8_t* colors);

    // ---- Classic theme text ----
    // Butano's 8x16 sprite fonts drawn whole, as the original sprite text was: white with the
    // fonts' own black outline. Coordinates are the original sprite ones: (0, 0) is the screen
    // center and y is the middle of the 16 px line (top = 72 + y on screen).
    enum class classic_font : uint8_t
    {
        small,      // variable width
        mono,       // 8 px per character
    };

    int classic_text_width(classic_font text_font, const bn::string_view& text);
    void classic_text(classic_font text_font, const bn::string_view& text, int x, int y,
                      align alignment = align::center);
}

#endif
