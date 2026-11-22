#include "ui.h"

#include "bn_core.h"
#include "bn_tile.h"
#include "bn_memory.h"
#include "bn_size.h"
#include "bn_optional.h"
#include "bn_bg_palette_ptr.h"
#include "bn_bg_palette_item.h"
#include "bn_regular_bg_ptr.h"
#include "bn_regular_bg_map_ptr.h"
#include "bn_regular_bg_tiles_ptr.h"

#include "bn_sprite_items_common_fixed_8x16_font.h"
#include "common_variable_8x16_sprite_font.h"

#include "otp_export.h"
#include "ui_raster.h"

namespace ui
{

namespace
{
    constexpr int screen_width = 240;
    constexpr int screen_height = 160;
    constexpr int tile_columns = screen_width / 8;     // 30
    constexpr int tile_rows = screen_height / 8;       // 20
    constexpr int tile_count = tile_columns * tile_rows;
    constexpr int tile_bytes = 64;                      // 8bpp

    constexpr bn::color hex(uint32_t rgb)
    {
        return bn::color(int((rgb >> 16) & 0xFF) >> 3, int((rgb >> 8) & 0xFF) >> 3, int(rgb & 0xFF) >> 3);
    }

    constexpr bn::color palette_colors[112] = {
        hex(0xf8f0d0),  // 0 backdrop (never visible: the canvas covers the screen)
        hex(0xf8f8f8), hex(0x404858), hex(0xc8d0d8), hex(0xd0e8f8), hex(0xe0ecf8), hex(0xf8f0d0), hex(0xf0e0b0),
        hex(0xd83828), hex(0xf87858), hex(0x982010), hex(0xf8c838), hex(0xc87818), hex(0x48d058), hex(0x6878a0),
        hex(0xf8d0c0),
        // 16..31: extra colors
        hex(0xa8f8a0), hex(0xf8c038), hex(0xf05030), hex(0xf8a890), hex(0xf8e8a0), hex(0xd0d8e0), hex(0x8890a0),
        hex(0x8898b8), hex(0xe8a020), hex(0x687890), hex(0xf8c8c0), hex(0xa8b8c8), hex(0x68788f), hex(0xe85840),
        hex(0xc8d0e0), hex(0x000000),
        // 32..39: badge colors
        hex(0x3070e0), hex(0x8060e8), hex(0x2898f8), hex(0xf07830), hex(0xe04038), hex(0x50a848), hex(0xd058a8),
        hex(0x38b0a0),
        // 40..43: Classic theme
        hex(0x101840), hex(0x223678), hex(0x2850aa), hex(0x6ea0ff),
        // 44..97: Pocket LCD, Phosphor (+ faded), Net Link (+ faded, faded badges)
        hex(0xe8f0d0), hex(0xa8c890), hex(0x507850), hex(0x183020), hex(0x081008), hex(0x302008), hex(0x806020),
        hex(0xa07828), hex(0xf8b030), hex(0x081008), hex(0x101008), hex(0x202010), hex(0x282810), hex(0x403010),
        hex(0xf8f8f8), hex(0x001830), hex(0x082050), hex(0x103878), hex(0x083878), hex(0x0850a0), hex(0x2878d0),
        hex(0x1878e0), hex(0x38d8f8), hex(0xc0f8f8), hex(0x78f858), hex(0x78b8e8), hex(0xf89830), hex(0xf8d838),
        hex(0xf85838), hex(0xf89878), hex(0x586878), hex(0x001830), hex(0x001838), hex(0x082048), hex(0x002048),
        hex(0x002858), hex(0x103868), hex(0x083870), hex(0x105878), hex(0x406878), hex(0x286840), hex(0x285070),
        hex(0x584830), hex(0x585830), hex(0x583030), hex(0x584848), hex(0x103870), hex(0x303070), hex(0x104878),
        hex(0x503830), hex(0x502830), hex(0x204838), hex(0x483058), hex(0x105058),
    };

    static_assert(palette_size <= 112);

    // Color index -> faded color index, for the modal backdrops.
    BN_DATA_EWRAM_BSS uint8_t fade_tables[2][256];

    void build_fade_tables()
    {
        for(int table = 0; table < 2; ++table)
        {
            for(int i = 0; i < 256; ++i)
            {
                fade_tables[table][i] = uint8_t(i);
            }
        }

        for(int i = 0; i < amber_bg_faded - amber_bg; ++i)
        {
            fade_tables[0][amber_bg + i] = uint8_t(amber_bg_faded + i);
        }

        for(int i = 0; i < net_text_faded - net_text; ++i)
        {
            fade_tables[1][net_text + i] = uint8_t(net_text_faded + i);
        }

        for(int i = 0; i < 8; ++i)
        {
            fade_tables[1][badge_0 + i] = uint8_t(badge_0_net_faded + i);
        }
    }

    constexpr bn::bg_palette_item palette_item(palette_colors, bn::bpp_mode::BPP_8);

