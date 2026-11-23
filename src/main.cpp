#include "bn_core.h"

#include "account_store.h"
#include "account_editor.h"
#include "ui.h"
#include "code_scene.h"
#include "entropy.h"
#include "pin_scenes.h"
#include "theme.h"

namespace
{
    // ~15 KB: kept in EWRAM (uninitialized; load_accounts resets it before use).
    BN_DATA_EWRAM_BSS app::account_list accounts;
}

int main()
{
    bn::core::init();
    app::entropy_init();
    ui::init();
    app::load_accounts(accounts);
    app::set_theme(accounts.theme);     // known before unlocking: the save header isn't encrypted
    app::set_clock_settings(accounts.clock);    // also known before unlocking
    int selected = 0;   // display position in the list

    if(app::unencrypted_save_rejected())
    {
        app::run_rejected_save_scene();
    }

    // Title screen at boot. First run gets the same screen as part of password setup instead.
    if(app::pin_enabled())
    {
        app::run_title_scene();
    }

    while(true)
    {
        // A PIN is mandatory: a fresh save goes straight to setup.
        if(! app::pin_enabled())
        {
            app::run_pin_setup_scene(accounts);
        }

        if(app::locked())
        {
            app::run_unlock_scene(accounts);
            selected = 0;
        }

        app::code_scene_action action = app::run_code_scene(accounts, selected);

        // The code scene works with display positions; editing works with stored indexes.
        int order[app::max_accounts];
        app::display_order(accounts, order);
        int index = accounts.count > 0 ? order[selected] : 0;

        switch(action)
        {

        case app::code_scene_action::add_account:
            if(app::run_add_account(accounts, index))
            {
                selected = app::display_position(accounts, index);
            }
            break;

        case app::code_scene_action::rename_account:
            app::run_rename_account(accounts, index);
            selected = app::display_position(accounts, index);    // A-Z order may have changed
            break;

        case app::code_scene_action::change_secret:
            app::run_change_secret(accounts, index);
            break;

        case app::code_scene_action::show_secret:
            app::run_show_secret(accounts.items[index]);
            break;

        case app::code_scene_action::settings_menu:
            app::run_settings_menu(accounts);

            if(accounts.count > 0)
            {
                selected = app::display_position(accounts, index);    // sort mode may have changed
            }
            break;

        default:
            break;
        }
    }
}
