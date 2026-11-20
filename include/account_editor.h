#ifndef ACCOUNT_EDITOR_H
#define ACCOUNT_EDITOR_H

#include "account_store.h"
#include "bn_string.h"

namespace app
{
    // Asks for a name and Base32 secret, then appends and saves the account.
    // On success, new_index is set to the new account's index. Returns false if cancelled.
    bool run_add_account(account_list& accounts, int& new_index);

    // Edits the name of an existing account (keyboard starts with the current name).
    bool run_rename_account(account_list& accounts, int index);

    // Replaces the secret of an existing account. The old secret is never shown.
    bool run_change_secret(account_list& accounts, int index);
}

#endif
