// Skinned themes: on-screen keyboard (screens 06, 08 and 09 of the theme handoffs).

#include "skin.h"

#include "bn_string.h"

#include "app_text.h"

namespace app::skin
{

namespace
{
    // Same geometry as Field Guide (handoffs: "Keyboards").
    struct layout_info
    {
        int input_height;
        int panel_y;
        int panel_height;
        int grid_x;         // top-left of the first key cap
        int grid_y;
        int pitch_x;
        int pitch_y;
        int cap_width;
        int cap_height;
        int footer_top;
    };

    constexpr int input_x = 4, input_y = 28, input_width = 232;
    constexpr int panel_x = 4, panel_width = 232;

    constexpr layout_info pin_info = { 22, 56, 82, 4 + 5, 56 + 4, 24, 18, 22, 16, 144 };
    constexpr layout_info name_info = { 22, 63, 67, 4 + 9, 63 + 3, 21, 15, 19, 13, 134 };
    constexpr layout_info base32_info = { 28, 62, 70, 4 + 4, 62 + 3, 27, 16, 25, 14, 136 };

    constexpr hint type_hints[] = { { "A", "type" }, { "B", "delete" }, { "START", "done" } };
    constexpr hint name_row_2_hints[] = { { "R", "space" }, { "L", "cancel" } };
    constexpr hint base32_row_2_hints[] = { { "L", "cancel" } };

    constexpr int pin_slots = 16;
    constexpr int pin_slot_size = 9;
    constexpr int pin_slot_gap = 4;

    const layout_info& info(const keyboard_state& state)
    {
        return state.layout == keyboard_layout::pin ? pin_info :
               state.layout == keyboard_layout::base32 ? base32_info : name_info;
    }
}

void skin_keyboard_painter::draw_all(const keyboard_state& state)
{
    _skin.background(0, 0, 240, 160);
    _skin.header(state.title, false);
    draw_counter(state);
    draw_input(state);

    if(state.layout == keyboard_layout::name)
    {
        int width = _skin.pill(8, 55, "SELECT", false);
        ui::text(ui::font::body, "case", 8 + width + 3, 55, _skin.colors.hint);
    }

    draw_keyboard(state);
    draw_footer(state);
}

void skin_keyboard_painter::key_moved(const keyboard_state& state, int old_row, int old_column)
{
    draw_key(state, old_row, old_column);
    draw_key(state, state.row, state.column);
}

void skin_keyboard_painter::case_changed(const keyboard_state& state)
{
    draw_keyboard(state);
}

void skin_keyboard_painter::text_changed(const keyboard_state& state)
{
    draw_input(state);
    draw_counter(state);
}

void skin_keyboard_painter::caret_changed(const keyboard_state& state)
{
    if(state.layout != keyboard_layout::pin)
    {
        draw_input(state);
    }
}

void skin_keyboard_painter::message_changed(const keyboard_state& state)
{
    draw_footer(state);
}

void skin_keyboard_painter::draw_counter(const keyboard_state& state)
{
    bn::string<16> counter;
    append_padded(counter, state.value.size(), 1);
    counter += '/';
    append_padded(counter, state.max_chars, 1);
    _skin.header_counter(counter);
}

void skin_keyboard_painter::draw_input(const keyboard_state& state)
{
    int height = info(state).input_height;
    _skin.input_box(input_x, input_y, input_width, height);

    switch(state.layout)
    {

    case keyboard_layout::pin:
    {
        int total = pin_slots * pin_slot_size + (pin_slots - 1) * pin_slot_gap;
        int x = input_x + (input_width - total) / 2;
        int y = input_y + (height - pin_slot_size) / 2;

        for(int i = 0; i < pin_slots; ++i)
        {
            _skin.pin_slot(x + i * (pin_slot_size + pin_slot_gap), y, i < state.value.size());
        }

        break;
    }

    case keyboard_layout::base32:
    {
        constexpr int chars_per_line = 32;
        constexpr int text_x = input_x + 5;
        const int line_y[2] = { input_y + 2, input_y + 13 };
        int size = state.value.size();

        for(int line = 0; line < 2; ++line)
        {
            ui::pattern_hline(text_x, input_y + 11 + line * 11, 220, 1, 2, _skin.colors.guide);

            int start = line * chars_per_line;

            if(size > start)
            {
                int count = size - start < chars_per_line ? size - start : chars_per_line;
                ui::text(ui::font::digits, bn::string_view(state.value.data() + start, count), text_x, line_y[line],
                         _skin.colors.input);
            }
        }

        if(state.caret_visible && size < state.max_chars)
        {
            int line = size / chars_per_line;
            int column = size % chars_per_line;
            ui::fill(text_x + column * 7 - 1, line_y[line], 2, 8, _skin.colors.caret);
        }

        break;
    }

    default:
        _skin.name_input(input_x, input_y, input_width, height, bn::string_view(state.value.data(), state.value.size()),
                         state.caret_visible);
        break;
    }
}

void skin_keyboard_painter::draw_keyboard(const keyboard_state& state)
{
    const layout_info& layout = info(state);

    if(state.layout == keyboard_layout::name)
    {
        _skin.case_tabs(state.lower_case);
    }

    _skin.key_panel(panel_x, layout.panel_y, panel_width, layout.panel_height);

    for(int r = 0; r < keyboard_state::rows; ++r)
    {
        for(int c = 0; c < state.columns(); ++c)
        {
            if(r != state.row || c != state.column)
            {
                draw_key(state, r, c);
            }
        }
    }

    // Last, so the neighbours don't paint over its outline.
    draw_key(state, state.row, state.column);
}

void skin_keyboard_painter::draw_key(const keyboard_state& state, int row, int column)
{
    const layout_info& layout = info(state);
    _skin.key(layout.grid_x + column * layout.pitch_x, layout.grid_y + row * layout.pitch_y, layout.cap_width,
              layout.cap_height, state.key(row, column), row == state.row && column == state.column);
}

void skin_keyboard_painter::draw_footer(const keyboard_state& state)
{
    int top = info(state).footer_top;
    _skin.background(0, top, 240, 160 - top);
    _skin.footer_bar(top);

    if(state.message)
    {
        footer_message(_skin, top + 3, state.message);
    }
    else
    {
        footer_hints(_skin, top + 3, type_hints, 3);
    }

    if(state.layout == keyboard_layout::name)
    {
        footer_hints(_skin, top + 14, name_row_2_hints, 2);
    }
    else if(state.layout == keyboard_layout::base32)
    {
        footer_hints(_skin, top + 14, base32_row_2_hints, 1);
    }
}

}
