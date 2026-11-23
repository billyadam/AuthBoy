#ifndef CLOCK_LAYOUT_H
#define CLOCK_LAYOUT_H

// Set clock screen geometry shared by Field Guide, Net Link, Pocket LCD and Phosphor (handoff
// authboy_handoff_delta_set_clock). Native pixels, top-left.

#include "bn_string.h"

#include "theme.h"
#include "ui.h"

namespace app::clock_layout
{
    constexpr int info_x = 4, info_y = 28, info_width = 232, info_height = 18;
    constexpr int dot_x = info_x + 5, dot_y = info_y + 6, dot_size = 7;
    constexpr int info_text_x = dot_x + dot_size + 5, info_text_y = info_y + 5;

    constexpr int panel_x = 4, panel_y = 50, panel_width = 232, panel_height = 80;
    constexpr int inner_x = panel_x + 2, inner_y = panel_y + 2;     // offsets are from here
    constexpr int cells_right = panel_x + panel_width - 2 - 6;      // 6 px from the inner right edge
    constexpr int cell_gap = 2;

    constexpr int label_x = inner_x + 6;
    constexpr int date_label_y = inner_y + 6, time_label_y = inner_y + 29, zone_label_y = inner_y + 54;
    constexpr int date_y = inner_y + 4, time_y = inner_y + 22, zone_y = inner_y + 52;
    constexpr int small_height = 12, big_height = 22;
    constexpr int readout_y = inner_y + 69;    // 2 px lower than the mockup: clears the zone cell's arrow

    constexpr int tag_right = 234, tag_y = 6;

    constexpr int paused_footer_top = 134, paused_row_1_y = 137, paused_row_2_y = 148;
    constexpr int running_footer_top = 144, running_row_y = 147;

    constexpr ui::font small_font = ui::font::digits6;
    constexpr ui::font big_font = ui::font::code12;
    constexpr ui::font separator_font = ui::font::digits;

    struct rect
    {
        int x, y, width, height;
    };

    struct layout
    {
        rect cells[clock_field_count];
        int date_separator_x[2];        // "-" left edges
        int time_separator_x[2];        // ":" left edges
        int utc_x;                      // right edge of the "UTC" before the zone cell
    };

    // Lays the rows out from the right: cell, 2 px, separator, 2 px, cell...
    inline layout make_layout()
    {
        layout result = {};
        int dash = ui::text_width(small_font, "-");
        int colon = ui::text_width(separator_font, ":");

        // The mockup's sizes, wider if our font needs it (3 px each side).
        auto cell_width = [](const char* sample, int mockup_width) {
            int width = ui::text_width(small_font, sample) + 6;
            return width > mockup_width ? width : mockup_width;
        };

        int x = cells_right;
        int two_digits = cell_width("00", 16);
        const int date_widths[3] = { cell_width("0000", 28), two_digits, two_digits };

        for(int i = 2; i >= 0; --i)
        {
            x -= date_widths[i];
            result.cells[clock_field_year + i] = { x, date_y, date_widths[i], small_height };

            if(i)
            {
                x -= cell_gap + dash;
                result.date_separator_x[i - 1] = x;
                x -= cell_gap;
            }
        }

        x = cells_right;

        for(int i = 2; i >= 0; --i)
        {
            x -= 36;
            result.cells[clock_field_hour + i] = { x, time_y, 36, big_height };

            if(i)
            {
                x -= cell_gap + colon;
                result.time_separator_x[i - 1] = x;
                x -= cell_gap;
            }
        }

        // 4 px before the zone cell: clear of the selection rings.
        int zone_width = cell_width("+00:00", 40);
        result.cells[clock_field_zone] = { cells_right - zone_width, zone_y, zone_width, small_height };
        result.utc_x = cells_right - zone_width - 4;
        return result;
    }

    inline bool big_cell(int field)
    {
        return field >= clock_field_hour && field <= clock_field_second;
    }

    // Cell text centered in the cell.
    inline void cell_text(const clock_editor_state& state, int field, const rect& cell, uint8_t color)
    {
        bn::string<8> text = clock_cell_text(state, field);

        if(big_cell(field))
        {
            ui::text(big_font, text, cell.x + cell.width / 2, cell.y + 5, color, ui::no_shadow, 1, ui::align::center);
        }
        else
        {
            ui::text(small_font, text, cell.x + cell.width / 2 + 1, cell.y + 3, color, ui::no_shadow, 1,
                     ui::align::center);
        }
    }

    // Separators ("-", ":") and "UTC".
    inline void separators(const layout& cells, uint8_t dim, uint8_t text)
    {
        for(int i = 0; i < 2; ++i)
        {
            ui::text(small_font, "-", cells.date_separator_x[i], date_y + 3, dim);
            ui::text(separator_font, ":", cells.time_separator_x[i], time_y + 7, dim);
        }

        ui::text(small_font, "UTC", cells.utc_x, zone_y + 3, text, ui::no_shadow, 1, ui::align::right);
    }

    inline void readout(const clock_editor_state& state, uint8_t dim)
    {
        ui::text(small_font, clock_utc_readout(state), cells_right, readout_y, dim, ui::no_shadow, 1,
                 ui::align::right);
    }

    // 8x4 triangles centered on the cell, 1 px clear of it (2 px below the time cells, like
    // the handoff's time row).
    inline void arrows(const rect& cell, uint8_t color)
    {
        int center = cell.x + cell.width / 2;
        int up_bottom = cell.y - 2;
        int down_top = cell.y + cell.height + (cell.height == big_height ? 2 : 1);

        for(int row = 0; row < 4; ++row)
        {
            int width = 2 + row * 2;
            ui::fill(center - width / 2, up_bottom - 3 + row, width, 1, color);
            ui::fill(center - width / 2, down_top + 3 - row, width, 1, color);
        }
    }

    inline const char* info_text(const clock_editor_state& state)
    {
        return state.running ? "Running. Compare with phone." : "Press A when phone shows this";
    }
}

#endif
