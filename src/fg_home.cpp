// Field Guide theme: home screen (design handoff screens 01, 03 and 04).

#include "theme.h"

#include "bn_string.h"

#include "app_text.h"
#include "totp.h"
#include "ui.h"

namespace app::field_guide
{

namespace
{
    constexpr int totp_period = home_state::totp_period;

    // Screen 01 geometry
    constexpr int list_x = 4, list_y = 41, list_width = 232, list_height = 99;
    constexpr int inner_left = list_x + 2;                      // 6
    constexpr int inner_right = list_x + list_width - 2;        // 234 (exclusive)
    constexpr int inner_top = list_y + 2;                       // 43
    constexpr int inner_bottom = list_y + list_height - 2;      // 138 (exclusive)
    constexpr int rows_top = inner_top + 3;                     // 46
    constexpr int row_x = inner_left + 3;                       // 9
    constexpr int row_height = 15;
    constexpr int selected_row_height = 23;
    constexpr int row_gap = 1;
    constexpr int scrollbar_x = inner_right - 3 - 4;            // 227
    constexpr int scrollbar_height = 89;

    constexpr int time_tag_x = 6, time_tag_y = 28;
    constexpr int time_bar_y = 29, time_bar_height = 8;
    constexpr int time_label_right = 233;
    constexpr int time_gap = 4;                                 // around the bar
    constexpr int header_title_end = 104;                       // the 12 px "AuthBoy" (with its shadow) ends before this
    constexpr int footer_top = 144, footer_text_y = 147;

    // Account menu (screen 03) and delete confirm (screen 04)
    constexpr int dialog_x = 4, dialog_y = 114, dialog_width = 128, dialog_height = 44;
    constexpr int command_x = 134, command_width = 102, command_bottom = 158;
    constexpr int confirm_x = 4, confirm_y = 108, confirm_width = 232, confirm_height = 50;

    // Name text that fits max_width, with "..." if it had to be shortened.
    template<int MaxSize>
    void fitted_name(bn::string<MaxSize>& text, const char* name, int max_width)
    {
        bool truncated;
        text += ui::fit_text(ui::font::body, name, max_width, 1, truncated);

        if(truncated)
        {
            text += "...";
        }
    }

    int row_h(const home_state& s, int position)
    {
        return position == s.selected ? selected_row_height : row_height;
    }

    bool scrolling(const home_state& s)
    {
        int total = s.accounts.count * (row_height + row_gap) - row_gap + (selected_row_height - row_height);
        return s.accounts.count > 0 && total > inner_bottom - rows_top;
    }

    int row_width(const home_state& s)
    {
        return (scrolling(s) ? inner_right - 10 : inner_right - 3) - row_x;
    }

    // Rows that fully fit when the list starts at first.
    int fitting_rows(const home_state& s, int first)
    {
        int y = rows_top;
        int count = 0;

        for(int position = first; position < s.accounts.count; ++position)
        {
            if(y + row_h(s, position) > inner_bottom)
            {
                break;
            }

            y += row_h(s, position) + row_gap;
            ++count;
        }

        return count;
    }

    int selected_row_y(const home_state& s)
    {
        int y = rows_top;

        for(int position = s.first_visible; position < s.selected; ++position)
        {
            y += row_h(s, position) + row_gap;
        }

        return y;
    }

    class home_painter_impl final : public home_painter
    {

    public:
        // Keeps the selected row fully visible, and the list filled to the bottom.
        void layout(home_state& s) override
        {
            s.prepare();

            if(s.first_visible > s.selected)
            {
                s.first_visible = s.selected;
            }

            while(s.first_visible < s.selected && s.first_visible + fitting_rows(s, s.first_visible) <= s.selected)
            {
                ++s.first_visible;
            }

            while(s.first_visible > 0 && s.first_visible - 1 + fitting_rows(s, s.first_visible - 1) >= s.accounts.count)
            {
                --s.first_visible;
            }

            s.visible_count = fitting_rows(s, s.first_visible);
        }

        void draw_all(home_state& s) override
        {
            layout(s);
            ui::background();
            ui::header("AuthBoy");
            draw_clock(s);
            draw_time_bar(s);
            draw_list(s);
            draw_footer(s);
            draw_modal(s);
        }

        void second_passed(home_state& s) override
        {
            draw_clock(s);
            draw_time_bar(s);
        }

        void codes_changed(home_state& s) override
        {
            layout(s);
            draw_list(s);

            if(s.modal != home_modal::none)
            {
                draw_modal(s);
            }
        }

        // List + whatever sits on top of it.
        void list_changed(home_state& s) override
        {
            layout(s);
            draw_list(s);
            draw_footer(s);
            draw_modal(s);
        }

