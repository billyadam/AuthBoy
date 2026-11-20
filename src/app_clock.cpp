#include "app_clock.h"

#include "bn_date.h"
#include "bn_time.h"
#include "bn_optional.h"

#include "totp.h"

namespace app
{

rtc_reading read_rtc()
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

}
