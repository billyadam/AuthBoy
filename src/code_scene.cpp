#include "code_scene.h"

#include "bn_keypad.h"
#include "bn_string.h"

#include "app_clock.h"
#include "app_text.h"
#include "entropy.h"
#include "totp.h"
#include "ui.h"

namespace app
{

namespace
{
    constexpr int totp_period = 30;
    constexpr int totp_digits = 6;

    // Screen 01 geometry (design handoff)
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
    constexpr int header_title_end = 104;                       // the 2x "AuthBoy" (with its shadow) ends before this
    constexpr int footer_top = 144, footer_text_y = 147;

    // Account menu (screen 03) and delete confirm (screen 04)
    constexpr int dialog_x = 4, dialog_y = 114, dialog_width = 128, dialog_height = 44;
    constexpr int command_x = 134, command_width = 102, command_bottom = 158;
    constexpr int confirm_x = 4, confirm_y = 108, confirm_width = 232, confirm_height = 50;

    enum popup_option
    {
        popup_rename,
        popup_change_key,
        popup_move,
        popup_delete,
        popup_cancel,
    };

    constexpr const char* popup_option_names[] = { "Rename", "Change key", "Move", "Delete", "Cancel" };
    constexpr popup_option custom_order_options[] = {
        popup_rename, popup_change_key, popup_move, popup_delete, popup_cancel
    };
    constexpr popup_option sorted_options[] = { popup_rename, popup_change_key, popup_delete, popup_cancel };

    enum class modal_type
    {
        none,
        menu,
        delete_confirm,
    };

    // One step on press, then repeats while held.
    bool repeat(bool held, int& held_frames)
    {
        if(! held)
        {
            held_frames = 0;
            return false;
        }

        ++held_frames;
        return held_frames == 1 || (held_frames > 18 && (held_frames - 18) % 5 == 0);
    }

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

    class home_view
    {

    public:
        home_view(account_list& accounts, int& selected) :
            _accounts(accounts),
            _selected(selected)
        {
        }

        account_list& _accounts;
        int& _selected;
        int order[max_accounts];
        int first_visible = 0;
        int visible_count = 0;
        modal_type modal = modal_type::none;
        int menu_selected = 0;
        bool moving = false;
        int frame_counter = 0;
        int footer_message_frames = 0;      // > 0: "List full" shown in the footer

        // Codes only change every 30 s: computed once per period, by stored account index.
        uint32_t codes[max_accounts];
        bool code_known[max_accounts];
        int64_t codes_period = -1;

        void invalidate_codes()
        {
            codes_period = -1;
        }

        uint32_t code_for(int index)
        {
            int64_t period = utc() / totp_period;

            if(period != codes_period)
            {
                codes_period = period;

                for(int i = 0; i < _accounts.count; ++i)
                {
                    code_known[i] = false;
                }
            }

            if(! code_known[index])
            {
                const account& item = _accounts.items[index];
                codes[index] = totp::totp(item.secret, item.secret_size, utc(), totp_digits, totp_period);
                code_known[index] = true;
            }

            return codes[index];
        }
        rtc_reading rtc = {};

        int row_h(int position) const
        {
            return position == _selected ? selected_row_height : row_height;
        }

        bool scrolling() const
        {
            int total = _accounts.count * (row_height + row_gap) - row_gap +
                        (selected_row_height - row_height);
            return _accounts.count > 0 && total > inner_bottom - rows_top;
        }

        int row_width() const
        {
            return (scrolling() ? inner_right - 10 : inner_right - 3) - row_x;
        }

        // Rows that fully fit when the list starts at first.
        int fitting_rows(int first) const
        {
            int y = rows_top;
            int count = 0;

            for(int position = first; position < _accounts.count; ++position)
            {
                if(y + row_h(position) > inner_bottom)
                {
                    break;
                }

                y += row_h(position) + row_gap;
                ++count;
            }

            return count;
        }

