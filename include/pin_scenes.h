#ifndef PIN_SCENES_H
#define PIN_SCENES_H

#include "account_store.h"

namespace app
{
    // Mandatory first-time PIN setup: welcome screen, then new PIN + confirmation until set.
    // Only offered for a fresh save, never for a rejected unencrypted save.
    void run_pin_setup_scene(account_list& accounts);

    // Explains why an unencrypted save cannot be used; waits for power-off without writing SRAM.
    [[noreturn]] void run_rejected_save_scene();

    // Title screen at boot when a password is already set: "Welcome back!", waits for A / START.
    void run_title_scene();

    // Asks for the PIN until the accounts are unlocked.
    void run_unlock_scene(account_list& accounts);

    // Asks for the password again, then shows the account's otpauth:// QR Code and its secret.
    void run_show_secret(const account& item);

    // Message with up to three lines (line_2 / line_3 may be nullptr); waits for A or B.
    void show_message(const char* line_1, const char* line_2, const char* line_3, const char* a_label);

    // About pages (SELECT in Settings).
    void run_about_scene();

    // Sort order, theme, set clock, lock now, change password.
    void run_settings_menu(account_list& accounts);
}

#endif
