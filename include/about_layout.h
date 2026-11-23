#ifndef ABOUT_LAYOUT_H
#define ABOUT_LAYOUT_H

// About screen layout shared by Field Guide, Net Link, Pocket LCD and Phosphor. Native pixels,
// top-left. Page 1 is one panel (logo, name, author, description, link and its QR Code);
// pages 2 and 3 are the info box with the page title over a panel of label / value items.

#include "otp_export.h"
#include "theme.h"
#include "ui.h"
#include "version.h"

namespace app::about_layout
{
    // One panel under the header on every page; the page title is the header's.
    constexpr int panel_x = 4, panel_y = 28, panel_width = 232, panel_height = 110;
    constexpr int item_first_y = panel_y + 7, item_step = 26;
    constexpr int text_x = 12;
    constexpr int qr_module = 2;

    // Page 1 inside the panel: text in text / dim, the logo in the theme's logo colors.
    inline void intro(const about_state& state, uint8_t text, uint8_t dim, const uint8_t* logo_colors)
    {
        ui::logo(ui::logo_frame::big, text_x, panel_y + 8, logo_colors);
        ui::text(ui::font::title12, "AuthBoy", text_x + 38, panel_y + 10, text);
        ui::text(ui::font::digits, app_version_label, text_x + 38, panel_y + 27, dim);

        ui::text(ui::font::body, "TOTP authenticator for", text_x, panel_y + 50, text);
        ui::text(ui::font::body, "the Game Boy Advance", text_x, panel_y + 62, text);
        ui::text(ui::font::body, about_author, text_x, panel_y + 74, dim);
        ui::text(ui::font::body, "github.com/billyadam/AuthBoy", text_x, panel_y + 94, dim);

        if(state.qrcode)
        {
            // Black on white with its quiet zone, top right of the panel (above the link).
            int side = (otp_export::qr_size(state.qrcode) + 8) * qr_module;
            ui::qr_code(state.qrcode, panel_x + panel_width - 6 - side, panel_y + 6, side, qr_module);
        }
    }

    // Word wrap: calls line(text) for each line of text that fits in max_width. Returns the
    // number of lines.
    template<typename Width, typename Line>
    int wrap(const char* text, int max_width, Width width_of, Line line)
    {
        int count = 0;

        while(*text)
        {
            while(*text == ' ')
            {
                ++text;
            }

            // Longest run of whole words that fits (at least one word).
            int end = 0;
            int fit = 0;

            while(text[end])
            {
                int word_end = end;

                while(text[word_end] == ' ')
                {
                    ++word_end;
                }

                while(text[word_end] && text[word_end] != ' ')
                {
                    ++word_end;
                }

                if(fit && width_of(bn::string_view(text, word_end)) > max_width)
                {
                    break;
                }

                fit = word_end;
                end = word_end;
            }

            if(! fit)
            {
                break;
            }

            line(bn::string_view(text, fit));
            ++count;
            text += fit;
        }

        return count;
    }

    // Security page inside the panel.
    constexpr int security_line_step = 10, security_paragraph_gap = 3;

    inline void security(int page, uint8_t text)
    {
        int y = panel_y + 5;
        int first, count;
        about_security_paragraphs(page, first, count);

        for(int i = first; i < first + count; ++i)
        {
            wrap(about_security_text[i], panel_width - 2 * (text_x - panel_x),
                 [](const bn::string_view& line) { return ui::text_width(ui::font::body, line); },
                 [&](const bn::string_view& line) {
                     ui::text(ui::font::body, line, text_x, y, text);
                     y += security_line_step;
                 });
            y += security_paragraph_gap;
        }
    }

    // Pages 3 and 4 inside the panel: label (dim) over value.
    inline void items(int page, uint8_t text, uint8_t dim)
    {
        const about_item* list = about_page_items(page);

        for(int i = 0; i < about_item_count; ++i)
        {
            int y = item_first_y + i * item_step;
            ui::text(ui::font::body, list[i].label, text_x, y, dim);
            ui::text(ui::font::body, list[i].value, text_x, y + 11, text);
        }
    }
}

#endif
