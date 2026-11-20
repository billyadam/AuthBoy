#include "account_store.h"

#include "bn_sram.h"
#include "bn_timer.h"
#include "bn_timers.h"
#include "bn_memory.h"

#include "entropy.h"
#include "vault.h"

#ifndef BN_DATA_EWRAM_BSS
    #define BN_DATA_EWRAM_BSS   // host tests
#endif

namespace app
{

namespace
{
    constexpr char save_magic[8] = { 'G', 'B', 'A', 'T', 'O', 'T', 'P', 0 };

    // Version 1: early settings-only layout (ignored).
    // Version 2: plain accounts (16 slots) with checksum. Still loaded, upgraded on next save.
    // Version 3: header + optional XChaCha20-Poly1305 encrypted payload, 16 slots, at offset 0.
    //            Still loaded, upgraded on next save.
    // Version 4: like version 3 with 256 slots, written alternately to two SRAM slots so an
    //            interrupted save never destroys the previous one.
    constexpr int32_t save_version = 4;
    constexpr uint32_t flag_encrypted = 1;
    constexpr uint32_t flag_custom_order = 2;   // unset = sort A-Z (the default)

    constexpr int sram_slot_size = 16 * 1024;
    constexpr int sram_slot_count = 2;

    // Argon2id cost for new passwords. Stored in each save, so changing these only affects
    // passwords set afterwards. Tuned for a few seconds of unlock time on the GBA.
    constexpr vault::kdf_params default_kdf_params = { 128, 4 };

    // Authenticated (not encrypted) as additional data, so it can't be swapped or edited.
    // save_counter also tells which SRAM slot is newer.
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

    struct payload
    {
        int32_t count;
        account items[max_accounts];
    };

    struct save_block
    {
        save_header header;
        uint8_t mac[vault::mac_size];   // encrypted saves
        uint32_t checksum;              // unencrypted saves: FNV-1a of the plaintext payload
        uint32_t slot_checksum;         // FNV-1a of everything else: detects a torn / corrupt slot
        payload data;
    };

    static_assert(sizeof(save_block) <= sram_slot_size, "Save block doesn't fit in an SRAM slot");

    // ---- Older formats (read only) ----

    constexpr int legacy_max_accounts = 16;

    struct payload_v3
    {
        int32_t count;
        account items[legacy_max_accounts];
    };

    struct save_block_v3
    {
        save_header header;
        uint8_t mac[vault::mac_size];
        uint32_t checksum;
        payload_v3 data;
    };

    struct save_block_v2
    {
        char magic[8];
        int32_t version;
        int32_t count;
        uint32_t checksum;
        account items[legacy_max_accounts];
    };

    // The current slot's block (~15 KB, in EWRAM). While locked it holds the encrypted save;
    // unlocking decrypts it in place, copies the accounts out and wipes the plaintext.
    BN_DATA_EWRAM_BSS save_block block;
    save_block_v3 legacy_block;

    struct
    {
        bool pin_enabled = false;
        bool unlocked = true;
        bool legacy = false;            // the loaded save is version 3 (in legacy_block)
        int active_slot = -1;           // SRAM slot of the loaded / last written v4 save
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

    uint32_t slot_checksum(const save_block& source)
    {
        uint32_t hash = checksum(&source.header, sizeof(source.header));
        hash = fnv1a(hash, source.mac, sizeof(source.mac));
        hash = fnv1a(hash, &source.checksum, sizeof(source.checksum));
        return fnv1a(hash, &source.data, sizeof(source.data));
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

    void copy_into_list(int count, int capacity, const account* items, account_list& accounts)
    {
        accounts.count = 0;

        if(count < 0 || count > capacity || count > max_accounts)
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

            vault::wipe(&item, sizeof(item));
        }
    }

    void wipe_list(account_list& accounts)
    {
        vault::wipe(accounts.items, sizeof(accounts.items));
        accounts.count = 0;
    }

    void read_slot(int slot)
    {
        bn::sram::read_offset(block, slot * sram_slot_size);
    }

