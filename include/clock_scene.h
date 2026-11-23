#ifndef CLOCK_SCENE_H
#define CLOCK_SCENE_H

#include "account_store.h"

namespace app
{
    // Set clock screen: the user sets the local time a little ahead of their phone, starts the
    // clock when the phone shows it, compares and confirms. Saves the zone and the RTC
    // correction (the RTC itself isn't changed).
    // first_run: part of first-time setup (B skips instead of going back).
    // Returns true if the settings were saved.
    bool run_set_clock_scene(account_list& accounts, bool first_run);
}

#endif
