// Host-side tests for the encryption layer.
//  - Monocypher primitives checked against independent official vectors
//    (RFC 7693 BLAKE2b, RFC 9106 Argon2id, draft-irtf-cfrg-xchacha XChaCha20-Poly1305).
//  - vault.cpp behaviour: PIN normalization, key derivation, tamper detection.
// Build & run:  make -C tests

#include "vault.h"

extern "C"
{
    #include "monocypher.h"
}

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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

    int hex_to_bytes(const char* hex, uint8_t* out)
    {
        int size = 0;

        for(const char* it = hex; it[0] && it[1]; it += 2)
        {
            unsigned value;
            std::sscanf(it, "%2x", &value);
            out[size++] = uint8_t(value);
        }

        return size;
    }

    bool equals_hex(const uint8_t* data, int size, const char* hex)
    {
        static uint8_t expected[512];
        int expected_size = hex_to_bytes(hex, expected);
        return expected_size == size && std::memcmp(data, expected, size_t(size)) == 0;
    }

    void* alloc_work_area(const vault::kdf_params& params)
    {
        return std::aligned_alloc(8, size_t(vault::work_area_size(params)));
    }
}

int main()
{
    // RFC 7693 Appendix A: BLAKE2b-512("abc")
    {
        uint8_t hash[64];
        crypto_blake2b(hash, 64, reinterpret_cast<const uint8_t*>("abc"), 3);
        check(equals_hex(hash, 64,
                         "ba80a53f981c4d0d6a2797b69f12f6e94c212f14685ac4b74b12bb6fdbffa2d1"
                         "7d87c5392aab792dc252d5de4533cc9518d38aa8dbf1925ab92386edd4009923"),
              "blake2b abc");
    }

    // RFC 9106 section 5.3: Argon2id test vector
    {
        uint8_t password[32], salt[16], secret[8], ad[12], tag[32];
        std::memset(password, 0x01, sizeof(password));
        std::memset(salt, 0x02, sizeof(salt));
        std::memset(secret, 0x03, sizeof(secret));
        std::memset(ad, 0x04, sizeof(ad));

        crypto_argon2_config config = { CRYPTO_ARGON2_ID, 32, 3, 4 };
        crypto_argon2_inputs inputs = { password, salt, 32, 16 };
        crypto_argon2_extras extras = { secret, ad, 8, 12 };
        void* work_area = std::aligned_alloc(8, 32 * 1024);
        crypto_argon2(tag, 32, work_area, config, inputs, extras);
        std::free(work_area);

        check(equals_hex(tag, 32, "0d640df58d78766c08c037a34a8b53c9d01ef0452d75b65eb52520e96b01e659"),
              "argon2id rfc9106");
    }

    // draft-irtf-cfrg-xchacha-03 Appendix A.3.1: AEAD_XChaCha20_Poly1305
    {
        const char* plaintext = "Ladies and Gentlemen of the class of '99: If I could offer you only one tip "
                                "for the future, sunscreen would be it.";
        size_t size = std::strlen(plaintext);
        uint8_t key[32], nonce[24], ad[12], mac[16], buffer[256];
        hex_to_bytes("808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f", key);
        hex_to_bytes("404142434445464748494a4b4c4d4e4f5051525354555657", nonce);
        hex_to_bytes("50515253c0c1c2c3c4c5c6c7", ad);
        std::memcpy(buffer, plaintext, size);

        vault::seal(key, nonce, ad, sizeof(ad), buffer, size, mac);
        check(equals_hex(buffer, int(size),
                         "bd6d179d3e83d43b9576579493c0e939572a1700252bfaccbed2902c21396cbb"
                         "731c7f1b0b4aa6440bf3a82f4eda7e39ae64c6708c54c216cb96b72e1213b452"
                         "2f8c9ba40db5d945b11b69b982c1bb9e3f3fac2bc369488f76b2383565d3fff9"
                         "21f9664c97637da9768812f615c68b13b52e"),
              "xchacha20-poly1305 ciphertext");
        check(equals_hex(mac, 16, "c0875924c1c7987947deafd8780acf49"), "xchacha20-poly1305 tag");

        check(vault::open(key, nonce, ad, sizeof(ad), buffer, size, mac) &&
              std::memcmp(buffer, plaintext, size) == 0, "xchacha20-poly1305 open");
    }

    // PIN normalization
    {
        char out[vault::max_pin_chars];
        check(vault::normalize_pin("abc123", 6, out) && std::memcmp(out, "ABC123", 6) == 0, "pin lowercase");
        check(! vault::normalize_pin("abc12", 5, out), "pin too short");
        check(! vault::normalize_pin("ABCDEFGHIJKLMNOPQ", 17, out), "pin too long");
        check(vault::normalize_pin("ABCDEFGHIJKLMNOP", 16, out), "pin max length");
        check(! vault::normalize_pin("abc 12", 6, out), "pin space");
        check(! vault::normalize_pin("abc-12", 6, out), "pin symbol");
    }

    // Key derivation and sealing (small params so the test is fast)
    {
        vault::kdf_params params = { 8, 1 };
        void* work_area = alloc_work_area(params);
        uint8_t salt[16], other_salt[16];
        std::memset(salt, 0x5A, sizeof(salt));
        std::memset(other_salt, 0xA5, sizeof(other_salt));

        uint8_t key_upper[32], key_lower[32], key_other_pin[32], key_other_salt[32];
        check(vault::derive_key("HUNTER22", 8, salt, params, work_area, key_upper), "derive");
        vault::derive_key("hunter22", 8, salt, params, work_area, key_lower);
        vault::derive_key("HUNTER23", 8, salt, params, work_area, key_other_pin);
        vault::derive_key("HUNTER22", 8, other_salt, params, work_area, key_other_salt);

        check(vault::keys_equal(key_upper, key_lower), "pin is case-insensitive");
        check(! vault::keys_equal(key_upper, key_other_pin), "different pin, different key");
        check(! vault::keys_equal(key_upper, key_other_salt), "different salt, different key");

        bool wiped = true;

        for(int i = 0; i < vault::work_area_size(params); ++i)
        {
            if(static_cast<uint8_t*>(work_area)[i])
            {
                wiped = false;
            }
        }

        check(wiped, "work area wiped");

        vault::kdf_params bad_params = { 4, 1 };
        check(! vault::derive_key("HUNTER22", 8, salt, bad_params, work_area, key_upper), "bad params rejected");
        std::free(work_area);

        uint8_t nonce[24], ad[8] = { 1, 2, 3, 4, 5, 6, 7, 8 }, mac[16];
        std::memset(nonce, 0x11, sizeof(nonce));
        uint8_t data[100], original[100];

        for(int i = 0; i < 100; ++i)
        {
            data[i] = original[i] = uint8_t(i * 7);
        }

        vault::seal(key_upper, nonce, ad, sizeof(ad), data, sizeof(data), mac);
        check(std::memcmp(data, original, sizeof(data)) != 0, "sealed data differs");

        uint8_t copy[100];
        std::memcpy(copy, data, sizeof(data));
        check(! vault::open(key_other_pin, nonce, ad, sizeof(ad), copy, sizeof(copy), mac), "wrong pin fails");
        check(std::memcmp(copy, data, sizeof(data)) == 0, "failed open leaves data");

        uint8_t bad_ad[8] = { 1, 2, 3, 4, 5, 6, 7, 9 };
        check(! vault::open(key_upper, nonce, bad_ad, sizeof(bad_ad), copy, sizeof(copy), mac), "tampered ad fails");

        copy[50] ^= 1;
        check(! vault::open(key_upper, nonce, ad, sizeof(ad), copy, sizeof(copy), mac), "tampered data fails");
        copy[50] ^= 1;

        uint8_t bad_mac[16];
        std::memcpy(bad_mac, mac, sizeof(mac));
        bad_mac[0] ^= 1;
        check(! vault::open(key_upper, nonce, ad, sizeof(ad), copy, sizeof(copy), bad_mac), "tampered mac fails");

        check(vault::open(key_upper, nonce, ad, sizeof(ad), copy, sizeof(copy), mac) &&
              std::memcmp(copy, original, sizeof(copy)) == 0, "correct pin opens");
    }

    // Host timing of the GBA settings, for reference only (the GBA is far slower).
    {
        vault::kdf_params params = { 128, 4 };
        void* work_area = alloc_work_area(params);
        uint8_t salt[16] = {}, key[32];
        auto start = std::chrono::steady_clock::now();
        vault::derive_key("HUNTER22", 8, salt, params, work_area, key);
        auto end = std::chrono::steady_clock::now();
        std::free(work_area);
        std::printf("host argon2id 128 KiB x4: %.2f ms\n",
                    std::chrono::duration<double, std::milli>(end - start).count());
    }

    std::printf("%d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