    // A slot holds a usable version 4 save: right format and not torn or corrupted.
    bool slot_valid(const save_block& candidate)
    {
        const save_header& header = candidate.header;

        if(! magic_matches(header.magic) || header.version != save_version)
        {
            return false;
        }

        if(candidate.slot_checksum != slot_checksum(candidate))
        {
            return false;
        }

        // In an encrypted save the count is ciphertext: it's checked after decryption instead.
        if(header.flags & flag_encrypted)
        {
            return true;
        }

        return candidate.data.count >= 0 && candidate.data.count <= max_accounts;
    }

    // Takes the password settings from a header. Returns false if they're unusable.
    bool apply_encrypted_header(const save_header& header)
    {
        vault::kdf_params params = { header.kdf_blocks, header.kdf_passes };

        if(! vault::kdf_params_valid(params))
        {
            return false;
        }

        state.pin_enabled = true;
        state.unlocked = false;
        state.kdf = params;

        for(int i = 0; i < vault::salt_size; ++i)
        {
            state.salt[i] = header.salt[i];
        }

        return true;
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
        return ok ? pin_result::ok : pin_result::wrong_pin;     // invalid password format
    }

    // Writes the accounts to the SRAM slot that isn't the current one, then makes it current.
    // If this is interrupted, the previous slot is still intact and is loaded next time.
    void write_block(const account_list& accounts)
    {
        int target_slot = state.active_slot == 0 ? 1 : 0;

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

        block.slot_checksum = slot_checksum(block);
        bn::sram::write_offset(block, target_slot * sram_slot_size);

        state.active_slot = target_slot;
        state.legacy = false;
    }

    // Loads the newest valid version 4 slot into block. Returns false if there's none.
    bool load_newest_slot()
    {
        int best_slot = -1;
        uint32_t best_counter = 0;

        for(int slot = 0; slot < sram_slot_count; ++slot)
        {
            read_slot(slot);

            // Earlier saves (nonces, counters) make later outputs differ even with little input entropy.
            entropy_add(&block.header, sizeof(block.header));

            if(slot_valid(block) && (best_slot < 0 || int32_t(block.header.save_counter - best_counter) > 0))
            {
                best_slot = slot;
                best_counter = block.header.save_counter;
            }
        }

        if(best_slot < 0)
        {
            return false;
        }

        read_slot(best_slot);
        state.active_slot = best_slot;
        state.save_counter = block.header.save_counter;
        return true;
    }

    // Re-reads the encrypted save of the current slot (after the in-place decryption was wiped).
    void reload_encrypted()
    {
        if(state.legacy)
        {
            bn::sram::read(legacy_block);
        }
        else if(state.active_slot >= 0)
        {
            read_slot(state.active_slot);
        }
    }