    // Font glyph fill indices in Butano's common fonts (their built-in shadow is ignored).
    constexpr int body_fill_index = 14;
    constexpr int digits_fill_index = 1;
    constexpr int digits_advance = 7;

    uint8_t* canvas = nullptr;
    bn::tile* vram_tiles = nullptr;
    uint32_t dirty[(tile_count + 31) / 32];
    bool any_dirty = false;

    bn::optional<bn::regular_bg_tiles_ptr> bg_tiles;
    bn::optional<bn::regular_bg_map_ptr> bg_map;
    bn::optional<bn::regular_bg_ptr> bg;

    const bn::tile* body_tiles = nullptr;
    const bn::tile* digits_tiles = nullptr;

    inline void put(int x, int y, uint8_t color)
    {
        if(unsigned(x) < unsigned(screen_width) && unsigned(y) < unsigned(screen_height))
        {
            canvas[((y >> 3) * tile_columns + (x >> 3)) * tile_bytes + ((y & 7) << 3) + (x & 7)] = color;
        }
    }

    void mark_dirty(int x, int y, int width, int height)
    {
        int x0 = x < 0 ? 0 : x;
        int y0 = y < 0 ? 0 : y;
        int x1 = x + width > screen_width ? screen_width : x + width;
        int y1 = y + height > screen_height ? screen_height : y + height;

        if(x0 >= x1 || y0 >= y1)
        {
            return;
        }

        for(int ty = y0 >> 3; ty <= (y1 - 1) >> 3; ++ty)
        {
            for(int tx = x0 >> 3; tx <= (x1 - 1) >> 3; ++tx)
            {
                int index = ty * tile_columns + tx;
                dirty[index >> 5] |= 1u << (index & 31);
            }
        }

        any_dirty = true;
    }

    // Copies changed tiles to VRAM. Runs in core::update during V-Blank.
    void upload()
    {
        if(! any_dirty)
        {
            return;
        }

        // Copy each run of consecutive changed tiles with one call (canvas and VRAM use the
        // same tile order, so a run is contiguous in both).
        int index = 0;

        while(index < tile_count)
        {
            if(! (dirty[index >> 5] & (1u << (index & 31))))
            {
                ++index;
                continue;
            }

            int run_start = index;

            while(index < tile_count && (dirty[index >> 5] & (1u << (index & 31))))
            {
                ++index;
            }

            const bn::tile* source = reinterpret_cast<const bn::tile*>(canvas + run_start * tile_bytes);
            bn::memory::copy(*source, (index - run_start) * 2, vram_tiles[run_start * 2]);
        }

        for(uint32_t& word : dirty)
        {
            word = 0;
        }

        any_dirty = false;
    }

    int inset(int radius, int row)
    {
        if(radius == 1)
        {
            return 1;
        }

        // radius - round(sqrt(r^2 - (r - row - 0.5)^2)), in integer math (values doubled).
        int d = 2 * radius - 2 * row - 1;
        int value = 4 * radius * radius - d * d;
        int root = 0;

        while((root + 1) * (root + 1) <= value)
        {
            ++root;
        }

        return radius - (root + 1) / 2;
    }

    int glyph_index(char c)
    {
        auto u = uint8_t(c);
        return u >= 32 && u <= 126 ? u - 32 : '?' - 32;
    }

    // Glyphs come from Butano's 8x16 fonts (bold lowercase with descenders). Each glyph is two
    // tiles (top and bottom half). Caps sit on cell rows 5-11 and descenders reach row 13, so we
    // draw rows 5-13: caps become the top 7 px of the 8 px line, descenders hang 2 px below it.
    constexpr int glyph_first_row = 5;
    constexpr int glyph_rows = 9;

    constexpr int glyph_count = 95;    // ASCII 32..126

    // Per glyph and row: bit n set = column n is filled. Built once from the font tiles, so
    // drawing text doesn't decode 4bpp pixels every time.
    uint8_t glyph_masks[2][glyph_count][glyph_rows];

    void build_glyph_masks(int font_index, const bn::tile* tiles, int fill_index, int column_offset)
    {
        for(int glyph = 0; glyph < glyph_count; ++glyph)
        {
            for(int row = 0; row < glyph_rows; ++row)
            {
                uint8_t mask = 0;

                // Butano font images have no glyph for the space: their first glyph is '!'.
                if(glyph > 0)
                {
                    const bn::tile* glyph_tiles = tiles + (glyph - 1) * 2;
                    int cell_row = glyph_first_row + row;
                    uint32_t bits = glyph_tiles[cell_row / 8].data[cell_row % 8];

                    for(int column = column_offset; column < 8; ++column)
                    {
                        if(int((bits >> (column * 4)) & 0xF) == fill_index)
                        {
                            mask |= uint8_t(1 << (column - column_offset));
                        }
                    }

                    // Fixed font: a pixel in column 0 (e.g. 'M') sits one pixel left of the cell.
                    if(column_offset && int(bits & 0xF) == fill_index)
                    {
                        mask |= 0x80;
                    }
                }

                glyph_masks[font_index][glyph][row] = mask;
            }
        }
    }

