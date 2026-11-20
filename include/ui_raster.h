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

    // Draws one glyph from its row bitmasks (bit n = column n), clipped to the screen.
    BN_CODE_IWRAM void glyph(uint8_t* canvas, const uint8_t* row_masks, int rows, int x, int y, int scale,
                             uint8_t color);
}

#endif