    // Version 3 / 2 saves live at offset 0. The first version 4 save then goes to slot 1,
    // leaving the old save untouched until a later save.
    void load_legacy(account_list& accounts)
    {
        bn::sram::read(legacy_block);
        const save_header& header = legacy_block.header;

        if(! magic_matches(header.magic))
        {
            return;
        }

        if(header.version == 3)
        {
            state.active_slot = 0;
            state.save_counter = header.save_counter;
            accounts.custom_order = (header.flags & flag_custom_order) != 0;

            if(header.flags & flag_encrypted)
            {
                if(apply_encrypted_header(header))
                {
                    state.legacy = true;
                }
            }
            else if(legacy_block.checksum == checksum(&legacy_block.data, sizeof(legacy_block.data)))
            {
                copy_into_list(legacy_block.data.count, legacy_max_accounts, legacy_block.data.items, accounts);
            }

            return;
        }

        if(header.version == 2)
        {
            static save_block_v2 old_block;
            bn::sram::read(old_block);

            uint32_t expected = fnv1a(checksum(&old_block.count, sizeof(old_block.count)),
                                      old_block.items, sizeof(old_block.items));

            if(old_block.checksum == expected)
            {
                state.active_slot = 0;
                copy_into_list(old_block.count, legacy_max_accounts, old_block.items, accounts);
            }

            vault::wipe(&old_block, sizeof(old_block));
        }
    }
}

void load_accounts(account_list& accounts)
{
    wipe_list(accounts);
    accounts.custom_order = false;
    state = {};

    if(! load_newest_slot())
    {
        load_legacy(accounts);
        return;
    }

    const save_header& header = block.header;
    accounts.custom_order = (header.flags & flag_custom_order) != 0;

    if(header.flags & flag_encrypted)
    {
        apply_encrypted_header(header);
        return;
    }

    if(block.checksum == checksum(&block.data, sizeof(block.data)))
    {
        copy_into_list(block.data.count, max_accounts, block.data.items, accounts);
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

    bool opened;

    // Decrypt in place: a failed open leaves the data untouched.
    if(state.legacy)
    {
        opened = vault::open(key, legacy_block.header.nonce,
                             reinterpret_cast<const uint8_t*>(&legacy_block.header), sizeof(legacy_block.header),
                             reinterpret_cast<uint8_t*>(&legacy_block.data), sizeof(legacy_block.data),
                             legacy_block.mac);

        if(opened)
        {
            copy_into_list(legacy_block.data.count, legacy_max_accounts, legacy_block.data.items, accounts);
            vault::wipe(&legacy_block.data, sizeof(legacy_block.data));
        }
    }
    else
    {
        opened = vault::open(key, block.header.nonce, reinterpret_cast<const uint8_t*>(&block.header),
                             sizeof(block.header), reinterpret_cast<uint8_t*>(&block.data), sizeof(block.data),
                             block.mac);

        if(opened)
        {
            copy_into_list(block.data.count, max_accounts, block.data.items, accounts);
            vault::wipe(&block.data, sizeof(block.data));
        }
    }

    if(! opened)
    {
        vault::wipe(key, sizeof(key));
        return pin_result::wrong_pin;
    }

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
    reload_encrypted();
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

    // A-Z order cache: re-sorted only when the labels, their order or the count change.
    int16_t cached_order[max_accounts];
    int16_t merge_buffer[max_accounts];
    uint32_t cached_fingerprint = 0;
    bool cache_valid = false;

    uint32_t labels_fingerprint(const account_list& accounts)
    {
        uint32_t hash = checksum(&accounts.count, sizeof(accounts.count));

        for(int i = 0; i < accounts.count; ++i)
        {
            const char* label = accounts.items[i].label;
            int length = 0;

            while(length < max_label_chars && label[length])
            {
                ++length;
            }

            hash = fnv1a(hash, label, length + 1);
        }

        return hash;
    }

    // Stable bottom-up merge sort of cached_order[0..count).
    void sort_cached_order(const account_list& accounts)
    {
        int count = accounts.count;

        for(int i = 0; i < count; ++i)
        {
            cached_order[i] = int16_t(i);
        }

        int16_t* source = cached_order;
        int16_t* target = merge_buffer;

        for(int width = 1; width < count; width *= 2)
        {
            for(int start = 0; start < count; start += 2 * width)
            {
                int middle = start + width < count ? start + width : count;
                int end = start + 2 * width < count ? start + 2 * width : count;
                int left = start;
                int right = middle;
                int out = start;

                while(left < middle && right < end)
                {
                    target[out++] = sorts_before(accounts, source[right], source[left]) ? source[right++] :
                                                                                           source[left++];
                }

                while(left < middle)
                {
                    target[out++] = source[left++];
                }

                while(right < end)
                {
                    target[out++] = source[right++];
                }
            }

            int16_t* swap = source;
            source = target;
            target = swap;
        }

        if(source != cached_order)
        {
            for(int i = 0; i < count; ++i)
            {
                cached_order[i] = source[i];
            }
        }
    }
}

void display_order(const account_list& accounts, int order[max_accounts])
{
    if(accounts.custom_order)
    {
        for(int i = 0; i < accounts.count; ++i)
        {
            order[i] = i;
        }

        return;
    }

    uint32_t fingerprint = labels_fingerprint(accounts);

    if(! cache_valid || fingerprint != cached_fingerprint)
    {
        sort_cached_order(accounts);
        cached_fingerprint = fingerprint;
        cache_valid = true;
    }

    for(int i = 0; i < accounts.count; ++i)
    {
        order[i] = cached_order[i];
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
