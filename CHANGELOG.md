# Changelog

AuthBoy uses [semantic versioning](https://semver.org/): MAJOR for changes that break older
saves or remove features, MINOR for new features, PATCH for fixes and small tweaks. Each release
is a git tag (`v1.0.0`, ...). The version shown in Settings comes from `include/version.h`.

The save format has its own version number (currently 4) and changes independently.

## Unreleased

- Reject unencrypted saves without loading or overwriting them, with a startup message.
  Both SRAM slots are checked; encrypted version 3 / 4 saves remain supported. Saving before
  password setup is disabled.
- Set clock screen (Settings, and once during first-time setup): set the local date and time to
  the second and the UTC offset (UTC-12:00 to UTC+14:00, 15 minute steps). The time is frozen
  while editing and starts when A is pressed, so it can be matched to a phone. The cartridge
  RTC isn't changed: the app saves a correction and the zone. In all five themes.
- Clock settings are stored in the spare space after the save in each SRAM slot, so the save
  format stays 4: v1.0.0 still loads these saves, and saves without clock settings keep
  UTC+7 with no correction
- Settings: Set clock row (Field Guide's menu rows are 2 px closer to fit five)
- Settings shows how many accounts are stored ("34 accounts stored") instead of "Password on"
- About pages (SELECT in Settings): version, author, a QR Code of the GitHub link, security
  notes (not audited, offline password guessing, avoid emulator save states while unlocked,
  keep backups and recovery codes, forgotten passwords cannot be recovered), the algorithms
  used and the libraries with their licenses
- Classic Original: welcome and unlocking screens centered, settings list evenly spaced

## v1.0.0 (2026-11-23)

First versioned release.

- TOTP codes (RFC 6238, SHA-1, 6 digits, 30 s) from the cartridge real-time clock, fixed UTC+7
- Up to 256 accounts in a scrolling list with a shared countdown; L/R page up/down
- Add, rename, change key, move and delete accounts; sort A - Z, Z - A or in a custom order
- Show secret: re-enter the password to see the account's `otpauth://` QR Code and Base32 secret
- Mandatory password (6-16 letters/digits, case-insensitive); accounts encrypted with
  XChaCha20-Poly1305, key from Argon2id (Monocypher)
- Crash-safe save: two alternating SRAM slots (save format 4); older saves are upgraded
- Five themes, chosen in Settings: Classic Original, Field Guide (default), Net Link,
  Pocket LCD and Phosphor
- Lock Boy logo on the title and unlocking screens; it opens on the right password and shakes
  on a wrong one
- Title screen at boot ("Welcome back!")