        // Keeps the selected row fully visible, and the list filled to the bottom.
        void update_layout()
        {
            display_order(_accounts, order);

            if(_selected >= _accounts.count)
            {
                _selected = _accounts.count - 1;
            }

            if(_selected < 0)
            {
                _selected = 0;
            }

            if(first_visible > _selected)
            {
                first_visible = _selected;
            }

            while(first_visible < _selected && first_visible + fitting_rows(first_visible) <= _selected)
            {
                ++first_visible;
            }

            while(first_visible > 0 && first_visible - 1 + fitting_rows(first_visible - 1) >= _accounts.count)
            {
                --first_visible;
            }

            visible_count = fitting_rows(first_visible);
        }

        int selected_row_y() const
        {
            int y = rows_top;

            for(int position = first_visible; position < _selected; ++position)
            {
                y += row_h(position) + row_gap;
            }

            return y;
        }

        int64_t utc() const
        {
            return utc_time(rtc);
        }

        void draw_clock()
        {
            ui::header_band_clear(header_title_end, 240 - header_title_end);

            if(rtc.status != rtc_status::ok)
            {
                return;
            }

            totp::civil_time civil = totp::civil_from_unix(rtc.local_time);
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

            // Short form ("+7", "+5:30") so the date doesn't run into the 2x title.
            int abs_offset = utc_offset_minutes < 0 ? -utc_offset_minutes : utc_offset_minutes;
            date_text += utc_offset_minutes < 0 ? '-' : '+';
            append_padded(date_text, abs_offset / 60, 1);

            if(abs_offset % 60)
            {
                date_text += ':';
                append_padded(date_text, abs_offset % 60, 2);
            }

            ui::text(ui::font::digits, time_text, 233, 3, ui::white, ui::no_shadow, 1, ui::align::right);
            ui::text(ui::font::digits, date_text, 233, 12, ui::header_subtext, ui::no_shadow, 1, ui::align::right);
        }

        void draw_time_bar()
        {
            ui::stripes(0, 26, 240, 14);

            if(rtc.status != rtc_status::ok)
            {
                return;
            }

            int remaining = totp_period - int(utc() % totp_period);

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
            ui::text(ui::font::digits, seconds_text, time_label_right, time_bar_y, remaining <= 5 ? ui::red : ui::ink, ui::no_shadow, 1,
                     ui::align::right);
        }

        void draw_badge(int x, int y, int size, const account& item, bool bordered)
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

        void draw_cursor(int y)
        {
            ui::fill(row_x + 1, y + 2, 9, 19, ui::selection);

            if(moving)
            {
                ui::cursor_up(row_x + 3, y + 4, ui::red);
                ui::cursor_down(row_x + 3, y + 16, ui::red);
            }
            else
            {
                ui::cursor_right(row_x + 2 + ui::cursor_bob(frame_counter), y + 7, ui::red);
            }
        }

