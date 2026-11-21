#include "otp_export.h"

#include "totp.h"

extern "C"
{
    #include "qrcodegen.h"
}

static_assert(otp_export::qr_buffer_size == qrcodegen_BUFFER_LEN_FOR_VERSION(otp_export::qr_max_version));

namespace otp_export
{

namespace
{
    bool keep_as_is(char c)
    {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
               c == '-' || c == '.' || c == '_' || c == '~' || c == '@';
    }

    struct writer
    {
        char* output;
        int capacity;
        int size = 0;
        bool ok = true;

        void put(char c)
        {
            if(size + 1 < capacity)
            {
                output[size++] = c;
            }
            else
            {
                ok = false;
            }
        }

        void put(const char* text)
        {
            for(const char* it = text; *it; ++it)
            {
                put(*it);
            }
        }
    };
}

int build_uri(const char* label, const uint8_t* secret, int secret_size, char* output, int output_capacity)
{
    static constexpr char hex_digits[] = "0123456789ABCDEF";

    if(output_capacity <= 0)
    {
        return -1;
    }

    writer out{ output, output_capacity };
    out.put("otpauth://totp/");

    for(const char* it = label; *it; ++it)
    {
        char c = *it;

        if(keep_as_is(c))
        {
            out.put(c);
        }
        else
        {
            out.put('%');
            out.put(hex_digits[(uint8_t(c) >> 4) & 0xF]);
            out.put(hex_digits[uint8_t(c) & 0xF]);
        }
    }

    out.put("?secret=");

    char base32[72];

    if(totp::base32_encode(secret, secret_size, base32, sizeof(base32)) < 0)
    {
        output[0] = 0;
        return -1;
    }

    out.put(base32);

    for(char& c : base32)
    {
        volatile char* p = &c;
        *p = 0;
    }

    output[out.size] = 0;
    return out.ok ? out.size : -1;
}

bool make_qr(const char* text, int size, uint8_t qrcode[qr_buffer_size], uint8_t temp[qr_buffer_size])
{
    if(size < 0 || size > qr_buffer_size)
    {
        return false;
    }

    for(int i = 0; i < size; ++i)
    {
        temp[i] = uint8_t(text[i]);
    }

    return qrcodegen_encodeBinary(temp, size_t(size), qrcode, qrcodegen_Ecc_LOW, qrcodegen_VERSION_MIN,
                                  qr_max_version, qrcodegen_Mask_AUTO, true);
}

int qr_size(const uint8_t qrcode[qr_buffer_size])
{
    return qrcodegen_getSize(qrcode);
}

bool qr_module(const uint8_t qrcode[qr_buffer_size], int x, int y)
{
    return qrcodegen_getModule(qrcode, x, y);
}

}
