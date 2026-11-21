#ifndef PIN_SCENES_H
#define PIN_SCENES_H

#include "account_store.h"

namespace app
{
    // Mandatory first-time PIN setup: welcome screen, then new PIN + confirmation until set.
    // Encrypts any existing (unencrypted) accounts.
    void run_pin_setup_scene(account_list& accounts);

    // Asks for the PIN until the accounts are unlocked.
    void run_unlock_scene(account_list& accounts);

    // Asks for the password again, then shows the account's otpauth:// QR Code and its secret.
    void run_show_secret(const account& item);

    // Sort order (A-Z / custom), lock now, change PIN.
    void run_settings_menu(account_list& accounts);
}

#endif