        // When the list didn't scroll, only the rows between the old and new position change, so
        // only those are repainted. Rows keep their total height (one tall row either way).
        void selection_moved(home_state& s, int old_selected) override
        {
            int old_first_visible = s.first_visible;
            layout(s);

            int first = old_selected < s.selected ? old_selected : s.selected;
            int last = old_selected < s.selected ? s.selected : old_selected;

            if(s.first_visible != old_first_visible || s.modal != home_modal::none || first < s.first_visible ||
               last >= s.first_visible + s.visible_count)
            {
                draw_list(s);
                return;
            }

            int y = rows_top;

            for(int position = s.first_visible; position < first; ++position)
            {
                y += row_h(s, position) + row_gap;
            }

            int height = -row_gap;

            for(int position = first; position <= last; ++position)
            {
                height += row_h(s, position) + row_gap;
            }

            ui::fill(row_x, y, row_width(s), height, ui::white);

            for(int position = first; position <= last; ++position)
            {
                draw_row(s, position, y);
                y += row_h(s, position) + row_gap;
            }
        }

        void modal_opened(home_state& s) override
        {
            draw_footer(s);     // hidden while a modal is open
            draw_modal(s);
        }

        void menu_moved(home_state& s, int old_menu_selected) override
        {
            draw_menu_item(s, old_menu_selected);
            draw_menu_item(s, s.menu_selected);
        }

        void footer_changed(home_state& s) override
        {
            draw_footer(s);
        }

        // Cursor bob: 1 px right for 16 frames, then back.
        void animate(home_state& s) override
        {
            if(s.frame_counter % 16 != 0 || s.rtc.status != rtc_status::ok || s.accounts.count == 0)
            {
                return;
            }

            if(s.modal == home_modal::menu)
            {
                draw_menu_item(s, s.menu_selected);
            }
            else if(s.modal == home_modal::none && ! s.moving)
            {
                draw_cursor(s, selected_row_y(s));
            }
        }

    private:
        static void draw_clock(const home_state& s)
        {
            ui::header_band_clear(header_title_end, 240 - header_title_end);

            if(s.rtc.status != rtc_status::ok)
            {
                return;
            }

            totp::civil_time civil = totp::civil_from_unix(s.rtc.local_time);
            bn::string<16> time_text;
            append_padded(time_text, civil.hour, 2);
            time_text += ':';
            append_padded(time_text, civil.minute, 2);
            time_text += ':';
            append_padded(time_text, civil.second, 2);

            bn::string<32> date_text;
            append_padded(date_text, civil.year, 4);
            date_text += '-';
            append_padded(date_text, civil.month, 2);
            date_text += '-';
            append_padded(date_text, civil.day, 2);
            date_text += " UTC";

            // Short form ("+7", "+5:30") so the date doesn't run into the title.
            int abs_offset = utc_offset_minutes < 0 ? -utc_offset_minutes : utc_offset_minutes;
            date_text += utc_offset_minutes < 0 ? '-' : '+';
            append_padded(date_text, abs_offset / 60, 1);

            if(abs_offset % 60)
            {
                date_text += ':';
                append_padded(date_text, abs_offset % 60, 2);
            }

            ui::text(ui::font::digits6, time_text, 233, 4, ui::white, ui::no_shadow, 1, ui::align::right);
            ui::text(ui::font::digits6, date_text, 233, 13, ui::header_subtext, ui::no_shadow, 1, ui::align::right);
        }

        static void draw_time_bar(const home_state& s)
        {
            ui::stripes(0, 26, 240, 14);

            if(s.rtc.status != rtc_status::ok)
            {
                return;
            }

            int remaining = s.remaining_seconds();

            // TIME tag
            int tag_width = ui::text_width(ui::font::digits, "TIME") + 4;

            // The bar sits between the tag and the widest seconds label ("30s").
            int time_bar_x = time_tag_x + tag_width + time_gap;
            int time_bar_width = time_label_right - ui::text_width(ui::font::digits, "30s") - time_gap - time_bar_x;
            ui::round_fill(time_tag_x, time_tag_y, tag_width, 10, 2, ui::ink);
            ui::text(ui::font::digits, "TIME", time_tag_x + 2, time_tag_y + 1, ui::selection);

            // Bar: green > 10 s, yellow 6-10 s, red <= 5 s
            uint8_t fill_color = ui::green;
            uint8_t shine_color = ui::green_shine;

            if(remaining <= 5)
            {
                fill_color = ui::bar_red;
                shine_color = ui::red_shine;
            }
            else if(remaining <= 10)
            {
                fill_color = ui::yellow;
                shine_color = ui::selection_shine;
            }

            ui::round_fill(time_bar_x, time_bar_y, time_bar_width, time_bar_height, 4, ui::ink);

            int inner_width = time_bar_width - 2;
            int fill_width = inner_width * remaining / totp_period;

            if(fill_width > 0)
            {
                ui::round_fill(time_bar_x + 1, time_bar_y + 1, fill_width, 6, 3, fill_color);

                if(fill_width > 4)
                {
                    ui::fill(time_bar_x + 3, time_bar_y + 1, fill_width - 4, 2, shine_color);
                }
            }

            bn::string<8> seconds_text;
            append_padded(seconds_text, remaining, 1);
            seconds_text += 's';
            ui::text(ui::font::digits, seconds_text, time_label_right, time_bar_y, remaining <= 5 ? ui::red : ui::ink,
                     ui::no_shadow, 1, ui::align::right);
        }

