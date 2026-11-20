#include "totp.h"

namespace totp
{

namespace
{
    constexpr uint32_t rotl(uint32_t value, int bits)
    {
        return (value << bits) | (value >> (32 - bits));
    }

    void sha1_transform(uint32_t state[5], const uint8_t block[sha1_block_size])
    {
        uint32_t w[80];

        for(int i = 0; i < 16; ++i)
        {
            w[i] = (uint32_t(block[i * 4]) << 24) | (uint32_t(block[i * 4 + 1]) << 16) |
                   (uint32_t(block[i * 4 + 2]) << 8) | uint32_t(block[i * 4 + 3]);
        }

        for(int i = 16; i < 80; ++i)
        {
            w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }

        uint32_t a = state[0];
        uint32_t b = state[1];
        uint32_t c = state[2];
        uint32_t d = state[3];
        uint32_t e = state[4];

        for(int i = 0; i < 80; ++i)
        {
            uint32_t f;
            uint32_t k;

            if(i < 20)
            {
                f = (b & c) | (~b & d);
                k = 0x5A827999;
            }
            else if(i < 40)
            {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            }
            else if(i < 60)
            {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            }
            else
            {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }

            uint32_t temp = rotl(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = rotl(b, 30);
            b = a;
            a = temp;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
    }
}

void sha1_init(sha1_context& ctx)
{
    ctx.state[0] = 0x67452301;
    ctx.state[1] = 0xEFCDAB89;
    ctx.state[2] = 0x98BADCFE;
    ctx.state[3] = 0x10325476;
    ctx.state[4] = 0xC3D2E1F0;
    ctx.total_bytes = 0;
    ctx.buffer_size = 0;
}

void sha1_update(sha1_context& ctx, const uint8_t* data, size_t size)
{
    ctx.total_bytes += size;

    for(size_t i = 0; i < size; ++i)
    {
        ctx.buffer[ctx.buffer_size++] = data[i];

        if(ctx.buffer_size == sha1_block_size)
        {
            sha1_transform(ctx.state, ctx.buffer);
            ctx.buffer_size = 0;
        }
    }
}

void sha1_final(sha1_context& ctx, uint8_t digest[sha1_digest_size])
{
    uint64_t total_bits = ctx.total_bytes * 8;

    ctx.buffer[ctx.buffer_size++] = 0x80;

    if(ctx.buffer_size > sha1_block_size - 8)
    {
        while(ctx.buffer_size < sha1_block_size)
        {
            ctx.buffer[ctx.buffer_size++] = 0;
        }

        sha1_transform(ctx.state, ctx.buffer);
        ctx.buffer_size = 0;
    }

    while(ctx.buffer_size < sha1_block_size - 8)
    {
        ctx.buffer[ctx.buffer_size++] = 0;
    }

    for(int i = 7; i >= 0; --i)
    {
        ctx.buffer[ctx.buffer_size++] = uint8_t(total_bits >> (i * 8));
    }

    sha1_transform(ctx.state, ctx.buffer);

    for(int i = 0; i < 5; ++i)
    {
        digest[i * 4] = uint8_t(ctx.state[i] >> 24);
        digest[i * 4 + 1] = uint8_t(ctx.state[i] >> 16);
        digest[i * 4 + 2] = uint8_t(ctx.state[i] >> 8);
        digest[i * 4 + 3] = uint8_t(ctx.state[i]);
    }
}

void sha1(const uint8_t* data, size_t size, uint8_t digest[sha1_digest_size])
{
    sha1_context ctx;
    sha1_init(ctx);
    sha1_update(ctx, data, size);
    sha1_final(ctx, digest);
}

void hmac_sha1(const uint8_t* key, size_t key_size, const uint8_t* message, size_t message_size,
               uint8_t mac[sha1_digest_size])
{
    uint8_t key_block[sha1_block_size] = {};

    if(key_size > size_t(sha1_block_size))
    {
        sha1(key, key_size, key_block);
    }
    else
    {
        for(size_t i = 0; i < key_size; ++i)
        {
            key_block[i] = key[i];
        }
    }

    uint8_t pad[sha1_block_size];
    uint8_t inner_digest[sha1_digest_size];
    sha1_context ctx;

    for(int i = 0; i < sha1_block_size; ++i)
    {
        pad[i] = key_block[i] ^ 0x36;
    }

    sha1_init(ctx);
    sha1_update(ctx, pad, sha1_block_size);
    sha1_update(ctx, message, message_size);
    sha1_final(ctx, inner_digest);

    for(int i = 0; i < sha1_block_size; ++i)
    {
        pad[i] = key_block[i] ^ 0x5C;
    }

    sha1_init(ctx);
    sha1_update(ctx, pad, sha1_block_size);
    sha1_update(ctx, inner_digest, sha1_digest_size);
    sha1_final(ctx, mac);
}

int base32_decode(const char* text, uint8_t* output, int output_capacity)
{
    uint32_t bit_buffer = 0;
    int bit_count = 0;
    int output_size = 0;

    for(const char* it = text; *it; ++it)
    {
        char c = *it;
        int value;

        if(c >= 'A' && c <= 'Z')
        {
            value = c - 'A';
        }
        else if(c >= 'a' && c <= 'z')
        {
            value = c - 'a';
        }
        else if(c >= '2' && c <= '7')
        {
            value = c - '2' + 26;
        }
        else if(c == ' ' || c == '-' || c == '=')
        {
            continue;
        }
        else
        {
            return -1;
        }

        bit_buffer = (bit_buffer << 5) | uint32_t(value);
        bit_count += 5;

        if(bit_count >= 8)
        {
            if(output_size >= output_capacity)
            {
                return -1;
            }

            bit_count -= 8;
            output[output_size++] = uint8_t(bit_buffer >> bit_count);
        }
    }

    return output_size;
}

uint32_t hotp(const uint8_t* key, size_t key_size, uint64_t counter, int digits)
{
    uint8_t message[8];

    for(int i = 7; i >= 0; --i)
    {
        message[i] = uint8_t(counter);
        counter >>= 8;
    }

    uint8_t mac[sha1_digest_size];
    hmac_sha1(key, key_size, message, sizeof(message), mac);

    int offset = mac[sha1_digest_size - 1] & 0x0F;
    uint32_t binary = (uint32_t(mac[offset] & 0x7F) << 24) | (uint32_t(mac[offset + 1]) << 16) |
                      (uint32_t(mac[offset + 2]) << 8) | uint32_t(mac[offset + 3]);

    uint32_t modulo = 1;

    for(int i = 0; i < digits; ++i)
    {
        modulo *= 10;
    }

    return binary % modulo;
}

uint32_t totp(const uint8_t* key, size_t key_size, int64_t unix_time, int digits, int period)
{
    if(unix_time < 0)
    {
        unix_time = 0;
    }

    return hotp(key, key_size, uint64_t(unix_time) / uint64_t(period), digits);
}

int64_t unix_time(int year, int month, int day, int hour, int minute, int second)
{
    // Howard Hinnant's days_from_civil algorithm.
    year -= month <= 2;
    int era = (year >= 0 ? year : year - 399) / 400;
    int year_of_era = year - era * 400;
    int day_of_year = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    int day_of_era = year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
    int64_t days = int64_t(era) * 146097 + day_of_era - 719468;

    return days * 86400 + hour * 3600 + minute * 60 + second;
}

int64_t unix_time(const civil_time& civil)
{
    return unix_time(civil.year, civil.month, civil.day, civil.hour, civil.minute, civil.second);
}

civil_time civil_from_unix(int64_t time)
{
    // Floor division so negative times also work.
    int64_t days = time / 86400;
    int64_t seconds_of_day = time % 86400;

    if(seconds_of_day < 0)
    {
        seconds_of_day += 86400;
        --days;
    }

    // Howard Hinnant's civil_from_days algorithm.
    days += 719468;
    int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    int day_of_era = int(days - era * 146097);
    int year_of_era = (day_of_era - day_of_era / 1460 + day_of_era / 36524 - day_of_era / 146096) / 365;
    int day_of_year = day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
    int month_index = (5 * day_of_year + 2) / 153;

    civil_time result;
    result.day = day_of_year - (153 * month_index + 2) / 5 + 1;
    result.month = month_index < 10 ? month_index + 3 : month_index - 9;
    result.year = int(year_of_era + era * 400) + (result.month <= 2);
    result.hour = int(seconds_of_day / 3600);
    result.minute = int(seconds_of_day / 60 % 60);
    result.second = int(seconds_of_day % 60);
    return result;
}

int days_in_month(int year, int month)
{
    switch(month)
    {

    case 2:
        return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28;

    case 4:
    case 6:
    case 9:
    case 11:
        return 30;

    default:
        return 31;
    }
}

}
