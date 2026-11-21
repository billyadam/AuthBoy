// Host-side tests for the account store (src/account_store.cpp) with an in-memory SRAM:
// version 4 two-slot saves, upgrades from version 3 / 2 saves, torn-write recovery,
// encryption with 256 accounts and the A-Z display order.
// Build & run:  make -C tests

#include "account_store.h"
#include "entropy.h"
#include "vault.h"

#include "bn_sram.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

// ---- entropy.h stand-ins (deterministic, but never repeating) ----
namespace app
{
    void entropy_init() {}
    void entropy_add(const void*, int) {}
    void frame_update() {}

    void random_bytes(uint8_t* output, int size)
    {
        static uint32_t counter = 12345;

        for(int i = 0; i < size; ++i)
        {
            counter = counter * 1103515245u + 12345u;
            output[i] = uint8_t(counter >> 16);
        }
    }
}

namespace
{
    int failures = 0;
    int checks = 0;

    void check(bool ok, const char* name)
    {
        ++checks;

        if(! ok)
        {
            ++failures;
            std::printf("FAIL: %s\n", name);
        }
    }

    // Exact copies of the version 3 / 2 layouts written by earlier builds.
    struct header_v3
    {
        char magic[8];
        int32_t version;
        uint32_t flags;
        uint32_t save_counter;
        uint32_t kdf_blocks;
        uint32_t kdf_passes;
        uint8_t salt[16];
        uint8_t nonce[24];
    };

    struct payload_v3
    {
        int32_t count;
        app::account items[16];
    };

    struct block_v3
    {
        header_v3 header;
        uint8_t mac[16];
        uint32_t checksum;
        payload_v3 data;
    };

    struct block_v2
    {
        char magic[8];
        int32_t version;
        int32_t count;
        uint32_t checksum;
        app::account items[16];
    };

    static_assert(sizeof(app::account) == 58, "account layout changed");
    static_assert(sizeof(block_v3) == 1020, "version 3 layout changed");

    constexpr int slot_size = 16 * 1024;

    uint32_t fnv(uint32_t hash, const void* data, size_t size)
    {
        auto bytes = static_cast<const uint8_t*>(data);

        for(size_t i = 0; i < size; ++i)
        {
            hash ^= bytes[i];
            hash *= 16777619u;
        }

        return hash;
    }

    void erase_sram()
    {
        std::memset(bn::sram::data, 0xFF, sizeof(bn::sram::data));
    }

    app::account make_account(const char* label, uint8_t seed)
    {
        app::account item = {};
        std::snprintf(item.label, sizeof(item.label), "%s", label);
        item.secret_size = 10;

        for(int i = 0; i < 10; ++i)
        {
            item.secret[i] = uint8_t(seed + i);
        }

        return item;
    }

    bool same_account(const app::account& a, const app::account& b)
    {
        return std::strcmp(a.label, b.label) == 0 && a.secret_size == b.secret_size &&
               std::memcmp(a.secret, b.secret, a.secret_size) == 0;
    }

    // ~15 KB each: static, like on the GBA.
    app::account_list list;
    app::account_list expected;

    void fill_expected(int count)
    {
        expected = app::account_list();

        for(int i = 0; i < count; ++i)
        {
            char label[17];
            std::snprintf(label, sizeof(label), "acct%03d-%c", (i * 37) % 251, 'a' + (i % 26));
            expected.items[i] = make_account(label, uint8_t(i));
        }

        expected.count = count;
    }

    bool lists_equal(const app::account_list& a, const app::account_list& b)
    {
        if(a.count != b.count)
        {
            return false;
        }

        for(int i = 0; i < a.count; ++i)
        {
            if(! same_account(a.items[i], b.items[i]))
            {
                return false;
            }
        }

        return true;
    }

    // Header bytes of an SRAM slot.
    header_v3 slot_header(int slot)
    {
        header_v3 header;
        std::memcpy(&header, bn::sram::data + slot * slot_size, sizeof(header));
        return header;
    }

    // Version 4 slot layout: header (68 bytes), mac (16), checksum (4), slot_checksum (4), payload.
    constexpr int v4_flags_offset = 12;
    constexpr int v4_slot_checksum_offset = 68 + 16 + 4;
    constexpr int v4_payload_offset = v4_slot_checksum_offset + 4;
    constexpr int v4_payload_size = 4 + app::max_accounts * int(sizeof(app::account));

