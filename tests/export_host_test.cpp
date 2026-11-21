// Host-side tests for exporting an account: Base32 encoding, the otpauth:// link and its QR Code.
// Also writes sample QR Codes as text (argv[1] = output directory, optional) so they can be turned
// into images and scanned with a phone.
// Build & run:  make -C tests

#include "otp_export.h"
#include "totp.h"

#include <cstdio>
#include <cstring>
#include <string>

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

    std::string encode(const char* text)
    {
        char out[128];
        totp::base32_encode(reinterpret_cast<const uint8_t*>(text), int(std::strlen(text)), out, sizeof(out));
        return out;
    }

    // A 7x7 finder pattern (dark ring, light ring, 3x3 dark center) with its top-left at (x, y).
    bool finder_at(const uint8_t* qr, int x, int y)
    {
        for(int dy = 0; dy < 7; ++dy)
        {
            for(int dx = 0; dx < 7; ++dx)
            {
                bool ring = dx == 0 || dy == 0 || dx == 6 || dy == 6;
                bool center = dx >= 2 && dx <= 4 && dy >= 2 && dy <= 4;

                if(otp_export::qr_module(qr, x + dx, y + dy) != (ring || center))
                {
                    return false;
                }
            }
        }

        return true;
    }

    void dump(const char* directory, const char* name, const uint8_t* qr)
    {
        if(! directory)
        {
            return;
        }

        std::string path = std::string(directory) + "/" + name + ".txt";
        FILE* file = std::fopen(path.c_str(), "w");

        if(! file)
        {
            return;
        }

        int size = otp_export::qr_size(qr);

        for(int y = 0; y < size; ++y)
        {
            for(int x = 0; x < size; ++x)
            {
                std::fputc(otp_export::qr_module(qr, x, y) ? '#' : '.', file);
            }

            std::fputc('\n', file);
        }

        std::fclose(file);
    }
}

int main(int argc, char** argv)
{
    const char* dump_directory = argc > 1 ? argv[1] : nullptr;

    // RFC 4648 section 10 (padding removed)
    check(encode("") == "", "base32 empty");
    check(encode("f") == "MY", "base32 f");
    check(encode("fo") == "MZXQ", "base32 fo");
    check(encode("foo") == "MZXW6", "base32 foo");
    check(encode("foob") == "MZXW6YQ", "base32 foob");
    check(encode("fooba") == "MZXW6YTB", "base32 fooba");
    check(encode("foobar") == "MZXW6YTBOI", "base32 foobar");

    {
        char out[4];
        check(totp::base32_encode(reinterpret_cast<const uint8_t*>("foo"), 3, out, sizeof(out)) == -1,
              "base32 output too small");
    }

    // Encode / decode round trip for every secret length the app accepts (1-40 bytes)
    {
        bool ok = true;

        for(int size = 1; size <= 40; ++size)
        {
            uint8_t secret[40];
            uint8_t decoded[40];
            char text[72];

            for(int i = 0; i < size; ++i)
            {
                secret[i] = uint8_t(i * 37 + size);
            }

            int length = totp::base32_encode(secret, size, text, sizeof(text));
            int decoded_size = totp::base32_decode(text, decoded, sizeof(decoded));
            ok = ok && length == (size * 8 + 4) / 5 && decoded_size == size &&
                 std::memcmp(secret, decoded, size_t(size)) == 0;
        }

        check(ok, "base32 round trip 1..40 bytes");
    }

    // otpauth:// links
    const uint8_t demo_secret[] = { 'H', 'e', 'l', 'l', 'o', '!', 0xDE, 0xAD, 0xBE, 0xEF };
    char uri[otp_export::max_uri_size];

    int length = otp_export::build_uri("GitHub", demo_secret, sizeof(demo_secret), uri, sizeof(uri));
    check(length == int(std::strlen(uri)) && std::strcmp(uri, "otpauth://totp/GitHub?secret=JBSWY3DPEHPK3PXP") == 0,
          "uri basic");

    otp_export::build_uri("My Bank", demo_secret, sizeof(demo_secret), uri, sizeof(uri));
    check(std::strcmp(uri, "otpauth://totp/My%20Bank?secret=JBSWY3DPEHPK3PXP") == 0, "uri space encoded");

    otp_export::build_uri("me@mail.com", demo_secret, sizeof(demo_secret), uri, sizeof(uri));
    check(std::strcmp(uri, "otpauth://totp/me@mail.com?secret=JBSWY3DPEHPK3PXP") == 0, "uri keeps @ . ");

    otp_export::build_uri("a-b_c.d~e", demo_secret, sizeof(demo_secret), uri, sizeof(uri));
    check(std::strcmp(uri, "otpauth://totp/a-b_c.d~e?secret=JBSWY3DPEHPK3PXP") == 0, "uri keeps - _ . ~");

    otp_export::build_uri("50% off", demo_secret, sizeof(demo_secret), uri, sizeof(uri));
    check(std::strcmp(uri, "otpauth://totp/50%25%20off?secret=JBSWY3DPEHPK3PXP") == 0, "uri encodes % and space");

    check(otp_export::build_uri("GitHub", demo_secret, sizeof(demo_secret), uri, 20) == -1, "uri too small");

    // Worst case the app allows: 16-char label of 1 letter + 15 spaces, 40-byte secret (64 chars)
    uint8_t long_secret[40];

    for(int i = 0; i < 40; ++i)
    {
        long_secret[i] = uint8_t(i * 7 + 3);
    }

    length = otp_export::build_uri("A               ", long_secret, sizeof(long_secret), uri, sizeof(uri));
    check(length == 133, "uri worst case is 133 bytes");

    // QR Codes
    uint8_t qr[otp_export::qr_buffer_size];
    uint8_t temp[otp_export::qr_buffer_size];

    // 133 bytes: version 6 (41x41) holds 134 at level L, so even the worst case is 41x41.
    check(otp_export::make_qr(uri, length, qr, temp) && otp_export::qr_size(qr) == 41, "qr worst case is 41x41");
    int worst_size = otp_export::qr_size(qr);
    check(finder_at(qr, 0, 0) && finder_at(qr, worst_size - 7, 0) && finder_at(qr, 0, worst_size - 7),
          "qr worst case finder patterns");
    dump(dump_directory, "qr_worst_case", qr);

    length = otp_export::build_uri("GitHub", demo_secret, sizeof(demo_secret), uri, sizeof(uri));
    check(otp_export::make_qr(uri, length, qr, temp), "qr demo");
    int demo_size = otp_export::qr_size(qr);
    check(demo_size == 25 || demo_size == 29, "qr demo is small (45 bytes)");
    check(finder_at(qr, 0, 0) && finder_at(qr, demo_size - 7, 0) && finder_at(qr, 0, demo_size - 7),
          "qr demo finder patterns");
    dump(dump_directory, "qr_demo_github", qr);

    // Typical: 32-char secret (20 bytes), 8-char name
    length = otp_export::build_uri("Dropbox1", long_secret, 20, uri, sizeof(uri));
    check(otp_export::make_qr(uri, length, qr, temp) && otp_export::qr_size(qr) <= 37, "qr typical fits 37x37");
    dump(dump_directory, "qr_typical", qr);

    // Too long for version 7 is rejected rather than overflowing
    {
        char huge[200];
        std::memset(huge, 'x', sizeof(huge));
        check(! otp_export::make_qr(huge, 160, qr, temp), "qr rejects data over version 7 capacity");
    }

    std::printf("%d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