        void draw_row(int position, int y)
        {
            const account& item = _accounts.items[order[position]];
            int width = row_width();
            int right = row_x + width;

            uint32_t code = code_for(order[position]);
            bn::string<8> code_text;
            append_padded(code_text, int(code / 1000), 3);
            code_text += ' ';
            append_padded(code_text, int(code % 1000), 3);

            if(position == _selected)
            {
                ui::round_fill(row_x, y, width, selected_row_height, 2, ui::selection_edge);
                ui::round_fill(row_x + 1, y + 1, width - 2, selected_row_height - 2, 2, ui::selection);
                ui::fill(row_x + 2, y + selected_row_height - 2, width - 4, 1, ui::row_inner_shade);
                draw_cursor(y);
                draw_badge(row_x + 12, y + 4, 14, item, true);

                int code_width = ui::text_width(ui::font::digits, code_text, 2);
                int name_x = row_x + 12 + 14 + 5;
                bn::string<24> name;
                fitted_name(name, item.label, right - 4 - code_width - 6 - name_x);
                ui::text(ui::font::body, name, name_x, y + 8, ui::ink);
                ui::text(ui::font::digits, code_text, right - 4, y + 4, ui::ink, ui::selection_shine, 2,
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

        void draw_list()
        {
            ui::panel(list_x, list_y, list_width, list_height, ui::white, false);

            if(rtc.status != rtc_status::ok)
            {
                bool missing = rtc.status == rtc_status::missing;
                ui::text(ui::font::body, missing ? "RTC not found" : "RTC returned an invalid time", 120, 78, ui::ink,
                         ui::light_shadow, 1, ui::align::center);
                ui::text(ui::font::body, missing ? "Needs a cart/emulator with a clock" :
                                                   "Set it with your flashcart's tools",
                         120, 92, ui::secondary, ui::no_shadow, 1, ui::align::center);
                return;
            }

            if(_accounts.count == 0)
            {
                ui::text(ui::font::body, "No accounts yet", 120, 78, ui::ink, ui::light_shadow, 1, ui::align::center);
                ui::text(ui::font::body, "START: add an account", 120, 92, ui::secondary, ui::no_shadow, 1,
                         ui::align::center);
                return;
            }

            int y = rows_top;

            for(int i = 0; i < visible_count; ++i)
            {
                int position = first_visible + i;
                draw_row(position, y);
                y += row_h(position) + row_gap;
            }

            if(scrolling())
            {
                ui::round_fill(scrollbar_x, rows_top, 4, scrollbar_height, 2, ui::light_shadow);

                int thumb_height = scrollbar_height * visible_count / _accounts.count;

                if(thumb_height < 8)
                {
                    thumb_height = 8;
                }

                int hidden = _accounts.count - visible_count;
                int thumb_y = rows_top + (hidden > 0 ? (scrollbar_height - thumb_height) * first_visible / hidden : 0);
                ui::round_fill(scrollbar_x, thumb_y, 4, thumb_height, 2, ui::scroll_thumb);
            }
        }

        void draw_footer()
        {
            if(modal != modal_type::none)
            {
                ui::stripes(0, list_y + list_height, 240, 160 - (list_y + list_height));
                return;
            }

            ui::stripes(0, list_y + list_height, 240, footer_top - (list_y + list_height));
            ui::footer_bar(footer_top);

            if(footer_message_frames > 0)
            {
                bn::string<32> message = "List full: ";
                append_padded(message, max_accounts, 1);
                message += " accounts";
                ui::footer_message(footer_text_y, message);
            }
            else if(moving)
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

        const popup_option* menu_options() const
        {
            return _accounts.custom_order ? custom_order_options : sorted_options;
        }

        int menu_count() const
        {
            return _accounts.custom_order ? 5 : 4;
        }

        void draw_menu_item(int index)
        {
            int height = 9 + menu_count() * 18 + 1;
            int box_y = command_bottom - height;
            popup_option option = menu_options()[index];
            bool danger = option == popup_delete;
            bool is_selected = index == menu_selected;

            ui::menu_item(command_x, command_width, box_y + 9 + index * 18, popup_option_names[option],
                          danger ? ui::red : ui::ink, danger ? ui::danger_shadow : ui::light_shadow, is_selected,
                          is_selected ? ui::cursor_bob(frame_counter) : 0);
        }

        void draw_modal()
        {
            if(modal == modal_type::menu)
            {
                const account& item = _accounts.items[order[_selected]];
                ui::panel(dialog_x, dialog_y, dialog_width, dialog_height, ui::white, true);
                ui::text(ui::font::body, "What to do with", dialog_x + 7, dialog_y + 9, ui::ink, ui::light_shadow);

                bn::string<32> name;
                fitted_name(name, item.label, dialog_width - 14 - 8);
                name += '?';
                ui::text(ui::font::body, name, dialog_x + 7, dialog_y + 22, ui::ink, ui::light_shadow);

                int height = 9 + menu_count() * 18 + 1;
                ui::panel(command_x, command_bottom - height, command_width, height, ui::white, true);

                for(int i = 0; i < menu_count(); ++i)
                {
                    draw_menu_item(i);
                }
            }
            else if(modal == modal_type::delete_confirm)
            {
                const account& item = _accounts.items[order[_selected]];
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

        void draw_all()
        {
            ui::background();
            ui::header("AuthBoy");
            draw_clock();
            draw_time_bar();
            draw_list();
            draw_footer();
            draw_modal();
        }

        // List + whatever sits on top of it.
        void redraw_list()
        {
            update_layout();
            draw_list();
            draw_footer();
            draw_modal();
        }

        // The selection moved from old_selected (or two rows were swapped): when the list didn't
        // scroll, only the rows between the old and new position change, so only those are
        // repainted. Rows keep their total height (one tall row either way), so nothing else moves.
        void redraw_selection_change(int old_selected)
        {
            int old_first_visible = first_visible;
            update_layout();

            int first = old_selected < _selected ? old_selected : _selected;
            int last = old_selected < _selected ? _selected : old_selected;

            if(first_visible != old_first_visible || modal != modal_type::none || first < first_visible ||
               last >= first_visible + visible_count)
            {
                draw_list();
                return;
            }

            int y = rows_top;

            for(int position = first_visible; position < first; ++position)
            {
                y += row_h(position) + row_gap;
            }

            int height = -row_gap;

            for(int position = first; position <= last; ++position)
            {
                height += row_h(position) + row_gap;
            }

            ui::fill(row_x, y, row_width(), height, ui::white);

            for(int position = first; position <= last; ++position)
            {
                draw_row(position, y);
                y += row_h(position) + row_gap;
            }
        }
    };
}

code_scene_action run_code_scene(account_list& accounts, int& selected)
{
    home_view view(accounts, selected);
    int up_frames = 0;
    int down_frames = 0;
    int64_t last_second = -1;
    int64_t last_period = -1;

    view.rtc = read_rtc();
    view.update_layout();
    view.draw_all();

    if(view.rtc.status == rtc_status::ok)
    {
        last_second = view.utc();
        last_period = last_second / totp_period;
    }

    // Let one frame pass so the button that closed the previous screen isn't seen again here.
    frame_update();

    while(true)
    {
        bool list_changed = false;      // list, footer and modal
        bool list_only = false;         // list (+ modal on top)
        bool open_modal = false;        // hide the footer and draw the modal over the list
        int moved_from = -1;            // selection moved / rows swapped: repaint just those rows
        rtc_status old_status = view.rtc.status;
        view.rtc = read_rtc();

        if(view.rtc.status != old_status)
        {
            view.draw_all();
        }

        // Input
        if(view.modal == modal_type::delete_confirm)
        {
            if(bn::keypad::a_pressed())
            {
                remove_account(accounts, view.order[selected]);
                save_accounts(accounts);
                view.invalidate_codes();
                view.modal = modal_type::none;
                list_changed = true;
            }
            else if(bn::keypad::b_pressed())
            {
                view.modal = modal_type::none;
                list_changed = true;
            }
        }
        else if(view.modal == modal_type::menu)
        {
            int old_menu_selected = view.menu_selected;
            int count = view.menu_count();

            if(bn::keypad::up_pressed())
            {
                view.menu_selected = view.menu_selected == 0 ? count - 1 : view.menu_selected - 1;
            }

            if(bn::keypad::down_pressed())
            {
                view.menu_selected = view.menu_selected == count - 1 ? 0 : view.menu_selected + 1;
            }

            if(view.menu_selected != old_menu_selected)
            {
                view.frame_counter = 0;
                view.draw_menu_item(old_menu_selected);
                view.draw_menu_item(view.menu_selected);
            }

            if(bn::keypad::b_pressed())
            {
                view.modal = modal_type::none;
                list_changed = true;
            }
            else if(bn::keypad::a_pressed())
            {
                switch(view.menu_options()[view.menu_selected])
                {

                case popup_rename:
                    return code_scene_action::rename_account;

                case popup_change_key:
                    return code_scene_action::change_secret;

                case popup_move:
                    view.modal = modal_type::none;
                    view.moving = true;
                    break;

                case popup_delete:
                    view.modal = modal_type::delete_confirm;
                    break;

                default:
                    view.modal = modal_type::none;
                    break;
                }

                list_changed = true;
            }
        }
        else if(view.moving)
        {
            // Custom order only: the display order is the stored order.
            if(repeat(bn::keypad::up_held(), up_frames) && selected > 0)
            {
                swap_accounts(accounts, selected, selected - 1);
                moved_from = moved_from < 0 ? selected : moved_from;
                --selected;
                view.invalidate_codes();
            }

            if(repeat(bn::keypad::down_held(), down_frames) && selected < accounts.count - 1)
            {
                swap_accounts(accounts, selected, selected + 1);
                moved_from = moved_from < 0 ? selected : moved_from;
                ++selected;
                view.invalidate_codes();
            }

            if(bn::keypad::a_pressed() || bn::keypad::b_pressed())
            {
                view.moving = false;
                save_accounts(accounts);
                list_changed = true;
            }
        }
        else
        {
            if(bn::keypad::start_pressed())
            {
                if(accounts.count < max_accounts)
                {
                    return code_scene_action::add_account;
                }

                view.footer_message_frames = 120;
                view.draw_footer();
            }

            if(bn::keypad::select_pressed())
            {
                return code_scene_action::settings_menu;
            }

            if(accounts.count > 0 && view.rtc.status == rtc_status::ok)
            {
                int old_selected = selected;

                if(repeat(bn::keypad::up_held(), up_frames))
                {
                    selected = selected == 0 ? accounts.count - 1 : selected - 1;
                }

                if(repeat(bn::keypad::down_held(), down_frames))
                {
                    selected = selected == accounts.count - 1 ? 0 : selected + 1;
                }

                // L / R: a page up / down (no wrap-around).
                int page = view.visible_count > 1 ? view.visible_count - 1 : 1;

                if(bn::keypad::l_pressed() && selected > 0)
                {
                    selected = selected > page ? selected - page : 0;
                }

                if(bn::keypad::r_pressed() && selected < accounts.count - 1)
                {
                    selected = selected + page < accounts.count - 1 ? selected + page : accounts.count - 1;
                }

                if(selected != old_selected)
                {
                    view.frame_counter = 0;
                    moved_from = old_selected;
                }

                if(bn::keypad::a_pressed())
                {
                    view.modal = modal_type::menu;
                    view.menu_selected = 0;
                    view.frame_counter = 0;
                    open_modal = true;
                }
            }
        }

        // "List full" message: shown for 2 seconds.
        if(view.footer_message_frames > 0 && --view.footer_message_frames == 0)
        {
            view.draw_footer();
        }

        // Once per second: clock and TIME bar. When the period rolls over: every code.
        if(view.rtc.status == rtc_status::ok)
        {
            int64_t now = view.utc();

            if(now != last_second)
            {
                last_second = now;
                view.draw_clock();
                view.draw_time_bar();

                if(now / totp_period != last_period)
                {
                    last_period = now / totp_period;
                    list_only = true;
                }
            }
        }

        if(list_changed)
        {
            view.redraw_list();
        }
        else
        {
            if(list_only)
            {
                view.update_layout();
                view.draw_list();

                if(view.modal != modal_type::none)
                {
                    view.draw_modal();
                }
            }
            else if(moved_from >= 0)
            {
                view.redraw_selection_change(moved_from);
            }

            if(open_modal)
            {
                view.draw_footer();     // hidden while a modal is open
                view.draw_modal();
            }
        }


        // Cursor bob: 1 px right for 16 frames, then back.
        ++view.frame_counter;

        if(view.frame_counter % 16 == 0 && view.rtc.status == rtc_status::ok && accounts.count > 0)
        {
            if(view.modal == modal_type::menu)
            {
                view.draw_menu_item(view.menu_selected);
            }
            else if(view.modal == modal_type::none && ! view.moving)
            {
                view.draw_cursor(view.selected_row_y());
            }
        }

        frame_update();
    }
}

}
