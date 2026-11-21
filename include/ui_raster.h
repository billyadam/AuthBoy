#ifndef UI_RASTER_H
#define UI_RASTER_H

// Hot pixel loops of the UI canvas, compiled as ARM code and run from IWRAM
// (src/ui_raster.bn_iwram.cpp). Used only by ui.cpp.

#include <cstdint>

#include "bn_common.h"

namespace ui::raster
{
    // Canvas layout: 30x20 tiles of 8x8 pixels, 8bpp, tile-major.
    constexpr int screen_width = 240;
    constexpr int screen_height = 160;
    constexpr int tile_columns = screen_width / 8;
    constexpr int tile_bytes = 64;
    constexpr int tile_row_bytes = tile_columns * tile_bytes;

    // Fills [x0, x1) x [y0, y1), already clipped to the screen. Uses 32-bit writes where it can.
    BN_CODE_IWRAM void fill(uint8_t* canvas, int x0, int y0, int x1, int y1, uint8_t color);

    // Draws one unscaled glyph up to 16 px wide from 16-bit row bitmasks, clipped to the screen.
    BN_CODE_IWRAM void glyph16(uint8_t* canvas, const uint16_t* row_masks, int rows, int x, int y, uint8_t color);

    // Draws one glyph from its row bitmasks (bit n = column n), clipped to the screen.
    BN_CODE_IWRAM void glyph(uint8_t* canvas, const uint8_t* row_masks, int rows, int x, int y, int scale,
                             uint8_t color);

    // Draws a whole 8x16 4bpp font cell (16 rows of 8 nibbles): pixels equal to fill_index get
    // fill_color, pixels equal to shade_index get shade_color, the rest is left as is.
    BN_CODE_IWRAM void glyph_cell(uint8_t* canvas, const uint32_t* cell_rows, int x, int y, int fill_index,
                                  uint8_t fill_color, int shade_index, uint8_t shade_color);
}

#endif
