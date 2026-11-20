#include "account_editor.h"

#include "bn_string.h"

#include "keyboard.h"
#include "totp.h"
#include "vault.h"

namespace app
{

namespace
{
    // Asks for a non-empty name. name holds the initial text and receives the result.
    bool ask_name(const char* title, bn::string<64>& name)
    {
        const char* message = nullptr;

        while(true)
        {
            if(! run_keyboard(keyboard_layout::name, title, name, max_label_chars, message))
            {
                return false;
            }

            // Trim trailing spaces
            while(! name.empty() && name.back() == ' ')
            {
                name.pop_back();
            }

            if(! name.empty())
            {
                return true;
            }

            message = "Enter a name first";
        }
    }

    // Asks for a Base32 secret and decodes it into secret / secret_size.
    bool ask_secret(const char* title, uint8_t* secret, uint8_t& secret_size)
    {
        bn::string<64> secret_text;
        const char* message = nullptr;

        while(true)
        {
            if(! run_keyboard(keyboard_layout::base32, title, secret_text, max_secret_chars, message))
            {
                wipe_string(secret_text);
                return false;
            }

            char buffer[max_secret_chars + 1];
            int size = secret_text.size();

            for(int i = 0; i < size; ++i)
            {
                buffer[i] = secret_text[i];
            }

            buffer[size] = 0;

            // Wipes the typed secret copy when leaving this iteration.
            struct buffer_wiper
            {
                char* data;
                ~buffer_wiper() { vault::wipe(data, max_secret_chars + 1); }
            } wiper{ buffer };

            // Base32 encodes 5 bits per character: lengths 1, 3 and 6 (mod 8) can't come from
            // whole bytes, so they're almost certainly a missing or extra character.
            int remainder = size % 8;

            if(size < 16)
            {
                message = "Too short: keys are usually 16+ chars";
            }
            else if(remainder == 1 || remainder == 3 || remainder == 6)
            {
                message = "Wrong length: check for a missing char";
            }
            else
            {
                int decoded = totp::base32_decode(buffer, secret, max_secret_bytes);

                if(decoded > 0)
                {
                    secret_size = uint8_t(decoded);
                    wipe_string(secret_text);
                    return true;
                }

                message = "Invalid key";
            }
        }
    }

    void set_label(account& item, const bn::string<64>& name)
    {
        for(int i = 0; i <= max_label_chars; ++i)
        {
            item.label[i] = 0;
        }

        for(int i = 0; i < name.size() && i < max_label_chars; ++i)
        {
            item.label[i] = name[i];
        }
    }
}

bool run_add_account(account_list& accounts, int& new_index)
{
    if(accounts.count >= max_accounts)
    {
        return false;
    }

    bn::string<64> name;

    if(! ask_name("Account name", name))
    {
        return false;
    }

    account new_account = {};

    if(! ask_secret("Secret key", new_account.secret, new_account.secret_size))
    {
        return false;
    }

    set_label(new_account, name);
    accounts.items[accounts.count] = new_account;
    vault::wipe(&new_account, sizeof(new_account));
    new_index = accounts.count;
    ++accounts.count;
    save_accounts(accounts);
    return true;
}

bool run_rename_account(account_list& accounts, int index)
{
    if(index < 0 || index >= accounts.count)
    {
        return false;
    }

    bn::string<64> name = accounts.items[index].label;

    if(! ask_name("Rename", name))
    {
        return false;
    }

    set_label(accounts.items[index], name);
    save_accounts(accounts);
    return true;
}

bool run_change_secret(account_list& accounts, int index)
{
    if(index < 0 || index >= accounts.count)
    {
        return false;
    }

    uint8_t secret[max_secret_bytes] = {};
    uint8_t secret_size = 0;

    if(! ask_secret("New secret key", secret, secret_size))
    {
        return false;
    }

    account& item = accounts.items[index];
    vault::wipe(item.secret, sizeof(item.secret));

    for(int i = 0; i < secret_size; ++i)
    {
        item.secret[i] = secret[i];
    }

    item.secret_size = secret_size;
    vault::wipe(secret, sizeof(secret));
    save_accounts(accounts);
    return true;
}

}
