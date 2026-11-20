#include "account_store.h"

#include "bn_sram.h"
#include "bn_timer.h"
#include "bn_timers.h"
#include "bn_memory.h"

#include "entropy.h"
#include "vault.h"

namespace app
{

namespace
{
    constexpr char save_magic[8] = { 'G', 'B', 'A', 'T', 'O', 'T', 'P', 0 };

    // Version 1: early settings-only layout (ignored).
    // Version 2: plain accounts with checksum (still loaded, upgraded on next save).
    // Version 3: header + optional XChaCha20-Poly1305 encrypted payload.
    constexpr int32_t save_version = 3;
    constexpr uint32_t flag_encrypted = 1;
    constexpr uint32_t flag_custom_order = 2;   // unset = sort A-Z (the default)

    // Argon2id cost for new PINs. Stored in each save, so changing these only affects
    // PINs set afterwards. Tuned for a few seconds of unlock time on the GBA.
    constexpr vault::kdf_params default_kdf_params = { 128, 4 };

    struct payload
    {
        int32_t count;
        account items[max_accounts];
    };

    // Authenticated (not encrypted) as additional data, so it can't be swapped or edited.
    struct save_header
    {
        char magic[8];
        int32_t version;
        uint32_t flags;
        uint32_t save_counter;
        uint32_t kdf_blocks;
        uint32_t kdf_passes;
        uint8_t salt[vault::salt_size];
        uint8_t nonce[vault::nonce_size];
    };

    struct save_block
    {
        save_header header;
        uint8_t mac[vault::mac_size];   // encrypted saves
        uint32_t checksum;              // unencrypted saves
        payload data;
    };

    struct save_block_v2
    {
        char magic[8];
        int32_t version;
        int32_t count;
        uint32_t checksum;
        account items[max_accounts];
    };

    // Static: these are ~1 KB each, kept off the small GBA stack.
    save_block stored_block;    // last block read from / written to SRAM
    payload work_payload;

    struct
    {
        bool pin_enabled = false;
        bool unlocked = true;
        uint8_t key[vault::key_size] = {};
        uint8_t salt[vault::salt_size] = {};
        vault::kdf_params kdf = default_kdf_params;
        uint32_t save_counter = 0;
        int last_kdf_ms = 0;
    } state;

    uint32_t fnv1a(uint32_t hash, const void* data, int size)
    {
        const uint8_t* bytes = static_cast<const uint8_t*>(data);

        for(int i = 0; i < size; ++i)
        {
            hash ^= bytes[i];
            hash *= 16777619u;
        }

        return hash;
    }

    uint32_t checksum(const void* data, int size)
    {
        return fnv1a(2166136261u, data, size);
    }

    bool magic_matches(const char* magic)
    {
        for(int i = 0; i < 8; ++i)
        {
            if(magic[i] != save_magic[i])
            {
                return false;
            }
        }

        return true;
    }

    // Guards against bad data reaching the text renderer or the TOTP code.
    bool sanitize(account& item)
    {
        if(item.secret_size == 0 || item.secret_size > max_secret_bytes)
        {
            return false;
        }

        item.label[max_label_chars] = 0;

        for(int i = 0; i < max_label_chars && item.label[i]; ++i)
        {
            char c = item.label[i];

            if(c < 32 || c > 126)
            {
                item.label[i] = '?';
            }
        }

        return true;
    }

    void copy_into_list(int count, const account* items, account_list& accounts)
    {
        accounts.count = 0;

        if(count < 0 || count > max_accounts)
        {
            return;
        }

        for(int i = 0; i < count; ++i)
        {
            account item = items[i];

            if(sanitize(item))
            {
                accounts.items[accounts.count++] = item;
            }
        }
    }

    void wipe_list(account_list& accounts)
    {
        vault::wipe(accounts.items, sizeof(accounts.items));
        accounts.count = 0;
    }

