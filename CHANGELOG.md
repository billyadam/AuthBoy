# Changelog

AuthBoy uses [semantic versioning](https://semver.org/): MAJOR for changes that break older
saves or remove features, MINOR for new features, PATCH for fixes and small tweaks. Each release
is a git tag (`v1.0.0`, ...). The version shown in Settings comes from `include/version.h`.

The save format has its own version number (currently 4) and changes independently.

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