        static void draw_badge(int x, int y, int size, const account& item, bool bordered)
        {
            uint8_t color = ui::badge_color(item.label);

            if(bordered)
            {
                ui::round_fill(x, y, size, size, 2, ui::ink);
                ui::round_fill(x + 1, y + 1, size - 2, size - 2, 2, color);
            }
            else
            {
                ui::round_fill(x, y, size, size, 2, color);
            }

            char initial = item.label[0];

            if(initial >= 'a' && initial <= 'z')
            {
                initial = char(initial - 'a' + 'A');
            }

            char initial_text[2] = { initial, 0 };
            ui::text(ui::font::digits, initial_text, x + size / 2, y + (size - 7) / 2, ui::white, ui::no_shadow, 1,
                     ui::align::center);
        }

        static void draw_cursor(const home_state& s, int y)
        {
            ui::fill(row_x + 1, y + 2, 9, 19, ui::selection);

            if(s.moving)
            {
                ui::cursor_up(row_x + 3, y + 4, ui::red);
                ui::cursor_down(row_x + 3, y + 16, ui::red);
            }
            else
            {
                ui::cursor_right(row_x + 2 + ui::cursor_bob(s.frame_counter), y + 7, ui::red);
            }
        }

        static void draw_row(home_state& s, int position, int y)
        {
            const account& item = s.item(position);
            int width = row_width(s);
            int right = row_x + width;

            uint32_t code = s.code(position);
            bn::string<8> code_text;
            append_padded(code_text, int(code / 1000), 3);
            code_text += ' ';
            append_padded(code_text, int(code % 1000), 3);

            if(position == s.selected)
            {
                ui::round_fill(row_x, y, width, selected_row_height, 2, ui::selection_edge);
                ui::round_fill(row_x + 1, y + 1, width - 2, selected_row_height - 2, 2, ui::selection);
                ui::fill(row_x + 2, y + selected_row_height - 2, width - 4, 1, ui::row_inner_shade);
                draw_cursor(s, y);
                draw_badge(row_x + 12, y + 4, 14, item, true);

                int code_width = ui::text_width(ui::font::code12, code_text);
                int name_x = row_x + 12 + 14 + 5;
                bn::string<24> name;
                fitted_name(name, item.label, right - 4 - code_width - 6 - name_x);
                ui::text(ui::font::body, name, name_x, y + 8, ui::ink);
                ui::text(ui::font::code12, code_text, right - 4, y + 5, ui::ink, ui::selection_shine, 1,
                         ui::align::right);
            }
            else
            {
                if(position % 2)
                {
                    ui::round_fill(row_x, y, width, row_height, 2, ui::zebra);
                }

                draw_badge(row_x + 12, y + 2, 10, item, false);

                int code_width = ui::text_width(ui::font::digits, code_text);
                int name_x = row_x + 12 + 10 + 5;
                bn::string<24> name;
                fitted_name(name, item.label, right - 4 - code_width - 6 - name_x);
                ui::text(ui::font::body, name, name_x, y + 4, ui::ink);
                ui::text(ui::font::digits, code_text, right - 4, y + 4, ui::ink, ui::no_shadow, 1, ui::align::right);
            }
        }

