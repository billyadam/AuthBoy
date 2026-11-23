#include "clock_scene.h"

#include "bn_keypad.h"

#include "entropy.h"
#include "pin_scenes.h"
#include "theme.h"

namespace app
{

namespace
{
    constexpr int blink_frames = 30;
    constexpr int frames_per_second = 60;

    // One step on press, then after 20 frames every 4 frames while held.
    bool repeat(bool held, int& held_frames)
    {
        if(! held)
        {
            held_frames = 0;
            return false;
        }

        ++held_frames;
        return held_frames == 1 || (held_frames > 20 && (held_frames - 20) % 4 == 0);
    }

    int wrap(int value, int min, int max)
    {
        int range = max - min + 1;
        return min + ((value - min) % range + range) % range;
    }

    int clamp(int value, int min, int max)
    {
        return value < min ? min : value > max ? max : value;
    }

    // Changes a field by delta: the year and zone stop at their ends, the rest wrap around
    // without carrying into the next field (like setting a watch).
    void adjust(clock_editor_state& state, int field, int delta)
    {
        totp::civil_time& local = state.local;

        switch(field)
        {

        case clock_field_year:
            local.year = clamp(local.year + delta, 2000, 2099);
            break;

        case clock_field_month:
            local.month = wrap(local.month + delta, 1, 12);
            break;

        case clock_field_day:
            local.day = wrap(local.day + delta, 1, totp::days_in_month(local.year, local.month));
            break;

        case clock_field_hour:
            local.hour = wrap(local.hour + delta, 0, 23);
            break;

        case clock_field_minute:
            local.minute = wrap(local.minute + delta, 0, 59);
            break;

        case clock_field_second:
            local.second = wrap(local.second + delta, 0, 59);
            break;

        default:
            state.zone_minutes = clamp(state.zone_minutes + delta * zone_step_minutes, min_zone_minutes,
                                       max_zone_minutes);
            break;
        }

        // A shorter month (or February of another year) keeps the day valid.
        int days = totp::days_in_month(local.year, local.month);

        if(local.day > days)
        {
            local.day = days;
        }
    }

    // L / R step: 10 (1 hour for the zone, nothing for the month).
    int big_step(int field)
    {
        if(field == clock_field_month)
        {
            return 0;
        }

        return field == clock_field_zone ? 60 / zone_step_minutes : 10;
    }

    // Follows the raw RTC every frame to know where in the current RTC second we are, so the
    // correction can be rounded to the nearest second instead of always truncated.
    struct rtc_tracker
    {
        rtc_reading raw = { rtc_status::missing, 0 };
        int frames_in_second = 0;
        bool phase_known = false;       // a second change was seen

        void update()
        {
            rtc_reading now = read_raw_rtc();

            if(now.status == rtc_status::ok && raw.status == rtc_status::ok && now.local_time != raw.local_time)
            {
                frames_in_second = 0;
                phase_known = true;
            }
            else
            {
                ++frames_in_second;
            }

            raw = now;
        }

        // RTC time of this moment, rounded to the nearest second.
        int64_t rounded_now() const
        {
            bool late_in_second = phase_known && frames_in_second >= frames_per_second / 2;
            return raw.local_time + (late_in_second ? 1 : 0);
        }
    };

    void show_no_clock(rtc_status status)
    {
        const char* line_2 = status == rtc_status::missing ? "No clock (RTC) found." : "The clock (RTC) isn't working.";
        show_message("Can't set the clock", line_2, "The battery may be dead.", "OK");
    }
}

bool run_set_clock_scene(account_list& accounts, bool first_run)
{
    rtc_tracker rtc;
    rtc.update();

    if(rtc.raw.status != rtc_status::ok)
    {
        if(! first_run)
        {
            show_no_clock(rtc.raw.status);
        }

        return false;
    }

    screen_painter& painter = painters().screens;
    clock_editor_state state;
    state.local = totp::civil_from_unix(rtc.raw.local_time + accounts.clock.correction_seconds);
    state.zone_minutes = accounts.clock.zone_minutes;
    state.back_label = first_run ? "skip" : "back";

    int32_t correction = accounts.clock.correction_seconds;
    int64_t started_local = 0;     // running: the local time the clock started from
    int blink_counter = 0;
    int up_frames = 0, down_frames = 0, l_frames = 0, r_frames = 0;

    painter.set_clock(state);
    frame_update();

    while(true)
    {
        rtc.update();

        if(rtc.raw.status != rtc_status::ok)
        {
            show_no_clock(rtc.raw.status);
            return false;
        }

        bool changed = false;

        if(! state.running)
        {
            if(bn::keypad::a_pressed())
            {
                // Starts from the shown time, on this frame.
                started_local = totp::unix_time(state.local);
                correction = int32_t(started_local - rtc.rounded_now());
                state.running = true;
                state.tag_visible = true;
                blink_counter = 0;
                painter.set_clock(state);
                frame_update();
                continue;
            }

            if(bn::keypad::b_pressed())
            {
                return false;
            }

            int old_field = state.field;

            if(bn::keypad::left_pressed())
            {
                state.field = state.field == 0 ? clock_field_count - 1 : state.field - 1;
            }

            if(bn::keypad::right_pressed())
            {
                state.field = state.field == clock_field_count - 1 ? 0 : state.field + 1;
            }

            int delta = 0;

            if(repeat(bn::keypad::up_held(), up_frames))
            {
                delta += 1;
            }

            if(repeat(bn::keypad::down_held(), down_frames))
            {
                delta -= 1;
            }

            if(repeat(bn::keypad::r_held(), r_frames))
            {
                delta += big_step(state.field);
            }

            if(repeat(bn::keypad::l_held(), l_frames))
            {
                delta -= big_step(state.field);
            }

            if(delta)
            {
                adjust(state, state.field, delta);
            }

            if(delta || state.field != old_field)
            {
                // Arrows back on with every change.
                state.arrows_visible = true;
                blink_counter = 0;
                changed = true;
            }
            else if(++blink_counter % blink_frames == 0)
            {
                state.arrows_visible = ! state.arrows_visible;
                changed = true;
            }
        }
        else
        {
            if(bn::keypad::a_pressed())
            {
                accounts.clock.correction_seconds = correction;
                accounts.clock.zone_minutes = int16_t(state.zone_minutes);
                set_clock_settings(accounts.clock);
                save_accounts(accounts);
                return true;
            }

            if(bn::keypad::b_pressed())
            {
                // Set again, from the time shown now.
                state.running = false;
                state.arrows_visible = true;
                blink_counter = 0;
                painter.set_clock(state);
                frame_update();
                continue;
            }

            // Never shows less than the start time (the rounded correction can make the RTC
            // look up to half a second behind until its next tick).
            int64_t local = rtc.raw.local_time + correction;
            int64_t shown = local > started_local ? local : started_local;

            if(shown != totp::unix_time(state.local))
            {
                state.local = totp::civil_from_unix(shown);
                changed = true;
            }

            if(++blink_counter % blink_frames == 0)
            {
                state.tag_visible = ! state.tag_visible;
                changed = true;
            }
        }

        if(changed)
        {
            painter.set_clock_changed(state);
        }

        frame_update();
    }
}

}
