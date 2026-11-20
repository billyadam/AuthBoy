#ifndef VAULT_H
#define VAULT_H

// Portable PIN-based encryption (no Butano dependencies, tested on the host).
//   Key derivation: Argon2id (Monocypher), PIN normalized to upper case.
//   Encryption:     XChaCha20-Poly1305 (Monocypher crypto_aead_lock / crypto_aead_unlock).

#include <cstddef>
#include <cstdint>

namespace vault
{
    constexpr int key_size = 32;
    constexpr int salt_size = 16;
    constexpr int nonce_size = 24;
    constexpr int mac_size = 16;

    constexpr int min_pin_chars = 6;
    constexpr int max_pin_chars = 16;
    constexpr int recommended_pin_chars = 8;

    struct kdf_params
    {
        uint32_t blocks;    // memory in KiB
        uint32_t passes;
    };

    // Limits accepted from a save file, so a corrupt header can't ask for absurd memory/time.
    constexpr uint32_t min_kdf_blocks = 8;
    constexpr uint32_t max_kdf_blocks = 192;
    constexpr uint32_t min_kdf_passes = 1;
    constexpr uint32_t max_kdf_passes = 64;

    bool kdf_params_valid(const kdf_params& params);

    constexpr int work_area_size(const kdf_params& params)
    {
        return int(params.blocks) * 1024;
    }

    // Copies pin into output in upper case. Accepts only A-Z, a-z and 0-9, with
    // min_pin_chars..max_pin_chars characters. output needs max_pin_chars bytes.
    bool normalize_pin(const char* pin, int pin_size, char* output);

    // Derives the encryption key. work_area must hold work_area_size(params) bytes,
    // 8-byte aligned; it is wiped afterwards. Returns false if the PIN or params are invalid.
    bool derive_key(const char* pin, int pin_size, const uint8_t salt[salt_size], const kdf_params& params,
                    void* work_area, uint8_t key[key_size]);

    // Encrypts data in place and writes the authentication tag to mac.
    // ad (additional data) is authenticated but not encrypted.
    void seal(const uint8_t key[key_size], const uint8_t nonce[nonce_size], const uint8_t* ad, size_t ad_size,
              uint8_t* data, size_t data_size, uint8_t mac[mac_size]);

    // Decrypts data in place. Returns false (and leaves data unchanged) if the key is wrong
    // or anything was modified.
    bool open(const uint8_t key[key_size], const uint8_t nonce[nonce_size], const uint8_t* ad, size_t ad_size,
              uint8_t* data, size_t data_size, const uint8_t mac[mac_size]);

    // Constant-time comparison of two keys.
    bool keys_equal(const uint8_t a[key_size], const uint8_t b[key_size]);

    void wipe(void* data, size_t size);
}

#endif
