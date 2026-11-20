#ifndef TOTP_H
#define TOTP_H

// Portable TOTP core (RFC 6238 / RFC 4226 / RFC 2104 / FIPS 180-1).
// No Butano dependencies: the same code runs on the GBA and in the host tests.

#include <cstddef>
#include <cstdint>

namespace totp
{
    constexpr int sha1_digest_size = 20;
    constexpr int sha1_block_size = 64;

    struct sha1_context
    {
        uint32_t state[5];
        uint64_t total_bytes;
        uint8_t buffer[sha1_block_size];
        int buffer_size;
    };

    void sha1_init(sha1_context& ctx);
    void sha1_update(sha1_context& ctx, const uint8_t* data, size_t size);
    void sha1_final(sha1_context& ctx, uint8_t digest[sha1_digest_size]);
    void sha1(const uint8_t* data, size_t size, uint8_t digest[sha1_digest_size]);

    void hmac_sha1(const uint8_t* key, size_t key_size, const uint8_t* message, size_t message_size,
                   uint8_t mac[sha1_digest_size]);

    // Decodes RFC 4648 Base32. Case-insensitive; spaces, dashes and '=' padding are ignored.
    // Returns the number of decoded bytes, or -1 on an invalid character or if output doesn't fit.
    int base32_decode(const char* text, uint8_t* output, int output_capacity);

    // RFC 4226. digits must be in [1..9].
    uint32_t hotp(const uint8_t* key, size_t key_size, uint64_t counter, int digits = 6);

    // RFC 6238 with SHA-1.
    uint32_t totp(const uint8_t* key, size_t key_size, int64_t unix_time, int digits = 6, int period = 30);

    // Seconds since 1970-01-01 00:00:00 for the given proleptic Gregorian date/time
    // (interpreted as UTC). month in [1..12], day in [1..31].
    int64_t unix_time(int year, int month, int day, int hour, int minute, int second);

    struct civil_time
    {
        int year;
        int month;
        int day;
        int hour;
        int minute;
        int second;
    };

    int64_t unix_time(const civil_time& civil);

    // Inverse of unix_time.
    civil_time civil_from_unix(int64_t unix_time);

    int days_in_month(int year, int month);
}

#endif
