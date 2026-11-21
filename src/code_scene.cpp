#include "code_scene.h"

#include "bn_keypad.h"

#include "entropy.h"
#include "theme.h"

namespace app
{

namespace
{
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
}

code_scene_action run_code_scene(account_list& accounts, int& selected)
{
    home_painter& painter = painters().home;
    home_state state(accounts, selected);
    int up_frames = 0;
    int down_frames = 0;
    int64_t last_second = -1;
    int64_t last_period = -1;

    state.rtc = read_rtc();
    painter.draw_all(state);

    if(state.rtc.status == rtc_status::ok)
    {
        last_second = state.utc();
        last_period = last_second / home_state::totp_period;
    }

    // Let one frame pass so the button that closed the previous screen isn't seen again here.
    frame_update();

    while(true)
    {
        bool list_changed = false;      // list, footer and modal
        bool codes_changed = false;     // a new period: every code
        bool open_modal = false;        // hide the footer and draw the modal over the list
        int moved_from = -1;            // selection moved / rows swapped
        rtc_status old_status = state.rtc.status;
        state.rtc = read_rtc();

        if(state.rtc.status != old_status)
        {
            painter.draw_all(state);
        }

        // Input
        if(state.modal == home_modal::delete_confirm)
        {
            if(bn::keypad::a_pressed())
            {
                remove_account(accounts, state.order[selected]);
                save_accounts(accounts);
                state.invalidate_codes();
                state.modal = home_modal::none;
                list_changed = true;
            }
            else if(bn::keypad::b_pressed())
            {
                state.modal = home_modal::none;
                list_changed = true;
            }
        }
        else if(state.modal == home_modal::menu)
        {
            int old_menu_selected = state.menu_selected;
            int count = state.menu_count();

            if(bn::keypad::up_pressed())
            {
                state.menu_selected = state.menu_selected == 0 ? count - 1 : state.menu_selected - 1;
            }

            if(bn::keypad::down_pressed())
            {
                state.menu_selected = state.menu_selected == count - 1 ? 0 : state.menu_selected + 1;
            }

            if(state.menu_selected != old_menu_selected)
            {
                state.frame_counter = 0;
                painter.menu_moved(state, old_menu_selected);
            }

            if(bn::keypad::b_pressed())
            {
                state.modal = home_modal::none;
                list_changed = true;
            }
            else if(bn::keypad::a_pressed())
            {
                switch(state.menu_option(state.menu_selected))
                {

                case home_option_rename:
                    return code_scene_action::rename_account;

                case home_option_change_key:
                    return code_scene_action::change_secret;

                case home_option_show_secret:
                    return code_scene_action::show_secret;

                case home_option_move:
                    state.modal = home_modal::none;
                    state.moving = true;
                    break;

                case home_option_delete:
                    state.modal = home_modal::delete_confirm;
                    break;

                default:
                    state.modal = home_modal::none;
                    break;
                }

                list_changed = true;
            }
        }
        else if(state.moving)
        {
            // Custom order only: the display order is the stored order.
            if(repeat(bn::keypad::up_held(), up_frames) && selected > 0)
            {
                swap_accounts(accounts, selected, selected - 1);
                moved_from = moved_from < 0 ? selected : moved_from;
                --selected;
                state.invalidate_codes();
            }

            if(repeat(bn::keypad::down_held(), down_frames) && selected < accounts.count - 1)
            {
                swap_accounts(accounts, selected, selected + 1);
                moved_from = moved_from < 0 ? selected : moved_from;
                ++selected;
                state.invalidate_codes();
            }

            if(bn::keypad::a_pressed() || bn::keypad::b_pressed())
            {
                state.moving = false;
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

                state.footer_message_frames = 120;
                painter.footer_changed(state);
            }

            if(bn::keypad::select_pressed())
            {
                return code_scene_action::settings_menu;
            }

            if(accounts.count > 0 && state.rtc.status == rtc_status::ok)
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
                int page = state.visible_count > 1 ? state.visible_count - 1 : 1;

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
                    state.frame_counter = 0;
                    moved_from = old_selected;
                }

                if(bn::keypad::a_pressed())
                {
                    state.modal = home_modal::menu;
                    state.menu_selected = 0;
                    state.frame_counter = 0;
                    open_modal = true;
                }
            }
        }

        // "List full" message: shown for 2 seconds.
        if(state.footer_message_frames > 0 && --state.footer_message_frames == 0)
        {
            painter.footer_changed(state);
        }

        // Once per second: clock and countdown. When the period rolls over: every code.
        if(state.rtc.status == rtc_status::ok)
        {
            int64_t now = state.utc();

            if(now != last_second)
            {
                last_second = now;
                painter.second_passed(state);

                if(now / home_state::totp_period != last_period)
                {
                    last_period = now / home_state::totp_period;
                    codes_changed = true;
                }
            }
        }

        if(list_changed)
        {
            painter.list_changed(state);
        }
        else
        {
            if(codes_changed)
            {
                painter.codes_changed(state);
            }
            else if(moved_from >= 0)
            {
                painter.selection_moved(state, moved_from);
            }

            if(open_modal)
            {
                painter.modal_opened(state);
            }
        }

        ++state.frame_counter;
        painter.animate(state);
        frame_update();
    }
}

}
