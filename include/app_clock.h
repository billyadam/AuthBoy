#ifndef APP_CLOCK_H
#define APP_CLOCK_H

#include <cstdint>

namespace app
{
    // The cartridge RTC stores local time with no time zone; this is the zone it's assumed to be in.
    constexpr int utc_offset_minutes = 7 * 60;

    enum class rtc_status
    {
        ok,
        missing,    // no RTC chip (or the emulator's RTC is disabled)
        invalid,    // RTC present but returned garbage (stopped, dead battery, never set)
    };

    struct rtc_reading
    {
        rtc_status status;
        int64_t local_time;     // RTC date/time as seconds since 1970 (local time, not UTC)
    };

    // Reads the cartridge RTC (years 2000..2099).
    rtc_reading read_rtc();

    // RTC local time converted to UTC: what TOTP needs.
    inline int64_t utc_time(const rtc_reading& rtc)
    {
        return rtc.local_time - int64_t(utc_offset_minutes) * 60;
    }
}

#endif
