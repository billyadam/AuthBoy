#ifndef ACCOUNT_STORE_H
#define ACCOUNT_STORE_H

#include <cstdint>

namespace app
{
    constexpr int max_accounts = 256;
    constexpr int max_label_chars = 16;
    constexpr int max_secret_chars = 64;    // Base32 characters typed by the user
    constexpr int max_secret_bytes = 40;    // 64 Base32 chars decode to 40 bytes

    struct account
    {
        char label[max_label_chars + 1];    // null-terminated, printable ASCII
        uint8_t secret_size;
        uint8_t secret[max_secret_bytes];
    };

    // The app's look. Saved in the (unencrypted) save header so the unlock screen can use it.
    enum class ui_theme : uint8_t
    {
        field_guide,    // the default
        classic,        // the original look
    };

    constexpr int ui_theme_count = 2;

    struct account_list
    {
        int count = 0;
        account items[max_accounts];    // stored in the custom order

        // false: the list is shown sorted A-Z (case-insensitive); true: shown in stored order.
        bool custom_order = false;

        // Kept when the store is locked (only the accounts are cleared).
        ui_theme theme = ui_theme::field_guide;
    };

    // Fills order[0..count) with item indexes in display order.
    void display_order(const account_list& accounts, int order[max_accounts]);

    // Display position of the item at index (inverse of display_order).
    int display_position(const account_list& accounts, int index);

    // Swaps two items in the stored (custom) order.
    void swap_accounts(account_list& accounts, int a, int b);

    // Reads SRAM. If the save is PIN-protected the store becomes locked and the list stays
    // empty until unlock_accounts succeeds. A missing or corrupt save gives an empty list.
    void load_accounts(account_list& accounts);

    bool pin_enabled();
    bool locked();

    enum class pin_result
    {
        ok,
        wrong_pin,
        out_of_memory,
    };

    // Derives the key from the PIN and decrypts the accounts into the list.
    pin_result unlock_accounts(const char* pin, int pin_size, account_list& accounts);

    // Checks the PIN against the current key (store must be unlocked).
    pin_result verify_pin(const char* pin, int pin_size);

    // Sets or changes the PIN (new random salt) and re-saves the accounts encrypted.
    pin_result set_pin(const char* pin, int pin_size, const account_list& accounts);

    // Forgets the key and clears the list from memory. The store must then be unlocked again.
    void lock_accounts(account_list& accounts);

    // Saves the list (encrypted with the current key if a PIN is set).
    void save_accounts(const account_list& accounts);

    // How long the last PIN key derivation took, in milliseconds.
    int last_kdf_milliseconds();

    void remove_account(account_list& accounts, int index);
}

#endif
