// Classic theme: on-screen keyboard, as in the original release (with today's layouts).
// Coordinates are the original sprite ones: (0, 0) is the screen center (see ui::classic_text).

#include "theme.h"

#include "bn_string.h"

#include "app_text.h"
#include "ui.h"

namespace app::classic
{

namespace
{
    using ui::classic_font;

    constexpr int cell_width = 16;      // one 8 px character + one 8 px gap in the mono font
    constexpr int first_row_y = -26;
    constexpr int row_spacing = 18;
    constexpr int title_y = -68;
    constexpr int input_y = -48;
    constexpr int footer_row_1_y = 52;
    constexpr int footer_row_2_y = 70;
    constexpr int visible_input_chars = 26;

    // Screen bands (y ranges) repainted on their own.
    constexpr int text_band_bottom = 42;        // title and typed text
    constexpr int grid_band_top = 44, grid_band_bottom = 118;
    constexpr int footer_band_top = 122;

    void clear(int top, int bottom)
    {
        ui::fill(0, top, 240, bottom - top, ui::classic_backdrop);
    }

    int grid_left(const keyboard_state& state)
    {
        int grid_width = state.columns() * cell_width - (cell_width / 2);
        return -grid_width / 2;
    }

    class keyboard_painter_impl final : public keyboard_painter
    {

    public:
        void draw_all(const keyboard_state& state) override
        {
            clear(0, 160);
            draw_text(state);
            draw_grid(state);
            draw_footer(state);
        }

        void key_moved(const keyboard_state& state, int old_row, int old_column) override
        {
            draw_key(state, old_row, old_column);
            draw_key(state, state.row, state.column);
        }

        void case_changed(const keyboard_state& state) override
        {
            clear(grid_band_top, grid_band_bottom);
            draw_grid(state);
        }

        void text_changed(const keyboard_state& state) override
        {
            clear(0, text_band_bottom);
            draw_text(state);
        }

        void caret_changed(const keyboard_state&) override
        {
            // The original "_" cursor doesn't blink.
        }

        void message_changed(const keyboard_state& state) override
        {
            clear(footer_band_top, 160);
            draw_footer(state);
        }

    private:
        // Title with character count, and the typed text (only the end if it's long) with a
        // trailing cursor.
        static void draw_text(const keyboard_state& state)
        {
            bn::string<48> title_text = state.title;
            title_text += "  ";
            append_padded(title_text, state.value.size(), 1);
            title_text += '/';
            append_padded(title_text, state.max_chars, 1);
            ui::classic_text(classic_font::small, title_text, 0, title_y);

            bn::string<32> input_text;
            int size = state.value.size();
            int start = size > visible_input_chars ? size - visible_input_chars : 0;

            if(start > 0)
            {
                input_text += "..";
                start += 2;
            }

            for(int i = start; i < size; ++i)
            {
                input_text += state.layout == keyboard_layout::pin ? '*' : state.value[i];
            }

            input_text += '_';
            ui::classic_text(classic_font::mono, input_text, 0, input_y);
        }

        static void draw_grid(const keyboard_state& state)
        {
            for(int r = 0; r < keyboard_state::rows; ++r)
            {
                for(int c = 0; c < state.columns(); ++c)
                {
                    draw_key(state, r, c);
                }
            }
        }

        // One key: the cursor box (behind the character) when selected.
        static void draw_key(const keyboard_state& state, int r, int c)
        {
            int x = grid_left(state) + c * cell_width;
            int y = first_row_y + r * row_spacing;

            // 16x16 box centered on (x + 4, y): screen top-left = (120 + x - 4, 72 + y).
            int box_x = 120 + x - 4;
            int box_y = 72 + y;
            ui::fill(box_x, box_y, 16, 16, ui::classic_backdrop);

            if(r == state.row && c == state.column)
            {
                ui::fill(box_x + 1, box_y, 14, 1, ui::classic_key_edge);
                ui::fill(box_x + 1, box_y + 15, 14, 1, ui::classic_key_edge);
                ui::fill(box_x, box_y + 1, 1, 14, ui::classic_key_edge);
                ui::fill(box_x + 15, box_y + 1, 1, 14, ui::classic_key_edge);
                ui::fill(box_x + 1, box_y + 1, 14, 14, ui::classic_key);
            }

            char label[2] = { state.key(r, c), 0 };
            ui::classic_text(classic_font::mono, label, x, y, ui::align::left);
        }

        // Help / message lines
        static void draw_footer(const keyboard_state& state)
        {
            ui::classic_text(classic_font::small, state.message ? state.message : "A: type   B: delete   STA: done", 0,
                             footer_row_1_y);

            if(state.layout == keyboard_layout::name)
            {
                ui::classic_text(classic_font::small, "SEL: abc/ABC   R: space   L: cancel", 0, footer_row_2_y);
            }
            else if(state.layout == keyboard_layout::base32)
            {
                ui::classic_text(classic_font::small, "L: cancel", 0, footer_row_2_y);
            }
        }
    };

    keyboard_painter_impl painter;
}

keyboard_painter& keyboard()
{
    return painter;
}

}
