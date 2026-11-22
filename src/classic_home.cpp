// Classic theme: home screen, as in the original release (sprite text on a dark blue screen).
// Coordinates are the original sprite ones: (0, 0) is the screen center (see ui::classic_text).

#include "theme.h"

#include "bn_string.h"

#include "app_text.h"
#include "totp.h"
#include "ui.h"

namespace app::classic
{

namespace
{
    using ui::classic_font;

    constexpr int bar_slots = 15;

    // List: 5 rows of "> label (16 chars)  123 456" in the mono font.
    constexpr int visible_rows = 5;
    constexpr int first_row_y = -30;
    constexpr int row_spacing = 18;
    constexpr int row_chars = 2 + max_label_chars + 2 + 7;
    constexpr int row_left_x = -(row_chars * 8) / 2;

    // Screen bands (y ranges) repainted on their own.
    constexpr int clock_band_bottom = 38;     // clock line (y -70) and countdown line (y -52)

    // Popup menu: 192x128 panel centered on the screen.
    constexpr int popup_x = 24, popup_y = 16, popup_width = 192, popup_height = 128;

    void clear(int top, int bottom)
    {
        ui::fill(0, top, 240, bottom - top, ui::classic_backdrop);
    }

    class home_painter_impl final : public home_painter
    {

    public:
        // Scrolls just enough to keep the selected row on screen.
        void layout(home_state& s) override
        {
            s.prepare();

            if(s.selected < s.first_visible)
            {
                s.first_visible = s.selected;
            }
            else if(s.selected >= s.first_visible + visible_rows)
            {
                s.first_visible = s.selected - visible_rows + 1;
            }

            if(s.first_visible > s.accounts.count - visible_rows)
            {
                s.first_visible = s.accounts.count > visible_rows ? s.accounts.count - visible_rows : 0;
            }

            int remaining = s.accounts.count - s.first_visible;
            s.visible_count = remaining < visible_rows ? remaining : visible_rows;
        }

        void draw_all(home_state& s) override
        {
            layout(s);
            clear(0, 160);

            if(s.rtc.status != rtc_status::ok)
            {
                bool missing = s.rtc.status == rtc_status::missing;
                ui::classic_text(classic_font::small, missing ? "RTC not found" : "RTC returned an invalid time", 0, -8);
                ui::classic_text(classic_font::small, missing ? "Needs a cart/emulator with a clock" :
                                                                "Set it with your flashcart's tools", 0, 12);
                return;
            }

            if(s.modal == home_modal::delete_confirm)
            {
                bn::string<48> question = "Delete \"";
                question += s.item(s.selected).label;
                question += "\"?";
                ui::classic_text(classic_font::small, question, 0, -12);
                ui::classic_text(classic_font::small, "A: delete   B: keep", 0, 20);
                return;
            }

            draw_clock(s);
            draw_list(s);

            if(s.modal == home_modal::menu)
            {
                draw_popup(s);
            }
        }

        void second_passed(home_state& s) override
        {
            if(s.modal == home_modal::none)
            {
                clear(0, clock_band_bottom);
                draw_clock(s);
            }
            else if(s.modal == home_modal::menu)
            {
                draw_all(s);    // the popup covers part of the clock lines
            }
        }

        void codes_changed(home_state& s) override
        {
            redraw_list(s);
        }

        void list_changed(home_state& s) override
        {
            draw_all(s);
        }

        void selection_moved(home_state& s, int) override
        {
            redraw_list(s);
        }

        void modal_opened(home_state& s) override
        {
            draw_all(s);
        }

        void menu_moved(home_state& s, int) override
        {
            draw_popup(s);
        }

        void footer_changed(home_state& s) override
        {
            redraw_list(s);
        }

        void animate(home_state&) override
        {
        }

    private:
        // List and footer; the popup / delete question are drawn over everything.
        void redraw_list(home_state& s)
        {
            if(s.modal != home_modal::none || s.rtc.status != rtc_status::ok)
            {
                draw_all(s);
                return;
            }

            layout(s);
            clear(clock_band_bottom, 160);
            draw_list(s);
        }

        // Clock and shared countdown
        static void draw_clock(const home_state& s)
        {
            bn::string<40> clock_text;
            append_date_time(clock_text, totp::civil_from_unix(s.rtc.local_time));
            clock_text += "  ";
            append_utc_offset(clock_text, utc_offset_minutes);
            ui::classic_text(classic_font::small, clock_text, 0, -70);

            int remaining = s.remaining_seconds();
            int filled = (remaining * bar_slots + home_state::totp_period - 1) / home_state::totp_period;
            bn::string<32> bar_text;

            for(int i = 0; i < bar_slots; ++i)
            {
                bar_text += i < filled ? '#' : '-';
            }

            bar_text += ' ';
            append_padded(bar_text, remaining, 2);
            bar_text += 's';
            ui::classic_text(classic_font::mono, bar_text, 0, -52);
        }

        static void draw_list(home_state& s)
        {
            if(s.accounts.count == 0)
            {
                ui::classic_text(classic_font::small, "No accounts yet", 0, -4);
                ui::classic_text(classic_font::small, "STA: add an account", 0, 16);
                ui::classic_text(classic_font::small, "SEL: settings", 0, 66);
                return;
            }

            for(int row = 0; row < s.visible_count; ++row)
            {
                int position = s.first_visible + row;
                const account& item = s.item(position);
                bn::string<row_chars + 4> row_text;

                if(position != s.selected)
                {
                    row_text += "  ";
                }
                else
                {
                    row_text += s.moving ? "* " : "> ";
                }

                int label_chars = 0;

                for(; label_chars < max_label_chars && item.label[label_chars]; ++label_chars)
                {
                    row_text += item.label[label_chars];
                }

                for(; label_chars < max_label_chars; ++label_chars)
                {
                    row_text += ' ';
                }

                row_text += "  ";

                uint32_t code = s.code(position);
                append_padded(row_text, int(code / 1000), 3);
                row_text += ' ';
                append_padded(row_text, int(code % 1000), 3);
                ui::classic_text(classic_font::mono, row_text, row_left_x, first_row_y + row * row_spacing,
                                 ui::align::left);
            }

            // Footer: scroll hints and controls
            bn::string<48> footer_text;
            footer_text += s.first_visible > 0 ? "^   " : "    ";

            if(s.footer_message_frames > 0)
            {
                footer_text += "List full: ";
                append_padded(footer_text, max_accounts, 1);
                footer_text += " accounts";
            }
            else
            {
                footer_text += s.moving ? "Up/Down: move   A: done" : "STA: add   SEL: settings";
            }

            footer_text += s.first_visible + s.visible_count < s.accounts.count ? "   v" : "    ";
            ui::classic_text(classic_font::small, footer_text, 0, 66);
        }

        static void draw_popup(const home_state& s)
        {
            ui::fill(popup_x, popup_y, popup_width, popup_height, ui::classic_panel);
            ui::classic_text(classic_font::small, s.item(s.selected).label, 0, -50);

            for(int i = 0; i < s.menu_count(); ++i)
            {
                bn::string<24> line = i == s.menu_selected ? "> " : "   ";
                line += home_option_names[s.menu_option(i)];
                ui::classic_text(classic_font::small, line, -36, -26 + i * 16, ui::align::left);
            }
        }
    };

    home_painter_impl painter;
}

home_painter& home()
{
    return painter;
}

}