        static void draw_list(home_state& s)
        {
            ui::panel(list_x, list_y, list_width, list_height, ui::white, false);

            if(s.rtc.status != rtc_status::ok)
            {
                bool missing = s.rtc.status == rtc_status::missing;
                ui::text(ui::font::body, missing ? "RTC not found" : "RTC returned an invalid time", 120, 78, ui::ink,
                         ui::light_shadow, 1, ui::align::center);
                ui::text(ui::font::body, missing ? "Needs a cart/emulator with a clock" :
                                                   "Set it with your flashcart's tools",
                         120, 92, ui::secondary, ui::no_shadow, 1, ui::align::center);
                return;
            }

            if(s.accounts.count == 0)
            {
                ui::text(ui::font::body, "No accounts yet", 120, 78, ui::ink, ui::light_shadow, 1, ui::align::center);
                ui::text(ui::font::body, "START: add an account", 120, 92, ui::secondary, ui::no_shadow, 1,
                         ui::align::center);
                return;
            }

            int y = rows_top;

            for(int i = 0; i < s.visible_count; ++i)
            {
                int position = s.first_visible + i;
                draw_row(s, position, y);
                y += row_h(s, position) + row_gap;
            }

            if(scrolling(s))
            {
                ui::round_fill(scrollbar_x, rows_top, 4, scrollbar_height, 2, ui::light_shadow);

                int thumb_height = scrollbar_height * s.visible_count / s.accounts.count;

                if(thumb_height < 8)
                {
                    thumb_height = 8;
                }

                int hidden = s.accounts.count - s.visible_count;
                int thumb_y = rows_top + (hidden > 0 ? (scrollbar_height - thumb_height) * s.first_visible / hidden : 0);
                ui::round_fill(scrollbar_x, thumb_y, 4, thumb_height, 2, ui::scroll_thumb);
            }
        }

        static void draw_footer(const home_state& s)
        {
            if(s.modal != home_modal::none)
            {
                ui::stripes(0, list_y + list_height, 240, 160 - (list_y + list_height));
                return;
            }

            ui::stripes(0, list_y + list_height, 240, footer_top - (list_y + list_height));
            ui::footer_bar(footer_top);

            if(s.footer_message_frames > 0)
            {
                bn::string<32> message = "List full: ";
                append_padded(message, max_accounts, 1);
                message += " accounts";
                ui::footer_message(footer_text_y, message);
            }
            else if(s.moving)
            {
                constexpr ui::hint hints[] = { { "UP/DOWN", "move" }, { "A", "done" } };
                ui::footer_hints(footer_text_y, hints, 2);
            }
            else
            {
                constexpr ui::hint hints[] = { { "START", "add" }, { "SELECT", "settings" } };
                ui::footer_hints(footer_text_y, hints, 2);
            }
        }

        static void draw_menu_item(const home_state& s, int index)
        {
            int height = 9 + s.menu_count() * 18 + 1;
            int box_y = command_bottom - height;
            home_option option = s.menu_option(index);
            bool danger = option == home_option_delete;
            bool is_selected = index == s.menu_selected;

            ui::menu_item(command_x, command_width, box_y + 9 + index * 18, home_option_names[option],
                          danger ? ui::red : ui::ink, danger ? ui::danger_shadow : ui::light_shadow, is_selected,
                          is_selected ? ui::cursor_bob(s.frame_counter) : 0);
        }

        static void draw_modal(const home_state& s)
        {
            if(s.modal == home_modal::menu)
            {
                const account& item = s.item(s.selected);
                ui::panel(dialog_x, dialog_y, dialog_width, dialog_height, ui::white, true);
                ui::text(ui::font::body, "What to do with", dialog_x + 7, dialog_y + 9, ui::ink, ui::light_shadow);

                bn::string<32> name;
                fitted_name(name, item.label, dialog_width - 14 - 8);
                name += '?';
                ui::text(ui::font::body, name, dialog_x + 7, dialog_y + 22, ui::ink, ui::light_shadow);

                int height = 9 + s.menu_count() * 18 + 1;
                ui::panel(command_x, command_bottom - height, command_width, height, ui::white, true);

                for(int i = 0; i < s.menu_count(); ++i)
                {
                    draw_menu_item(s, i);
                }
            }
            else if(s.modal == home_modal::delete_confirm)
            {
                const account& item = s.item(s.selected);
                ui::panel(confirm_x, confirm_y, confirm_width, confirm_height, ui::white, true);

                bn::string<40> question = "Delete \"";
                fitted_name(question, item.label, confirm_width - 16 - 60);
                question += "\"?";
                ui::text(ui::font::body, question, confirm_x + 8, confirm_y + 9, ui::ink, ui::light_shadow);

                int x = confirm_x + 8;
                int y = confirm_y + 28;
                ui::round_button(x, y - 1, "A", ui::red, ui::white);
                ui::text(ui::font::body, "delete", x + 13, y, ui::red, ui::danger_shadow);
                x += 13 + ui::text_width(ui::font::body, "delete") + 14;
                ui::round_button(x, y - 1, "B", ui::ink, ui::white);
                ui::text(ui::font::body, "keep", x + 13, y, ui::ink, ui::light_shadow);
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
