#include "vault.h"

extern "C"
{
    #include "monocypher.h"
}

namespace vault
{

bool kdf_params_valid(const kdf_params& params)
{
    return params.blocks >= min_kdf_blocks && params.blocks <= max_kdf_blocks &&
           params.passes >= min_kdf_passes && params.passes <= max_kdf_passes;
}

bool normalize_pin(const char* pin, int pin_size, char* output)
{
    if(pin_size < min_pin_chars || pin_size > max_pin_chars)
    {
        return false;
    }

    for(int i = 0; i < pin_size; ++i)
    {
        char c = pin[i];

        if(c >= 'a' && c <= 'z')
        {
            c = char(c - 'a' + 'A');
        }
        else if(! ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
        {
            wipe(output, size_t(max_pin_chars));
            return false;
        }

        output[i] = c;
    }

    return true;
}

bool derive_key(const char* pin, int pin_size, const uint8_t salt[salt_size], const kdf_params& params,
                void* work_area, uint8_t key[key_size])
{
    char normalized[max_pin_chars];

    if(! kdf_params_valid(params) || ! normalize_pin(pin, pin_size, normalized))
    {
        return false;
    }

    crypto_argon2_config config;
    config.algorithm = CRYPTO_ARGON2_ID;
    config.nb_blocks = params.blocks;
    config.nb_passes = params.passes;
    config.nb_lanes = 1;

    crypto_argon2_inputs inputs;
    inputs.pass = reinterpret_cast<const uint8_t*>(normalized);
    inputs.pass_size = uint32_t(pin_size);
    inputs.salt = salt;
    inputs.salt_size = salt_size;

    crypto_argon2(key, key_size, work_area, config, inputs, crypto_argon2_no_extras);

    wipe(normalized, sizeof(normalized));
    wipe(work_area, size_t(work_area_size(params)));
    return true;
}

void seal(const uint8_t key[key_size], const uint8_t nonce[nonce_size], const uint8_t* ad, size_t ad_size,
          uint8_t* data, size_t data_size, uint8_t mac[mac_size])
{
    crypto_aead_lock(data, mac, key, nonce, ad, ad_size, data, data_size);
}

bool open(const uint8_t key[key_size], const uint8_t nonce[nonce_size], const uint8_t* ad, size_t ad_size,
          uint8_t* data, size_t data_size, const uint8_t mac[mac_size])
{
    return crypto_aead_unlock(data, mac, key, nonce, ad, ad_size, data, data_size) == 0;
}

bool keys_equal(const uint8_t a[key_size], const uint8_t b[key_size])
{
    return crypto_verify32(a, b) == 0;
}

void wipe(void* data, size_t size)
{
    crypto_wipe(data, size);
}

}