    // Runs Argon2id with a temporary EWRAM work area and records the time taken.
    pin_result derive(const char* pin, int pin_size, const uint8_t* salt, const vault::kdf_params& params,
                      uint8_t key[vault::key_size])
    {
        int area_size = vault::work_area_size(params);
        void* raw = bn::memory::ewram_alloc(area_size + 8);

        if(! raw)
        {
            return pin_result::out_of_memory;
        }

        // Monocypher expects 8-byte alignment.
        uintptr_t aligned = (reinterpret_cast<uintptr_t>(raw) + 7) & ~uintptr_t(7);

        bn::timer timer;
        bool ok = vault::derive_key(pin, pin_size, salt, params, reinterpret_cast<void*>(aligned), key);
        state.last_kdf_ms = int(int64_t(timer.elapsed_ticks()) * 1000 / bn::timers::ticks_per_second());

        bn::memory::ewram_free(raw);
        return ok ? pin_result::ok : pin_result::wrong_pin;     // invalid PIN format
    }

    void write_block(const account_list& accounts)
    {
        save_block& block = stored_block;
        block = save_block();

        for(int i = 0; i < 8; ++i)
        {
            block.header.magic[i] = save_magic[i];
        }

        block.header.version = save_version;
        block.header.save_counter = ++state.save_counter;
        block.header.flags = accounts.custom_order ? flag_custom_order : 0;

        payload& data = block.data;
        data.count = accounts.count;

        for(int i = 0; i < accounts.count; ++i)
        {
            data.items[i] = accounts.items[i];
        }

        if(state.pin_enabled)
        {
            block.header.flags |= flag_encrypted;
            block.header.kdf_blocks = state.kdf.blocks;
            block.header.kdf_passes = state.kdf.passes;

            for(int i = 0; i < vault::salt_size; ++i)
            {
                block.header.salt[i] = state.salt[i];
            }

            entropy_add(&block.header.save_counter, sizeof(block.header.save_counter));
            random_bytes(block.header.nonce, vault::nonce_size);

            vault::seal(state.key, block.header.nonce, reinterpret_cast<const uint8_t*>(&block.header),
                        sizeof(block.header), reinterpret_cast<uint8_t*>(&data), sizeof(data), block.mac);
        }
        else
        {
            block.checksum = checksum(&data, sizeof(data));
        }

        bn::sram::write(block);
    }
}

void load_accounts(account_list& accounts)
{
    wipe_list(accounts);
    accounts.custom_order = false;
    state = {};
    bn::sram::read(stored_block);

    // Earlier saves (nonces, counters) make later outputs differ even with little input entropy.
    entropy_add(&stored_block, sizeof(stored_block));

    const save_header& header = stored_block.header;

    if(! magic_matches(header.magic))
    {
        return;
    }

    if(header.version == 2)
    {
        static save_block_v2 old_block;
        bn::sram::read(old_block);

        uint32_t expected = fnv1a(fnv1a(2166136261u, &old_block.count, sizeof(old_block.count)),
                                  old_block.items, sizeof(old_block.items));

        if(old_block.checksum == expected)
        {
            copy_into_list(old_block.count, old_block.items, accounts);
        }

        vault::wipe(&old_block, sizeof(old_block));
        return;
    }

    if(header.version != save_version)
    {
        return;
    }

    state.save_counter = header.save_counter;
    accounts.custom_order = (header.flags & flag_custom_order) != 0;

    if(header.flags & flag_encrypted)
    {
        vault::kdf_params params = { header.kdf_blocks, header.kdf_passes };

        if(vault::kdf_params_valid(params))
        {
            state.pin_enabled = true;
            state.unlocked = false;
            state.kdf = params;

            for(int i = 0; i < vault::salt_size; ++i)
            {
                state.salt[i] = header.salt[i];
            }
        }

        return;
    }

    if(stored_block.checksum == checksum(&stored_block.data, sizeof(stored_block.data)))
    {
        copy_into_list(stored_block.data.count, stored_block.data.items, accounts);
    }
}

bool pin_enabled()
{
    return state.pin_enabled;
}

bool locked()
{
    return state.pin_enabled && ! state.unlocked;
}

pin_result unlock_accounts(const char* pin, int pin_size, account_list& accounts)
{
    uint8_t key[vault::key_size];

    if(pin_result result = derive(pin, pin_size, state.salt, state.kdf, key); result != pin_result::ok)
    {
        vault::wipe(key, sizeof(key));
        return result;
    }

    work_payload = stored_block.data;

    if(! vault::open(key, stored_block.header.nonce, reinterpret_cast<const uint8_t*>(&stored_block.header),
                     sizeof(stored_block.header), reinterpret_cast<uint8_t*>(&work_payload),
                     sizeof(work_payload), stored_block.mac))
    {
        vault::wipe(key, sizeof(key));
        vault::wipe(&work_payload, sizeof(work_payload));
        return pin_result::wrong_pin;
    }

    copy_into_list(work_payload.count, work_payload.items, accounts);
    vault::wipe(&work_payload, sizeof(work_payload));

    for(int i = 0; i < vault::key_size; ++i)
    {
        state.key[i] = key[i];
    }

    vault::wipe(key, sizeof(key));
    state.unlocked = true;
    return pin_result::ok;
}

pin_result verify_pin(const char* pin, int pin_size)
{
    uint8_t key[vault::key_size];

    if(pin_result result = derive(pin, pin_size, state.salt, state.kdf, key); result != pin_result::ok)
    {
        vault::wipe(key, sizeof(key));
        return result;
    }

    bool ok = vault::keys_equal(key, state.key);
    vault::wipe(key, sizeof(key));
    return ok ? pin_result::ok : pin_result::wrong_pin;
}

pin_result set_pin(const char* pin, int pin_size, const account_list& accounts)
{
    uint8_t salt[vault::salt_size];
    uint8_t key[vault::key_size];
    random_bytes(salt, vault::salt_size);

    if(pin_result result = derive(pin, pin_size, salt, default_kdf_params, key); result != pin_result::ok)
    {
        vault::wipe(key, sizeof(key));
        return result;
    }

    for(int i = 0; i < vault::key_size; ++i)
    {
        state.key[i] = key[i];
    }

    for(int i = 0; i < vault::salt_size; ++i)
    {
        state.salt[i] = salt[i];
    }

    vault::wipe(key, sizeof(key));
    state.kdf = default_kdf_params;
    state.pin_enabled = true;
    state.unlocked = true;
    write_block(accounts);
    return pin_result::ok;
}

void lock_accounts(account_list& accounts)
{
    wipe_list(accounts);
    vault::wipe(state.key, sizeof(state.key));
    state.unlocked = false;
}

void save_accounts(const account_list& accounts)
{
    if(locked())
    {
        return;
    }

    write_block(accounts);
}

int last_kdf_milliseconds()
{
    return state.last_kdf_ms;
}

namespace
{
    char lower(char c)
    {
        return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
    }

