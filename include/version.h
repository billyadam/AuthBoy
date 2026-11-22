#ifndef VERSION_H
#define VERSION_H

// App version (semantic versioning: MAJOR.MINOR.PATCH). Bump it with each release, add the
// changes to CHANGELOG.md and tag the release commit "v" + this (e.g. v1.0.0).
// The save format has its own version (account_store.cpp) and changes independently.

namespace app
{
    constexpr const char* app_version = "1.0.0";
    constexpr const char* app_version_label = "v1.0.0";     // as shown in Settings
}

#endif
