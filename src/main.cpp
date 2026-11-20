#include "bn_core.h"
#include "bn_color.h"
#include "bn_bg_palettes.h"

#include "account_store.h"
#include "account_editor.h"
#include "app_text.h"
#include "code_scene.h"
#include "entropy.h"
#include "pin_scenes.h"

namespace
{
    // Static: the account list is ~1 KB, kept off the small GBA stack.
    app::account_list accounts;
}

int main()
{
    bn::core::init();
    bn::bg_palettes::set_transparent_color(bn::color(2, 3, 8));

    app::entropy_init();
    app::text_generators text;
    app::load_accounts(accounts);
    int selected = 0;   // display position in the list

    while(true)
    {
        // A PIN is mandatory: first boot (or an old unencrypted save) goes straight to setup.
        if(! app::pin_enabled())
        {
            app::run_pin_setup_scene(text, accounts);
        }

        if(app::locked())
        {
            app::run_unlock_scene(text, accounts);
            selected = 0;
        }

        app::code_scene_action action = app::run_code_scene(text, accounts, selected);

        // The code scene works with display positions; editing works with stored indexes.
        int order[app::max_accounts];
        app::display_order(accounts, order);
        int index = accounts.count > 0 ? order[selected] : 0;

        switch(action)
        {

        case app::code_scene_action::add_account:
            if(app::run_add_account(text, accounts, index))
            {
                selected = app::display_position(accounts, index);
            }
            break;

        case app::code_scene_action::rename_account:
            app::run_rename_account(text, accounts, index);
            selected = app::display_position(accounts, index);    // A-Z order may have changed
            break;

        case app::code_scene_action::change_secret:
            app::run_change_secret(text, accounts, index);
            break;

        case app::code_scene_action::settings_menu:
            app::run_settings_menu(text, accounts);

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
