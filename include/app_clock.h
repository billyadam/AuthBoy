#ifndef APP_CLOCK_H
#define APP_CLOCK_H

#include <cstdint>

namespace app
{
    // The cartridge RTC can't be set from here, so the app keeps two settings (saved with the
    // accounts, see account_store.h) and applies them to every reading:
    //   local time = RTC time + correction
    //   UTC        = local time - zone
    // Saves without clock settings get the defaults: zone UTC+7, no correction (the app's
    // fixed behavior before the Set clock screen).
    constexpr int default_zone_minutes = 7 * 60;
    constexpr int min_zone_minutes = -12 * 60;
    constexpr int max_zone_minutes = 14 * 60;
    constexpr int zone_step_minutes = 15;

    struct clock_settings
    {
        int32_t correction_seconds = 0;
        int16_t zone_minutes = default_zone_minutes;

        bool operator==(const clock_settings&) const = default;
    };

    // A zone in range and in 15 minute steps.
    inline bool clock_settings_valid(const clock_settings& settings)
    {
        int zone = settings.zone_minutes;
        return zone >= min_zone_minutes && zone <= max_zone_minutes && zone % zone_step_minutes == 0;
    }

    void set_clock_settings(const clock_settings& settings);
    const clock_settings& current_clock_settings();

    enum class rtc_status
    {
        ok,
        missing,    // no RTC chip (or the emulator's RTC is disabled)
        invalid,    // RTC present but returned garbage (stopped, dead battery, never set)
    };

    struct rtc_reading
    {
        rtc_status status;
        int64_t local_time;     // seconds since 1970, local time (not UTC)
    };

    // Reads the cartridge RTC (years 2000..2099) as it is, without the correction.
    rtc_reading read_raw_rtc();

    // Reads the cartridge RTC with the correction applied: the user's local time.
    rtc_reading read_rtc();

    // The user's zone (from the current clock settings).
    inline int zone_minutes()
    {
        return current_clock_settings().zone_minutes;
    }

    // Local time (from read_rtc) converted to UTC: what TOTP needs.
    inline int64_t utc_time(const rtc_reading& rtc)
    {
        return rtc.local_time - int64_t(zone_minutes()) * 60;
    }
}

#endif