    // "code12": the 7x6 digit glyphs stretched to 12x10 by repeating some rows / columns twice
    // (bars and outer stems stay 2 px, the thin rows next to them 1 px), so the pixel style holds.
    constexpr int code12_rows = 12;
    constexpr int code12_advance = 12;     // 10 px glyph + 2 px gap
    constexpr uint8_t code12_row_repeat[7] = { 2, 1, 2, 2, 2, 1, 2 };
    constexpr uint8_t code12_column_repeat[6] = { 2, 1, 2, 2, 1, 2 };

    uint16_t code12_masks[10][code12_rows];

    void build_code12_masks()
    {
        for(int digit = 0; digit < 10; ++digit)
        {
            const uint8_t* source = glyph_masks[1]['0' + digit - 32];
            int out_row = 0;

            for(int row = 0; row < 7; ++row)
            {
                uint16_t wide = 0;
                int out_column = 0;

                for(int column = 0; column < 6; ++column)
                {
                    for(int i = 0; i < code12_column_repeat[column]; ++i)
                    {
                        if(source[row] & (1 << column))
                        {
                            wide |= uint16_t(1 << out_column);
                        }

                        ++out_column;
                    }
                }

                for(int i = 0; i < code12_row_repeat[row]; ++i)
                {
                    code12_masks[digit][out_row++] = wide;
                }
            }
        }
    }

    // Derived sizes, built at startup from the 9 drawn rows / 8 cell columns of a base font by
    // repeating each row / column 0, 1 or 2 times (so 2 px strokes stay as even as possible).
    struct derived_font_data
    {
        uint16_t masks[glyph_count][16];    // bit n = output column n
        int8_t advance[glyph_count];
        int rows;
        int x_offset;                       // output columns left of the glyph's x
        int last_gap;                       // blank columns after the last glyph
    };

    BN_DATA_EWRAM_BSS derived_font_data derived_fonts[3];     // body6, digits6, title12

    derived_font_data* derived(font text_font)
    {
        switch(text_font)
        {

        case font::body6:
            return &derived_fonts[0];

        case font::digits6:
            return &derived_fonts[1];

        case font::title12:
            return &derived_fonts[2];

        default:
            return nullptr;
        }
    }

    void build_derived_font(derived_font_data& out, bool digits_base, const uint8_t (&row_repeat)[glyph_rows],
                            const uint8_t (&column_repeat)[8], int last_gap)
    {
        out.rows = 0;

        for(uint8_t repeat : row_repeat)
        {
            out.rows += repeat;
        }

        // Fixed font: cell column 1 sits at x, column 0 (bit 7 in its masks) one step left of it.
        out.x_offset = digits_base ? column_repeat[0] : 0;
        out.last_gap = last_gap;

        for(int glyph = 0; glyph < glyph_count; ++glyph)
        {
            const uint8_t* source = glyph_masks[digits_base ? 1 : 0][glyph];
            int advance = 0;

            if(digits_base)
            {
                for(int column = 1; column < 8; ++column)
                {
                    advance += column_repeat[column];
                }
            }
            else
            {
                int width = common::variable_8x16_sprite_font_character_widths[glyph];

                for(int column = 0; column < width && column < 8; ++column)
                {
                    advance += column_repeat[column];
                }
            }

            out.advance[glyph] = int8_t(advance);

            int out_row = 0;

            for(int row = 0; row < glyph_rows; ++row)
            {
                uint16_t mask = 0;
                int out_column = 0;

                for(int column = 0; column < 8; ++column)
                {
                    bool on = digits_base ? (column == 0 ? (source[row] >> 7) & 1 : (source[row] >> (column - 1)) & 1) :
                                            (source[row] >> column) & 1;

                    for(int i = 0; i < column_repeat[column]; ++i)
                    {
                        if(on)
                        {
                            mask |= uint16_t(1 << out_column);
                        }

                        ++out_column;
                    }
                }

                for(int i = 0; i < row_repeat[row]; ++i)
                {
                    out.masks[glyph][out_row++] = mask;
                }
            }
        }
    }

    void build_derived_fonts()
    {
        // 6 px: drop the 2nd row (a stem row in caps and digits; lowercase starts below it).
        constexpr uint8_t six_rows[glyph_rows] = { 1, 0, 1, 1, 1, 1, 1, 1, 1 };
        constexpr uint8_t same_columns[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };

        // 12 px title: 2x, except the 2nd and 6th rows and some columns stay single.
        constexpr uint8_t twelve_rows[glyph_rows] = { 2, 1, 2, 2, 2, 1, 2, 2, 2 };
        constexpr uint8_t twelve_columns[8] = { 2, 1, 2, 2, 1, 2, 2, 1 };

        build_derived_font(derived_fonts[0], false, six_rows, same_columns, 1);
        build_derived_font(derived_fonts[1], true, six_rows, same_columns, 1);
        build_derived_font(derived_fonts[2], false, twelve_rows, twelve_columns, 2);
    }

