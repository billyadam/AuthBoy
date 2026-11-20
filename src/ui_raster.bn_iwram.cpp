#include "ui_raster.h"

namespace ui::raster
{

BN_CODE_IWRAM void fill(uint8_t* canvas, int x0, int y0, int x1, int y1, uint8_t color)
{
    uint32_t word = color * 0x01010101u;

    for(int y = y0; y < y1; ++y)
    {
        uint8_t* row = canvas + (y >> 3) * tile_row_bytes + ((y & 7) << 3);
        int x = x0;

        while(x < x1)
        {
            int start = x & 7;
            int end = start + (x1 - x);

            if(end > 8)
            {
                end = 8;
            }

            uint8_t* pixels = row + (x >> 3) * tile_bytes;     // 8 bytes, 4-byte aligned

            if(start == 0 && end == 8)
            {
                reinterpret_cast<uint32_t*>(pixels)[0] = word;
                reinterpret_cast<uint32_t*>(pixels)[1] = word;
            }
            else
            {
                int i = start;

                for(; i < end && (i & 3); ++i)
                {
                    pixels[i] = color;
                }

                for(; i + 4 <= end; i += 4)
                {
                    *reinterpret_cast<uint32_t*>(pixels + i) = word;
                }

                for(; i < end; ++i)
                {
                    pixels[i] = color;
                }
            }

            x += end - start;
        }
    }
}

BN_CODE_IWRAM void glyph(uint8_t* canvas, const uint8_t* row_masks, int rows, int x, int y, int scale,
                         uint8_t color)
{
    for(int row = 0; row < rows; ++row)
    {
        unsigned mask = row_masks[row];

        if(! mask)
        {
            continue;
        }

        int py0 = y + row * scale;

        for(int column = 0; mask; ++column, mask >>= 1)
        {
            if(! (mask & 1))
            {
                continue;
            }

            int px0 = x + column * scale;

            for(int sy = 0; sy < scale; ++sy)
            {
                int py = py0 + sy;

                if(unsigned(py) >= unsigned(screen_height))
                {
                    continue;
                }

                uint8_t* row_base = canvas + (py >> 3) * tile_row_bytes + ((py & 7) << 3);

                for(int sx = 0; sx < scale; ++sx)
                {
                    int px = px0 + sx;

                    if(unsigned(px) < unsigned(screen_width))
                    {
                        row_base[(px >> 3) * tile_bytes + (px & 7)] = color;
                    }
                }
            }
        }
    }
}

}
