#ifndef PIN_SCENES_H
#define PIN_SCENES_H

#include "account_store.h"
#include "app_text.h"

namespace app
{
    // Mandatory first-time PIN setup: welcome screen, then new PIN + confirmation until set.
    // Encrypts any existing (unencrypted) accounts.
    void run_pin_setup_scene(text_generators& text, account_list& accounts);

    // Asks for the PIN until the accounts are unlocked.
    void run_unlock_scene(text_generators& text, account_list& accounts);

    // Sort order (A-Z / custom), lock now, change PIN.
    void run_settings_menu(text_generators& text, account_list& accounts);
}

#endif