    int advance(font text_font, char c, int scale)
    {
        if(derived_font_data* data = derived(text_font))
        {
            return data->advance[glyph_index(c)];
        }

        if(text_font == font::code12)
        {
            return code12_advance;
        }

        if(text_font == font::digits)
        {
            return digits_advance * scale;
        }

        return common::variable_8x16_sprite_font_character_widths[glyph_index(c)] * scale;
    }

    void draw_glyph(font text_font, char c, int x, int y, uint8_t color, int scale)
    {
        int index = glyph_index(c);

        if(index == 0)
        {
            return;
        }

        if(text_font == font::code12)
        {
            if(c >= '0' && c <= '9')
            {
                raster::glyph16(canvas, code12_masks[c - '0'], code12_rows, x, y, color);
            }

            return;
        }

        if(derived_font_data* data = derived(text_font))
        {
            raster::glyph16(canvas, data->masks[index], data->rows, x - data->x_offset, y, color);
            return;
        }

        bool digits = text_font == font::digits;
        const uint8_t* masks = glyph_masks[digits ? 1 : 0][index];

        if(digits && (masks[0] | masks[1] | masks[2] | masks[3] | masks[4] | masks[5] | masks[6] | masks[7] |
                      masks[8]) & 0x80)
        {
            // Column 0 pixel of the fixed font: drawn one pixel to the left (see build_glyph_masks).
            uint8_t left[glyph_rows];
            uint8_t rest[glyph_rows];

            for(int row = 0; row < glyph_rows; ++row)
            {
                left[row] = masks[row] & 0x80 ? 1 : 0;
                rest[row] = masks[row] & 0x7F;
            }

            raster::glyph(canvas, left, glyph_rows, x - scale, y, scale, color);
            raster::glyph(canvas, rest, glyph_rows, x, y, scale, color);
            return;
        }

        raster::glyph(canvas, masks, glyph_rows, x, y, scale, color);
    }
}

void init()
{
    canvas = static_cast<uint8_t*>(bn::memory::ewram_alloc(tile_count * tile_bytes));
    BN_ASSERT(canvas, "Not enough EWRAM for the UI canvas");

    body_tiles = bn::sprite_items::common_variable_8x16_font.tiles_item().tiles_ref().data();
    digits_tiles = bn::sprite_items::common_fixed_8x16_font.tiles_item().tiles_ref().data();
    build_glyph_masks(0, body_tiles, body_fill_index, 0);
    build_glyph_masks(1, digits_tiles, digits_fill_index, 1);   // fixed font glyphs start one column in
    build_code12_masks();
    build_derived_fonts();
    build_fade_tables();

    // 8bpp tiles are counted in 4bpp units by Butano. No offset: the map indexes them directly.
    bg_tiles = bn::regular_bg_tiles_ptr::allocate(tile_count * 2, bn::bpp_mode::BPP_8, false);
    bn::bg_palette_ptr palette = bn::bg_palette_ptr::create(palette_item);
    bg_map = bn::regular_bg_map_ptr::allocate(bn::size(32, 32), *bg_tiles, palette);

    vram_tiles = bg_tiles->vram()->data();

    bn::span<bn::regular_bg_map_cell> cells = *bg_map->vram();

    for(int y = 0; y < 32; ++y)
    {
        for(int x = 0; x < 32; ++x)
        {
            cells[y * 32 + x] = bn::regular_bg_map_cell(x < tile_columns && y < tile_rows ? y * tile_columns + x : 0);
        }
    }

    // A 256x256 map is centered on the screen by default: shift it so map (0,0) is screen (0,0).
    bg = bn::regular_bg_ptr::create(8, 48, *bg_map);

    background();
    bn::core::set_vblank_callback(upload);
}

void fill(int x, int y, int width, int height, uint8_t color)
{
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + width > screen_width ? screen_width : x + width;
    int y1 = y + height > screen_height ? screen_height : y + height;

    if(x0 >= x1 || y0 >= y1)
    {
        return;
    }

    raster::fill(canvas, x0, y0, x1, y1, color);
    mark_dirty(x0, y0, x1 - x0, y1 - y0);
}

void round_fill(int x, int y, int width, int height, int radius, uint8_t color)
{
    int max_radius = (width < height ? width : height) / 2;

    if(radius > max_radius)
    {
        radius = max_radius;
    }

    if(radius <= 0)
    {
        fill(x, y, width, height, color);
        return;
    }

    for(int row = 0; row < radius; ++row)
    {
        int row_inset = inset(radius, row);
        fill(x + row_inset, y + row, width - 2 * row_inset, 1, color);
        fill(x + row_inset, y + height - 1 - row, width - 2 * row_inset, 1, color);
    }

    fill(x, y + radius, width, height - 2 * radius, color);
}

void stripes(int x, int y, int width, int height)
{
    for(int row = y; row < y + height; ++row)
    {
        fill(x, row, width, 1, (row >> 1) & 1 ? stripe_b : stripe_a);
    }
}

void dotted_hline(int x, int y, int width, uint8_t color)
{
    for(int i = 0; i < width; i += 2)
    {
        put(x + i, y, color);
    }

    mark_dirty(x, y, width, 1);
}

int text_width(font text_font, const bn::string_view& text, int scale)
{
    int width = 0;

    for(char c : text)
    {
        width += advance(text_font, c, scale);
    }

    int last_gap = scale;

    if(text_font == font::code12)
    {
        last_gap = code12_advance - 10;
    }
    else if(derived_font_data* data = derived(text_font))
    {
        last_gap = data->last_gap;
    }

    return width > 0 ? width - last_gap : 0;     // no gap after the last glyph
}

void text(font text_font, const bn::string_view& text, int x, int y, uint8_t color, int shadow, int scale,
          align alignment, int shadow_offset)
{
    int width = text_width(text_font, text, scale);

    if(alignment == align::center)
    {
        x -= width / 2;
    }
    else if(alignment == align::right)
    {
        x -= width;
    }

    // Shadows first so they never cover the previous glyph's fill.
    if(shadow >= 0)
    {
        int cx = x;

        for(char c : text)
        {
            draw_glyph(text_font, c, cx + shadow_offset, y + shadow_offset, uint8_t(shadow), scale);
            cx += advance(text_font, c, scale);
        }
    }

    int cx = x;

    for(char c : text)
    {
        draw_glyph(text_font, c, cx, y, color, scale);
        cx += advance(text_font, c, scale);
    }

    mark_dirty(x - scale, y - scale, width + 2 * scale + shadow_offset + 1, 10 * scale + shadow_offset + 1);
}

bn::string_view fit_text(font text_font, const bn::string_view& text, int max_width, int scale, bool& truncated)
{
    truncated = false;

    if(text_width(text_font, text, scale) <= max_width)
    {
        return text;
    }

    truncated = true;
    int dots_width = text_width(text_font, "...", scale) + scale;
    int size = text.size();

    while(size > 0 && text_width(text_font, text.substr(0, size), scale) + dots_width > max_width)
    {
        --size;
    }

    return text.substr(0, size);
}

void background()
{
    stripes(0, 0, screen_width, screen_height);
}

void header(const bn::string_view& title)
{
    fill(0, 0, screen_width, 22, red);
    fill(0, 1, screen_width, 1, header_light);
    fill(0, 22, screen_width, 2, header_shadow);
    text(font::title12, title, 6, 4, white, header_shadow);
}

void header_band_clear(int x, int width)
{
    fill(x, 2, width, 20, red);
}

void footer_bar(int top)
{
    fill(0, top, screen_width, screen_height - top, ink);
    fill(0, top, screen_width, 1, footer_light);
}

int pill_width(const char* label, font label_font)
{
    return text_width(label_font, label) + 8;
}

int pill(int x, int y, const char* label, uint8_t fill_color, uint8_t text_color, font label_font)
{
    int width = pill_width(label, label_font);
    round_fill(x, y, width, 10, 4, fill_color);
    text(label_font, label, x + 4, y + 2, text_color);
    return width;
}

void round_button(int x, int y, const char* letter, uint8_t fill_color, uint8_t letter_color, font letter_font)
{
    round_fill(x, y, 10, 10, 5, fill_color);
    text(letter_font, letter, x + 5, y + 2, letter_color, no_shadow, 1, align::center);
}

namespace
{
    bool is_round_button(const char* button)
    {
        return (button[0] == 'A' || button[0] == 'B') && button[1] == 0;
    }

