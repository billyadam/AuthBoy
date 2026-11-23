#include "app_clock.h"

#include "bn_date.h"
#include "bn_time.h"
#include "bn_optional.h"

#include "totp.h"

namespace app
{

namespace
{
    clock_settings active_settings;
}

void set_clock_settings(const clock_settings& settings)
{
    active_settings = clock_settings_valid(settings) ? settings : clock_settings();
}

const clock_settings& current_clock_settings()
{
    return active_settings;
}

rtc_reading read_raw_rtc()
{
    if(! bn::time::active())
    {
        return { rtc_status::missing, 0 };
    }

    bn::optional<bn::time> time_before = bn::time::current();
    bn::optional<bn::date> date = bn::date::current();
    bn::optional<bn::time> time_after = bn::time::current();

    if(! time_before || ! date || ! time_after)
    {
        return { rtc_status::invalid, 0 };
    }

    // If midnight passed between the reads, the date may belong to the old day: read it again.
    if(time_after->hour() < time_before->hour())
    {
        date = bn::date::current();

        if(! date)
        {
            return { rtc_status::invalid, 0 };
        }
    }

    int64_t local_time = totp::unix_time(2000 + date->year(), date->month(), date->month_day(),
                                         time_after->hour(), time_after->minute(), time_after->second());
    return { rtc_status::ok, local_time };
}

rtc_reading read_rtc()
{
    rtc_reading result = read_raw_rtc();

    if(result.status == rtc_status::ok)
    {
        result.local_time += active_settings.correction_seconds;
    }

    return result;
}

}
