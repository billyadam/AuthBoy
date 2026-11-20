#ifndef APP_TEXT_H
#define APP_TEXT_H

#include "bn_string.h"
#include "bn_sprite_text_generator.h"

#include "totp.h"

namespace app
{
    // The fonts used across the app.
    struct text_generators
    {
        bn::sprite_text_generator small;    // variable width 8x16
        bn::sprite_text_generator mono;     // fixed width 8x16, for aligned fields and codes

        text_generators();
    };

    template<int MaxSize>
    void append_padded(bn::string<MaxSize>& text, int value, int width)
    {
        bn::string<12> digits = bn::to_string<12>(value);

        for(int i = digits.size(); i < width; ++i)
        {
            text += '0';
        }

        text += digits;
    }

    // "YYYY-MM-DD HH:MM:SS" (19 characters)
    template<int MaxSize>
    void append_date_time(bn::string<MaxSize>& text, const totp::civil_time& civil)
    {
        append_padded(text, civil.year, 4);
        text += '-';
        append_padded(text, civil.month, 2);
        text += '-';
        append_padded(text, civil.day, 2);
        text += ' ';
        append_padded(text, civil.hour, 2);
        text += ':';
        append_padded(text, civil.minute, 2);
        text += ':';
        append_padded(text, civil.second, 2);
    }

    // "UTC+07:00" (9 characters)
    template<int MaxSize>
    void append_utc_offset(bn::string<MaxSize>& text, int offset_minutes)
    {
        int abs_offset = offset_minutes < 0 ? -offset_minutes : offset_minutes;
        text += "UTC";
        text += offset_minutes < 0 ? '-' : '+';
        append_padded(text, abs_offset / 60, 2);
        text += ':';
        append_padded(text, abs_offset % 60, 2);
    }
}

#endif
