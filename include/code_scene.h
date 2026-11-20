#ifndef CODE_SCENE_H
#define CODE_SCENE_H

#include "account_store.h"

namespace app
{
    enum class code_scene_action
    {
        add_account,
        rename_account,     // for the account at display position selected
        change_secret,      // for the account at display position selected
        settings_menu,
    };

    // Shows all accounts and their TOTP codes as a scrolling list, read from the RTC every frame.
    // selected is a display position (see display_order). Up/Down select an account; A opens a
    // popup (rename, change key, move in custom order, delete with confirmation).
    // Returns when another screen is needed.
    code_scene_action run_code_scene(account_list& accounts, int& selected);
}

#endif