    int hint_width(const hint& item)
    {
        int button_width = is_round_button(item.button) ? 10 : pill_width(item.button, font::digits6);
        return button_width + 3 + text_width(font::body6, item.label);
    }
}

void footer_hints(int text_y, const hint* hints, int count)
{
    constexpr int gap = 12;
    int total = 0;

    for(int i = 0; i < count; ++i)
    {
        total += hint_width(hints[i]) + (i ? gap : 0);
    }

    int x = (screen_width - total) / 2;

    for(int i = 0; i < count; ++i)
    {
        const hint& item = hints[i];
        int button_width;

        if(is_round_button(item.button))
        {
            round_button(x, text_y - 1, item.button, white, ink, font::digits6);
            button_width = 10;
        }
        else
        {
            button_width = pill(x, text_y - 1, item.button, white, ink, font::digits6);
        }

        text(font::body6, item.label, x + button_width + 3, text_y, white);
        x += hint_width(item) + gap;
    }
}

void footer_message(int text_y, const bn::string_view& message)
{
    text(font::body6, message, screen_width / 2, text_y, selection, no_shadow, 1, align::center);
}

void panel(int x, int y, int width, int height, uint8_t fill_color, bool modal)
{
    round_fill(x, y, width, height, 4, ink);

    if(modal)
    {
        round_fill(x + 2, y + 2, width - 4, height - 4, 2, bevel);
        round_fill(x + 3, y + 3, width - 6, height - 6, 1, fill_color);
    }
    else
    {
        round_fill(x + 2, y + 2, width - 4, height - 4, 2, fill_color);
    }
}

void selection_bar(int x, int y, int width)
{
    round_fill(x, y, width, 15, 2, selection);
    fill(x + 1, y + 14, width - 2, 1, selection_edge);
}

void menu_item(int box_x, int box_width, int item_y, const bn::string_view& label, uint8_t text_color,
               int shadow, bool selected, int bob)
{
    fill(box_x + 4, item_y - 4, box_width - 8, 15, white);

    if(selected)
    {
        selection_bar(box_x + 4, item_y - 4, box_width - 8);
        cursor_right(box_x + 8 + bob, item_y, red);
    }

    text(font::body, label, box_x + 17, item_y, text_color, shadow);
}

int cursor_bob(int frame_counter)
{
    return (frame_counter / 16) & 1;
}

void cursor_right(int x, int y, uint8_t color)
{
    for(int row = 0; row < 8; ++row)
    {
        int length = (row < 7 - row ? row : 7 - row) + 2;
        fill(x, y + row, length, 1, color);
    }
}

void cursor_down(int x, int y, uint8_t color)
{
    fill(x, y, 5, 1, color);
    fill(x + 1, y + 1, 3, 1, color);
    fill(x + 2, y + 2, 1, 1, color);
}

void cursor_up(int x, int y, uint8_t color)
{
    fill(x + 2, y, 1, 1, color);
    fill(x + 1, y + 1, 3, 1, color);
    fill(x, y + 2, 5, 1, color);
}

void key_cap(int x, int y, int width, int height, char label, bool selected, uint8_t panel_color)
{
    fill(x - 2, y - 2, width + 4, height + 4, panel_color);

    if(selected)
    {
        round_fill(x - 2, y - 2, width + 4, height + 4, 3, red);
        round_fill(x - 1, y - 1, width + 2, height + 2, 2, panel_color);
    }

    round_fill(x, y, width, height, 2, selected ? selection : white);
    fill(x + 1, y + height - 1, width - 2, 1, selected ? selection_edge : key_shade);

    char label_text[2] = { label, 0 };
    text(font::digits, label_text, x + width / 2, y + (height - 8) / 2, ink, no_shadow, 1, align::center);
}

void busy_box(const bn::string_view& title, const bn::string_view& subtitle, int y)
{
    constexpr int x = 24, width = 192, height = 62;
    panel(x, y, width, height, white, true);
    text(font::body, title, screen_width / 2, y + 7, ink, light_shadow, 2, align::center);

    // Indeterminate bar, drawn once with the block in the middle.
    int bar_x = x + 16;
    int bar_width = width - 32;
    round_fill(bar_x, y + 28, bar_width, 8, 4, ink);

    int block_width = bar_width * 3 / 10;
    int block_x = bar_x + (bar_width - block_width) / 2;
    round_fill(block_x, y + 29, block_width, 6, 3, selection);
    fill(block_x + 2, y + 29, block_width - 4, 2, selection_shine);

    text(font::body, subtitle, screen_width / 2, y + 44, secondary, no_shadow, 1, align::center);
}

uint8_t badge_color(const char* name)
{
    uint32_t hash = 2166136261u;

    for(const char* it = name; *it; ++it)
    {
        hash ^= uint8_t(*it);
        hash *= 16777619u;
    }

    return uint8_t(badge_0 + hash % 8);
}

int qr_code(const uint8_t* qrcode, int x, int area_top, int area_height, int max_module)
{
    constexpr int quiet_zone = 4;     // modules of white border required around a QR Code
    int size = otp_export::qr_size(qrcode);
    int modules = size + 2 * quiet_zone;
    int pixels = area_height / modules;

    if(pixels > max_module)
    {
        pixels = max_module;
    }

    int side = modules * pixels;
    int y = area_top + (area_height - side) / 2;
    fill(x, y, side, side, white);

    for(int module_y = 0; module_y < size; ++module_y)
    {
        for(int module_x = 0; module_x < size; ++module_x)
        {
            if(otp_export::qr_module(qrcode, module_x, module_y))
            {
                fill(x + (quiet_zone + module_x) * pixels, y + (quiet_zone + module_y) * pixels, pixels, pixels,
                     qr_dark);
            }
        }
    }

    return side;
}

namespace
{
    // Sprite font palette indices: body (variable) fill 14 / outline 12, mono (fixed) fill 1 / outline 2.
    constexpr int classic_small_fill = 14, classic_small_shade = 12;
    constexpr int classic_mono_fill = 1, classic_mono_shade = 2;

