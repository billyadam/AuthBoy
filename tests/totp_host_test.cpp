// Host-side tests for the portable TOTP core, using the official RFC test vectors.
// Build & run:  make -C tests

#include "totp.h"

#include <cstdio>
#include <cstring>
#include <initializer_list>

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

    void to_hex(const uint8_t* data, int size, char* out)
    {
        static const char digits[] = "0123456789abcdef";

        for(int i = 0; i < size; ++i)
        {
            out[i * 2] = digits[data[i] >> 4];
            out[i * 2 + 1] = digits[data[i] & 0xF];
        }

        out[size * 2] = 0;
    }

    void check_sha1(const char* message, const char* expected)
    {
        uint8_t digest[totp::sha1_digest_size];
        char hex[totp::sha1_digest_size * 2 + 1];
        totp::sha1(reinterpret_cast<const uint8_t*>(message), std::strlen(message), digest);
        to_hex(digest, totp::sha1_digest_size, hex);
        check(std::strcmp(hex, expected) == 0, message);
    }

    void check_hmac(const uint8_t* key, size_t key_size, const char* message, const char* expected)
    {
        uint8_t mac[totp::sha1_digest_size];
        char hex[totp::sha1_digest_size * 2 + 1];
        totp::hmac_sha1(key, key_size, reinterpret_cast<const uint8_t*>(message), std::strlen(message), mac);
        to_hex(mac, totp::sha1_digest_size, hex);
        check(std::strcmp(hex, expected) == 0, message);
    }
}

int main()
{
    // FIPS 180-1 / RFC 3174 SHA-1 vectors
    check_sha1("", "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    check_sha1("abc", "a9993e364706816aba3e25717850c26c9cd0d89d");
    check_sha1("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
               "84983e441c3bd26ebaae4aa1f95129e5e54670f1");

    {
        // One million 'a' characters, fed in uneven chunks to exercise buffering.
        static uint8_t chunk[997];
        std::memset(chunk, 'a', sizeof(chunk));
        totp::sha1_context ctx;
        totp::sha1_init(ctx);
        size_t remaining = 1000000;

        while(remaining)
        {
            size_t size = remaining < sizeof(chunk) ? remaining : sizeof(chunk);
            totp::sha1_update(ctx, chunk, size);
            remaining -= size;
        }

        uint8_t digest[totp::sha1_digest_size];
        char hex[totp::sha1_digest_size * 2 + 1];
        totp::sha1_final(ctx, digest);
        to_hex(digest, totp::sha1_digest_size, hex);
        check(std::strcmp(hex, "34aa973cd4c4daa4f61eeb2bdbad27316534016f") == 0, "sha1 million a");
    }

    // RFC 2202 HMAC-SHA1 vectors
    {
        uint8_t key[80];

        std::memset(key, 0x0b, 20);
        check_hmac(key, 20, "Hi There", "b617318655057264e28bc0b6fb378c8ef146be00");

        check_hmac(reinterpret_cast<const uint8_t*>("Jefe"), 4, "what do ya want for nothing?",
                   "effcdf6ae5eb2fa2d27416d5f184df9c259a7c79");

        std::memset(key, 0xaa, 80);
        check_hmac(key, 80, "Test Using Larger Than Block-Size Key - Hash Key First",
                   "aa4ae5e15272d00e95705637ce8a3b55ed402112");
    }

    const uint8_t* rfc_secret = reinterpret_cast<const uint8_t*>("12345678901234567890");

    // RFC 4226 Appendix D HOTP vectors
    {
        const uint32_t expected[] = {
            755224, 287082, 359152, 969429, 338314, 254676, 287922, 162583, 399871, 520489
        };

        for(int i = 0; i < 10; ++i)
        {
            check(totp::hotp(rfc_secret, 20, uint64_t(i), 6) == expected[i], "rfc4226 hotp");
        }
    }

    // RFC 6238 Appendix B TOTP (SHA-1) vectors, 8 digits
    {
        struct vector { int64_t time; uint32_t code; };
        const vector vectors[] = {
            { 59, 94287082 },
            { 1111111109, 7081804 },
            { 1111111111, 14050471 },
            { 1234567890, 89005924 },
            { 2000000000, 69279037 },
            { 20000000000, 65353130 },
        };

        for(const vector& v : vectors)
        {
            check(totp::totp(rfc_secret, 20, v.time, 8) == v.code, "rfc6238 totp");
        }
    }

    // Base32 (RFC 4648)
    {
        uint8_t out[64];
        int size = totp::base32_decode("GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ", out, sizeof(out));
        check(size == 20 && std::memcmp(out, "12345678901234567890", 20) == 0, "base32 rfc secret");

        size = totp::base32_decode("jbsw y3dp ehpk 3pxp", out, sizeof(out));
        const uint8_t hello[] = { 'H', 'e', 'l', 'l', 'o', '!', 0xDE, 0xAD, 0xBE, 0xEF };
        check(size == 10 && std::memcmp(out, hello, 10) == 0, "base32 lowercase with spaces");

        size = totp::base32_decode("MZXW6YQ=", out, sizeof(out));
        check(size == 4 && std::memcmp(out, "foob", 4) == 0, "base32 padding");

        check(totp::base32_decode("ABC1", out, sizeof(out)) == -1, "base32 invalid char");
        check(totp::base32_decode("GEZDGNBVGY3TQOJQ", out, 5) == -1, "base32 overflow");
    }

    // Calendar -> Unix time
    check(totp::unix_time(1970, 1, 1, 0, 0, 0) == 0, "epoch");
    check(totp::unix_time(2000, 1, 1, 0, 0, 0) == 946684800, "y2k");
    check(totp::unix_time(2000, 2, 29, 12, 0, 0) == 951825600, "leap day 2000");
    check(totp::unix_time(2009, 2, 13, 23, 31, 30) == 1234567890, "1234567890");
    check(totp::unix_time(2033, 5, 18, 3, 33, 20) == 2000000000, "2000000000");
    check(totp::unix_time(2099, 12, 31, 23, 59, 59) == 4102444799LL, "end of RTC range");

    // Unix time -> calendar
    {
        totp::civil_time c = totp::civil_from_unix(1234567890);
        check(c.year == 2009 && c.month == 2 && c.day == 13 && c.hour == 23 && c.minute == 31 && c.second == 30,
              "civil 1234567890");

        c = totp::civil_from_unix(-1);
        check(c.year == 1969 && c.month == 12 && c.day == 31 && c.hour == 23 && c.minute == 59 && c.second == 59,
              "civil -1");

        // Round trip every day of the RTC range (2000..2099) at a few times of day.
        bool round_trip_ok = true;

        for(int64_t t = 946684800; t <= 4102444799LL; t += 86400)
        {
            for(int64_t s : { int64_t(0), int64_t(1), int64_t(43210), int64_t(86399) })
            {
                totp::civil_time civil = totp::civil_from_unix(t + s);

                if(totp::unix_time(civil) != t + s || civil.day > totp::days_in_month(civil.year, civil.month))
                {
                    round_trip_ok = false;
                }
            }
        }

        check(round_trip_ok, "civil round trip 2000..2099");
    }

    check(totp::days_in_month(2000, 2) == 29, "feb 2000");
    check(totp::days_in_month(2100, 2) == 28, "feb 2100");
    check(totp::days_in_month(2024, 2) == 29, "feb 2024");
    check(totp::days_in_month(2026, 11) == 30, "nov");
    check(totp::days_in_month(2026, 12) == 31, "dec");

    std::printf("%d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