    // Case-insensitive label comparison; ties keep the stored order so sorting is stable.
    bool sorts_before(const account_list& accounts, int a, int b)
    {
        const char* x = accounts.items[a].label;
        const char* y = accounts.items[b].label;

        for(int i = 0; i < max_label_chars; ++i)
        {
            char cx = lower(x[i]);
            char cy = lower(y[i]);

            if(cx != cy)
            {
                return cx < cy;
            }

            if(! cx)
            {
                break;
            }
        }

        return a < b;
    }
}

void display_order(const account_list& accounts, int order[max_accounts])
{
    for(int i = 0; i < accounts.count; ++i)
    {
        order[i] = i;
    }

    if(accounts.custom_order)
    {
        return;
    }

    // Insertion sort: at most 16 items.
    for(int i = 1; i < accounts.count; ++i)
    {
        int value = order[i];
        int j = i - 1;

        while(j >= 0 && sorts_before(accounts, value, order[j]))
        {
            order[j + 1] = order[j];
            --j;
        }

        order[j + 1] = value;
    }
}

int display_position(const account_list& accounts, int index)
{
    int order[max_accounts];
    display_order(accounts, order);

    for(int i = 0; i < accounts.count; ++i)
    {
        if(order[i] == index)
        {
            return i;
        }
    }

    return 0;
}

void swap_accounts(account_list& accounts, int a, int b)
{
    if(a < 0 || b < 0 || a >= accounts.count || b >= accounts.count || a == b)
    {
        return;
    }

    account temp = accounts.items[a];
    accounts.items[a] = accounts.items[b];
    accounts.items[b] = temp;
    vault::wipe(&temp, sizeof(temp));
}

void remove_account(account_list& accounts, int index)
{
    if(index < 0 || index >= accounts.count)
    {
        return;
    }

    for(int i = index; i < accounts.count - 1; ++i)
    {
        accounts.items[i] = accounts.items[i + 1];
    }

    --accounts.count;
    vault::wipe(&accounts.items[accounts.count], sizeof(account));
}

}
