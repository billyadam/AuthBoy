#ifndef OTP_EXPORT_H
#define OTP_EXPORT_H

// Exporting an account to a phone app: the standard otpauth:// link and its QR Code.
// Portable (no Butano), tested on the host.

#include <cstdint>

namespace otp_export
{
    // Longest link: "otpauth://totp/" + a 16-char label (spaces become %20) + "?secret=" + 64 Base32
    // chars = 133 bytes. Version 7 (45x45) holds 154 bytes at error correction level L.
    constexpr int max_uri_size = 160;
    constexpr int qr_max_version = 7;
    constexpr int qr_buffer_size = ((qr_max_version * 4 + 17) * (qr_max_version * 4 + 17) + 7) / 8 + 1;

    // Builds "otpauth://totp/<label>?secret=<BASE32>" (no issuer; defaults SHA-1 / 6 digits / 30 s).
    // The label is percent-encoded except for A-Z a-z 0-9 - . _ ~ @.
    // Returns the length, or -1 if it doesn't fit in output_capacity (including the terminator).
    int build_uri(const char* label, const uint8_t* secret, int secret_size, char* output, int output_capacity);

    // Encodes text as a QR Code, Model 2, versions 1-7, the smallest that fits, error correction
    // level L at least (raised automatically when it still fits). temp is overwritten.
    bool make_qr(const char* text, int size, uint8_t qrcode[qr_buffer_size], uint8_t temp[qr_buffer_size]);

    // Side length in modules (21, 25, ... 45).
    int qr_size(const uint8_t qrcode[qr_buffer_size]);

    // True for a dark module.
    bool qr_module(const uint8_t qrcode[qr_buffer_size], int x, int y);
}

#endif