    int classic_advance(classic_font text_font, char c)
    {
        return text_font == classic_font::mono ? 8 : common::variable_8x16_sprite_font_character_widths[glyph_index(c)];
    }
}

int classic_text_width(classic_font text_font, const bn::string_view& text)
{
    int width = 0;

    for(char c : text)
    {
        width += classic_advance(text_font, c);
    }

    return width;
}

void classic_text(classic_font text_font, const bn::string_view& text, int x, int y, align alignment)
{
    int width = classic_text_width(text_font, text);
    int left = screen_width / 2 + x;
    int top = screen_height / 2 + y - 8;

    if(alignment == align::center)
    {
        left -= width / 2;
    }
    else if(alignment == align::right)
    {
        left -= width;
    }

    bool mono = text_font == classic_font::mono;
    const bn::tile* tiles = mono ? digits_tiles : body_tiles;
    int cx = left;

    for(char c : text)
    {
        int index = glyph_index(c);

        // Butano font images have no glyph for the space: their first glyph is '!'.
        if(index > 0)
        {
            const bn::tile* cell = tiles + (index - 1) * 2;
            raster::glyph_cell(canvas, cell[0].data, cx, top, mono ? classic_mono_fill : classic_small_fill, white,
                               mono ? classic_mono_shade : classic_small_shade, black);
        }

        cx += classic_advance(text_font, c);
    }

    mark_dirty(left, top, width + 8, 16);
}

void pattern_hline(int x, int y, int width, int on, int period, uint8_t color)
{
    for(int i = 0; i < width; i += period)
    {
        fill(x + i, y, on < width - i ? on : width - i, 1, color);
    }
}

void pattern_vline(int x, int y, int height, int on, int period, uint8_t color)
{
    for(int i = 0; i < height; i += period)
    {
        fill(x, y + i, 1, on < height - i ? on : height - i, color);
    }
}

namespace
{
    bool clip(int& x, int& y, int& width, int& height)
    {
        int x1 = x + width > screen_width ? screen_width : x + width;
        int y1 = y + height > screen_height ? screen_height : y + height;
        x = x < 0 ? 0 : x;
        y = y < 0 ? 0 : y;
        width = x1 - x;
        height = y1 - y;
        return width > 0 && height > 0;
    }
}

void fade(int x, int y, int width, int height, fade_style style)
{
    if(clip(x, y, width, height))
    {
        raster::remap(canvas, x, y, x + width, y + height, fade_tables[style == fade_style::phosphor ? 0 : 1]);
        mark_dirty(x, y, width, height);
    }
}

void checker(int x, int y, int width, int height, uint8_t color)
{
    if(clip(x, y, width, height))
    {
        raster::checker(canvas, x, y, x + width, y + height, color);
        mark_dirty(x, y, width, height);
    }
}


namespace
{
    // "Lock Boy" logo (handoff authboy_logo_handoff/*/logo/logo_lockboy.h): 4bpp tiles, pixel x in
    // bits [4x, 4x + 3]. Colors: 1 outline, 2 shackle, 3 body, 4 body shade, 5 screen, 6 A button.
    // 16x16: frame 0 closed, frame 1 open (2x2 tiles each, row-major); 32x32: the same two frames,
    // pre-scaled (4x4 tiles each).
    constexpr uint32_t logo_tiles_16[8][8] = {
        { 0x00000000, 0x11100000, 0x22210000, 0x11121000, 0x00121000, 0x00121000, 0x11111110, 0x33333310 },
        { 0x00000000, 0x00000111, 0x00001222, 0x00012111, 0x00012100, 0x00012100, 0x01111111, 0x01333333 },
        { 0x11133310, 0x55131310, 0x15111110, 0x15131310, 0x11133310, 0x33333310, 0x44444410, 0x11111100 },
        { 0x01333111, 0x01333155, 0x01363151, 0x01336151, 0x01333111, 0x01333333, 0x01444444, 0x00111111 },
        { 0x11100000, 0x22210000, 0x11121000, 0x00121000, 0x00121000, 0x00121000, 0x11111110, 0x33333310 },
        { 0x00000111, 0x00001222, 0x00012111, 0x00012100, 0x00011100, 0x00000000, 0x01111111, 0x01333333 },
        { 0x11133310, 0x55131310, 0x55111110, 0x55131310, 0x11133310, 0x33333310, 0x44444410, 0x11111100 },
        { 0x01333111, 0x01333155, 0x01363155, 0x01336155, 0x01333111, 0x01333333, 0x01444444, 0x00111111 },
    };