    uint32_t v4_slot_checksum(int slot)
    {
        const uint8_t* base = bn::sram::data + slot * slot_size;
        return fnv(fnv(2166136261u, base, v4_slot_checksum_offset), base + v4_payload_offset, v4_payload_size);
    }

    uint32_t v4_flags(int slot)
    {
        uint32_t flags;
        std::memcpy(&flags, bn::sram::data + slot * slot_size + v4_flags_offset, sizeof(flags));
        return flags;
    }

    // Rewrites a slot's header flags and fixes its slot checksum (as another build would write it).
    void set_v4_flags(int slot, uint32_t flags)
    {
        uint8_t* base = bn::sram::data + slot * slot_size;
        std::memcpy(base + v4_flags_offset, &flags, sizeof(flags));
        uint32_t sum = v4_slot_checksum(slot);
        std::memcpy(base + v4_slot_checksum_offset, &sum, sizeof(sum));
    }

    // Saves with a password, then reloads and unlocks into list. Returns true if it worked.
    bool reload_and_unlock(const char* password)
    {
        app::load_accounts(list);
        return app::locked() && app::unlock_accounts(password, int(std::strlen(password)), list) == app::pin_result::ok;
    }
}

int main()
{
    // ---- Blank SRAM ----
    erase_sram();
    app::load_accounts(list);
    check(! app::pin_enabled() && list.count == 0, "blank sram: no password, no accounts");

    // ---- First password: written to slot 0, then slots alternate ----
    fill_expected(3);
    list = expected;
    check(app::set_pin("Hunter22", 8, list) == app::pin_result::ok, "set password");
    check(slot_header(0).version == 4, "first save goes to slot 0");

    app::load_accounts(list);
    check(app::locked() && list.count == 0, "reload: locked, nothing decrypted yet");
    check(app::unlock_accounts("WRONG123", 8, list) == app::pin_result::wrong_pin, "wrong password rejected");
    check(app::unlock_accounts("hunter22", 8, list) == app::pin_result::ok, "case-insensitive password unlocks");
    check(lists_equal(list, expected), "accounts survive save + unlock");

    std::strcpy(list.items[0].label, "second");
    app::save_accounts(list);
    check(slot_header(1).version == 4 && slot_header(1).save_counter > slot_header(0).save_counter,
          "second save goes to slot 1 with a higher counter");

    std::strcpy(list.items[0].label, "third");
    app::save_accounts(list);
    check(slot_header(0).save_counter > slot_header(1).save_counter, "third save goes back to slot 0");

    check(reload_and_unlock("HUNTER22") && std::strcmp(list.items[0].label, "third") == 0,
          "reload picks the newest slot");

    // ---- Torn / corrupt newest slot: the previous save is used ----
    bn::sram::data[0 * slot_size + 500] ^= 0x5A;    // damage the newest (slot 0)
    check(reload_and_unlock("HUNTER22") && std::strcmp(list.items[0].label, "second") == 0,
          "corrupt newest slot: falls back to the previous save");

    std::strcpy(list.items[0].label, "after-recovery");
    app::save_accounts(list);
    check(slot_header(0).save_counter > slot_header(1).save_counter, "recovery save overwrites the damaged slot");
    check(reload_and_unlock("HUNTER22") && std::strcmp(list.items[0].label, "after-recovery") == 0,
          "save after recovery loads");

    // ---- Lock and unlock again (decryption happens in place) ----
    app::lock_accounts(list);
    check(app::locked() && list.count == 0, "lock clears the list");
    check(app::unlock_accounts("HUNTER22", 8, list) == app::pin_result::ok &&
          std::strcmp(list.items[0].label, "after-recovery") == 0, "unlock after lock");

    // ---- Theme: in the header flags, readable while locked; unknown values mean Field Guide ----
    check(list.theme == app::ui_theme::field_guide && (v4_flags(0) & 0xFF00) == 0, "theme: default Field Guide");

    list.theme = app::ui_theme::classic;
    app::save_accounts(list);
    app::load_accounts(list);
    check(app::locked() && list.theme == app::ui_theme::classic, "theme: classic known before unlocking");
    check(app::unlock_accounts("HUNTER22", 8, list) == app::pin_result::ok &&
          std::strcmp(list.items[0].label, "after-recovery") == 0, "theme: accounts unaffected");

    app::lock_accounts(list);
    check(list.theme == app::ui_theme::classic, "theme: kept when locking");
    app::unlock_accounts("HUNTER22", 8, list);

    // A theme number from a future build (sealed normally): Field Guide here, accounts still open.
    list.theme = app::ui_theme(7);
    list.custom_order = true;
    app::save_accounts(list);
    app::load_accounts(list);
    check(list.theme == app::ui_theme::field_guide && list.custom_order, "theme: unknown value -> Field Guide");
    check(app::unlock_accounts("HUNTER22", 8, list) == app::pin_result::ok &&
          std::strcmp(list.items[0].label, "after-recovery") == 0, "theme: unknown value, accounts still unlock");
    list.custom_order = false;

    // Unencrypted version 4 saves: from a build before themes (theme bits 0) and with garbage there.
    erase_sram();
    app::load_accounts(list);
    fill_expected(6);
    list = expected;
    list.theme = app::ui_theme::classic;
    app::save_accounts(list);       // no password yet: unencrypted, slot 0
    {
        uint32_t stored;
        std::memcpy(&stored, bn::sram::data + v4_slot_checksum_offset, sizeof(stored));
        check(v4_slot_checksum(0) == stored && (v4_flags(0) & 0xFF00) == 0x100,
              "theme: test helper matches the slot layout");
    }

    set_v4_flags(0, 2);             // custom order, no theme bits
    app::load_accounts(list);
    check(! app::pin_enabled() && list.theme == app::ui_theme::field_guide && list.custom_order &&
          lists_equal(list, expected), "theme: pre-theme v4 save -> Field Guide, accounts loaded");

    set_v4_flags(0, 2 | 0xAB00 | 0x40000);
    app::load_accounts(list);
    check(list.theme == app::ui_theme::field_guide && list.custom_order && lists_equal(list, expected),
          "theme: garbage theme bits -> Field Guide, accounts loaded");

    set_v4_flags(0, 1u << 8);
    app::load_accounts(list);
    check(list.theme == app::ui_theme::classic && ! list.custom_order && lists_equal(list, expected),
          "theme: classic bits read back");

    // ---- Version 3 encrypted save (current release) upgrades ----
    erase_sram();
    {
        static block_v3 old = {};
        std::memcpy(old.header.magic, "GBATOTP", 8);
        old.header.version = 3;
        old.header.flags = 1 | 2;    // encrypted, custom order
        old.header.save_counter = 41;
        old.header.kdf_blocks = 128;
        old.header.kdf_passes = 4;

        for(int i = 0; i < 16; ++i)
        {
            old.header.salt[i] = uint8_t(i * 3);
        }

        for(int i = 0; i < 24; ++i)
        {
            old.header.nonce[i] = uint8_t(i * 7 + 1);
        }

        fill_expected(16);
        old.data.count = 16;

        for(int i = 0; i < 16; ++i)
        {
            old.data.items[i] = expected.items[i];
        }

        vault::kdf_params params = { 128, 4 };
        void* work = std::aligned_alloc(8, size_t(vault::work_area_size(params)));
        uint8_t key[32];
        vault::derive_key("OLDPASS8", 8, old.header.salt, params, work, key);
        std::free(work);
        vault::seal(key, old.header.nonce, reinterpret_cast<const uint8_t*>(&old.header), sizeof(old.header),
                    reinterpret_cast<uint8_t*>(&old.data), sizeof(old.data), old.mac);
        std::memcpy(bn::sram::data, &old, sizeof(old));
    }

    uint8_t v3_bytes[sizeof(block_v3)];
    std::memcpy(v3_bytes, bn::sram::data, sizeof(v3_bytes));

    app::load_accounts(list);
    check(app::pin_enabled() && app::locked(), "v3: encrypted save detected, locked");
    check(list.custom_order, "v3: sort setting kept");
    check(app::unlock_accounts("nope1234", 8, list) == app::pin_result::wrong_pin, "v3: wrong password rejected");
    check(app::unlock_accounts("oldpass8", 8, list) == app::pin_result::ok, "v3: old password unlocks");
    check(lists_equal(list, expected), "v3: all 16 accounts loaded");

    app::save_accounts(list);
    check(std::memcmp(bn::sram::data, v3_bytes, sizeof(v3_bytes)) == 0,
          "v3: first upgrade save leaves the old save untouched");
    check(slot_header(1).version == 4 && slot_header(1).save_counter > 41, "v3: upgrade written to slot 1");
    check(reload_and_unlock("OLDPASS8") && lists_equal(list, expected) && list.custom_order,
          "v3: upgraded save loads with the same password and settings");

    app::save_accounts(list);
    check(slot_header(0).version == 4, "v3: next save replaces the old save in slot 0");
    check(reload_and_unlock("OLDPASS8") && lists_equal(list, expected), "v3: still loads after both slots are v4");

    // ---- Version 3 unencrypted and version 2 saves ----
    erase_sram();
    {
        static block_v3 old = {};
        std::memcpy(old.header.magic, "GBATOTP", 8);
        old.header.version = 3;
        fill_expected(5);
        old.data.count = 5;

        for(int i = 0; i < 5; ++i)
        {
            old.data.items[i] = expected.items[i];
        }

        old.checksum = fnv(2166136261u, &old.data, sizeof(old.data));
        std::memcpy(bn::sram::data, &old, sizeof(old));
    }

    app::load_accounts(list);
    check(! app::pin_enabled() && lists_equal(list, expected) && list.theme == app::ui_theme::field_guide,
          "v3 unencrypted: accounts loaded, setup needed, Field Guide");

    erase_sram();
    {
        static block_v2 old = {};
        std::memcpy(old.magic, "GBATOTP", 8);
        old.version = 2;
        fill_expected(4);
        old.count = 4;

        for(int i = 0; i < 4; ++i)
        {
            old.items[i] = expected.items[i];
        }

        old.checksum = fnv(fnv(2166136261u, &old.count, sizeof(old.count)), old.items, sizeof(old.items));
        std::memcpy(bn::sram::data, &old, sizeof(old));
    }

    app::load_accounts(list);
    check(! app::pin_enabled() && lists_equal(list, expected) && list.theme == app::ui_theme::field_guide,
          "v2: accounts loaded, Field Guide");
    check(app::set_pin("NEWPASS9", 8, list) == app::pin_result::ok && slot_header(1).version == 4,
          "v2: setting a password writes slot 1");
    check(reload_and_unlock("newpass9") && lists_equal(list, expected), "v2: upgraded and encrypted");

    // ---- 256 accounts ----
    erase_sram();
    app::load_accounts(list);
    fill_expected(app::max_accounts);
    list = expected;
    check(app::set_pin("BIGLIST1", 8, list) == app::pin_result::ok, "256: set password");
    check(reload_and_unlock("BIGLIST1") && lists_equal(list, expected), "256: all accounts survive");

    app::save_accounts(list);
    check(reload_and_unlock("BIGLIST1") && lists_equal(list, expected), "256: survive the other slot too");

    // ---- Both slots damaged: nothing usable is loaded ----
    bn::sram::data[0 * slot_size + 100] ^= 1;
    bn::sram::data[1 * slot_size + 100] ^= 1;
    app::load_accounts(list);
    check(! app::pin_enabled() && list.count == 0, "both slots damaged: treated as no save");

    // ---- A-Z display order (case-insensitive, stable, cached) ----
    {
        list = app::account_list();
        const char* labels[] = { "github", "Amazon", "discord", "GitHub", "apple", "Zoom", "aws", "Bank" };
        int count = int(sizeof(labels) / sizeof(labels[0]));

        for(int i = 0; i < count; ++i)
        {
            list.items[i] = make_account(labels[i], uint8_t(i));
        }

        list.count = count;

        int order[app::max_accounts];
        app::display_order(list, order);
        const char* sorted[] = { "Amazon", "apple", "aws", "Bank", "discord", "github", "GitHub", "Zoom" };
        bool ok = true;

        for(int i = 0; i < count; ++i)
        {
            ok = ok && std::strcmp(list.items[order[i]].label, sorted[i]) == 0;
        }

        check(ok, "a-z: case-insensitive, ties keep stored order");

        std::strcpy(list.items[5].label, "Aardvark");    // was Zoom
        app::display_order(list, order);
        check(std::strcmp(list.items[order[0]].label, "Aardvark") == 0, "a-z: cache refreshes after a rename");
        check(app::display_position(list, 5) == 0, "a-z: display_position follows the order");

        list.custom_order = true;
        app::display_order(list, order);
        check(order[0] == 0 && order[7] == 7, "custom order: stored order");

        list.custom_order = false;
        fill_expected(app::max_accounts);
        list = expected;
        app::display_order(list, order);
        bool sorted_ok = true;

        for(int i = 1; i < list.count; ++i)
        {
            sorted_ok = sorted_ok && strcasecmp(list.items[order[i - 1]].label, list.items[order[i]].label) <= 0;
        }

        check(sorted_ok, "a-z: 256 accounts sorted");
    }

    std::printf("%d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
