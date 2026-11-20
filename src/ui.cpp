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

#include "bn_sprite_items_common_fixed_8x8_font.h"
#include "common_variable_8x8_sprite_font.h"

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

    constexpr bn::color palette_colors[64] = {
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
    };

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

        for(int word = 0; word < int(sizeof(dirty) / sizeof(dirty[0])); ++word)
        {
            uint32_t bits = dirty[word];

            while(bits)
            {
                int bit = __builtin_ctz(bits);
                bits &= bits - 1;

                int index = (word << 5) + bit;
                const bn::tile* source = reinterpret_cast<const bn::tile*>(canvas + index * tile_bytes);
                bn::memory::copy(*source, 2, vram_tiles[index * 2]);
            }

            dirty[word] = 0;
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

    // Butano font images have no glyph for the space: their first tile is '!'.
    // Returns nullptr for characters with nothing to draw.
    const bn::tile* glyph_tile(font text_font, char c)
    {
        int index = glyph_index(c);

        if(index == 0)
        {
            return nullptr;
        }

        return &(text_font == font::digits ? digits_tiles : body_tiles)[index - 1];
    }

    int advance(font text_font, char c, int scale)
    {
        if(text_font == font::digits)
        {
            return digits_advance * scale;
        }

        return common::variable_8x8_sprite_font_character_widths[glyph_index(c)] * scale;
    }

    void draw_glyph(font text_font, char c, int x, int y, uint8_t color, int scale)
    {
        const bn::tile* glyph_ptr = glyph_tile(text_font, c);

        if(! glyph_ptr)
        {
            return;
        }

        const bn::tile& glyph = *glyph_ptr;
        bool digits = text_font == font::digits;
        int fill_index = digits ? digits_fill_index : body_fill_index;
        int column_offset = digits ? 1 : 0;    // fixed font glyphs start one column in

        for(int row = 0; row < 8; ++row)
        {
            uint32_t bits = glyph.data[row];

            if(! bits)
            {
                continue;
            }

            for(int column = 0; column < 8; ++column)
            {
                if(int((bits >> (column * 4)) & 0xF) == fill_index)
                {
                    int px = x + (column - column_offset) * scale;
                    int py = y + row * scale;

                    for(int sy = 0; sy < scale; ++sy)
                    {
                        for(int sx = 0; sx < scale; ++sx)
                        {
                            put(px + sx, py + sy, color);
                        }
                    }
                }
            }
        }
    }
}

void init()
{
    canvas = static_cast<uint8_t*>(bn::memory::ewram_alloc(tile_count * tile_bytes));
    BN_ASSERT(canvas, "Not enough EWRAM for the UI canvas");

    body_tiles = bn::sprite_items::common_variable_8x8_font.tiles_item().tiles_ref().data();
    digits_tiles = bn::sprite_items::common_fixed_8x8_font.tiles_item().tiles_ref().data();

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

    for(int py = y0; py < y1; ++py)
    {
        uint8_t* row = canvas + (py >> 3) * tile_columns * tile_bytes + ((py & 7) << 3);
        int px = x0;

        while(px < x1)
        {
            int start = px & 7;
            int end = start + (x1 - px);

            if(end > 8)
            {
                end = 8;
            }

            uint8_t* pixels = row + (px >> 3) * tile_bytes;

            for(int i = start; i < end; ++i)
            {
                pixels[i] = color;
            }

            px += end - start;
        }
    }

    mark_dirty(x0, y0, x1 - x0, y1 - y0);
}

void round_fill(int x, int y, int width, int height, int radius, uint8_t color)
{
    int max_radius = (width < height ? width : height) / 2;

    if(radius > max_radius)
    {
        radius = max_radius;
    }

    for(int row = 0; row < height; ++row)
    {
        int edge_row = row < height - 1 - row ? row : height - 1 - row;
        int row_inset = radius > 0 && edge_row < radius ? inset(radius, edge_row) : 0;
        fill(x + row_inset, y + row, width - 2 * row_inset, 1, color);
    }
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

    return width > 0 ? width - scale : 0;     // no gap after the last glyph
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

    mark_dirty(x - scale, y - scale, width + 2 * scale + shadow_offset + 1, 8 * scale + shadow_offset + 1);
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
    text(font::body, title, 6, 3, white, header_shadow, 2);
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

int pill_width(const char* label)
{
    return text_width(font::digits, label) + 8;
}

int pill(int x, int y, const char* label, uint8_t fill_color, uint8_t text_color)
{
    int width = pill_width(label);
    round_fill(x, y, width, 10, 4, fill_color);
    text(font::digits, label, x + 4, y + 2, text_color);
    return width;
}

void round_button(int x, int y, const char* letter, uint8_t fill_color, uint8_t letter_color)
{
    round_fill(x, y, 10, 10, 5, fill_color);
    text(font::digits, letter, x + 5, y + 2, letter_color, no_shadow, 1, align::center);
}

namespace
{
    bool is_round_button(const char* button)
    {
        return (button[0] == 'A' || button[0] == 'B') && button[1] == 0;
    }

    int hint_width(const hint& item)
    {
        int button_width = is_round_button(item.button) ? 10 : pill_width(item.button);
        return button_width + 3 + text_width(font::body, item.label);
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
            round_button(x, text_y - 1, item.button, white, ink);
            button_width = 10;
        }
        else
        {
            button_width = pill(x, text_y - 1, item.button, white, ink);
        }

        text(font::body, item.label, x + button_width + 3, text_y, white);
        x += hint_width(item) + gap;
    }
}

void footer_message(int text_y, const bn::string_view& message)
{
    text(font::body, message, screen_width / 2, text_y, selection, no_shadow, 1, align::center);
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

void busy_box(const bn::string_view& title, const bn::string_view& subtitle)
{
    constexpr int x = 24, y = 52, width = 192, height = 62;
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

}