    constexpr uint32_t logo_tiles_32[32][8] = {
        { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x11000000, 0x11000000 },
        { 0x00000000, 0x00000000, 0x11111100, 0x11111100, 0x22222211, 0x22222211, 0x11111122, 0x11111122 },
        { 0x00000000, 0x00000000, 0x00111111, 0x00111111, 0x11222222, 0x11222222, 0x22111111, 0x22111111 },
        { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000011, 0x00000011 },
        { 0x11000000, 0x11000000, 0x11000000, 0x11000000, 0x11111100, 0x11111100, 0x33331100, 0x33331100 },
        { 0x00001122, 0x00001122, 0x00001122, 0x00001122, 0x11111111, 0x11111111, 0x33333333, 0x33333333 },
        { 0x22110000, 0x22110000, 0x22110000, 0x22110000, 0x11111111, 0x11111111, 0x33333333, 0x33333333 },
        { 0x00000011, 0x00000011, 0x00000011, 0x00000011, 0x00111111, 0x00111111, 0x00113333, 0x00113333 },
        { 0x33331100, 0x33331100, 0x11331100, 0x11331100, 0x11111100, 0x11111100, 0x11331100, 0x11331100 },
        { 0x11111133, 0x11111133, 0x55551133, 0x55551133, 0x11551111, 0x11551111, 0x11551133, 0x11551133 },
        { 0x33111111, 0x33111111, 0x33115555, 0x33115555, 0x33115511, 0x33115511, 0x66115511, 0x66115511 },
        { 0x00113333, 0x00113333, 0x00113333, 0x00113333, 0x00113366, 0x00113366, 0x00113333, 0x00113333 },
        { 0x33331100, 0x33331100, 0x33331100, 0x33331100, 0x44441100, 0x44441100, 0x11110000, 0x11110000 },
        { 0x11111133, 0x11111133, 0x33333333, 0x33333333, 0x44444444, 0x44444444, 0x11111111, 0x11111111 },
        { 0x33111111, 0x33111111, 0x33333333, 0x33333333, 0x44444444, 0x44444444, 0x11111111, 0x11111111 },
        { 0x00113333, 0x00113333, 0x00113333, 0x00113333, 0x00114444, 0x00114444, 0x00001111, 0x00001111 },
        { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x11000000, 0x11000000, 0x11000000, 0x11000000 },
        { 0x11111100, 0x11111100, 0x22222211, 0x22222211, 0x11111122, 0x11111122, 0x00001122, 0x00001122 },
        { 0x00111111, 0x00111111, 0x11222222, 0x11222222, 0x22111111, 0x22111111, 0x22110000, 0x22110000 },
        { 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000011, 0x00000011, 0x00000011, 0x00000011 },
        { 0x11000000, 0x11000000, 0x11000000, 0x11000000, 0x11111100, 0x11111100, 0x33331100, 0x33331100 },
        { 0x00001122, 0x00001122, 0x00001122, 0x00001122, 0x11111111, 0x11111111, 0x33333333, 0x33333333 },
        { 0x11110000, 0x11110000, 0x00000000, 0x00000000, 0x11111111, 0x11111111, 0x33333333, 0x33333333 },
        { 0x00000011, 0x00000011, 0x00000000, 0x00000000, 0x00111111, 0x00111111, 0x00113333, 0x00113333 },
        { 0x33331100, 0x33331100, 0x11331100, 0x11331100, 0x11111100, 0x11111100, 0x11331100, 0x11331100 },
        { 0x11111133, 0x11111133, 0x55551133, 0x55551133, 0x55551111, 0x55551111, 0x55551133, 0x55551133 },
        { 0x33111111, 0x33111111, 0x33115555, 0x33115555, 0x33115555, 0x33115555, 0x66115555, 0x66115555 },
        { 0x00113333, 0x00113333, 0x00113333, 0x00113333, 0x00113366, 0x00113366, 0x00113333, 0x00113333 },
        { 0x33331100, 0x33331100, 0x33331100, 0x33331100, 0x44441100, 0x44441100, 0x11110000, 0x11110000 },
        { 0x11111133, 0x11111133, 0x33333333, 0x33333333, 0x44444444, 0x44444444, 0x11111111, 0x11111111 },
        { 0x33111111, 0x33111111, 0x33333333, 0x33333333, 0x44444444, 0x44444444, 0x11111111, 0x11111111 },
        { 0x00113333, 0x00113333, 0x00113333, 0x00113333, 0x00114444, 0x00114444, 0x00001111, 0x00001111 },
    };
}

void logo(logo_frame frame, int x, int y, const uint8_t* colors)
{
    bool big = frame == logo_frame::big || frame == logo_frame::big_open;
    bool open = frame == logo_frame::open || frame == logo_frame::big_open;
    int tiles_per_row = big ? 4 : 2;
    const uint32_t (*tiles)[8] = big ? logo_tiles_32 + (open ? 16 : 0) : logo_tiles_16 + (open ? 4 : 0);

    for(int tile = 0; tile < tiles_per_row * tiles_per_row; ++tile)
    {
        int tile_x = x + (tile % tiles_per_row) * 8;
        int tile_y = y + (tile / tiles_per_row) * 8;

        for(int row = 0; row < 8; ++row)
        {
            uint32_t bits = tiles[tile][row];

            for(int column = 0; column < 8; ++column, bits >>= 4)
            {
                if(int value = int(bits & 0xF); value && value < 7)
                {
                    put(tile_x + column, tile_y + row, colors[value]);
                }
            }
        }
    }

    mark_dirty(x, y, tiles_per_row * 8, tiles_per_row * 8);
}

}
