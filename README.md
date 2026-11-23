# AuthBoy

A TOTP authenticator (like Google Authenticator) for the Game Boy Advance, built with
[Butano](https://github.com/GValiente/butano).

- 6-digit TOTP codes (RFC 6238, SHA-1, 30 s), shown as a scrolling list with a shared countdown
- Up to 256 accounts, entered with an on-screen Base32 keyboard
- Rename, change key, delete; sort A-Z, Z-A or in a custom order
- Show secret: after re-entering the password, shows the account's `otpauth://` QR Code
  (scan it to move the account to a phone app) and its Base32 secret
- Five themes, chosen in Settings: Classic Original (the original look), Field Guide (default), Net Link,
  Pocket LCD and Phosphor
- Mandatory PIN: accounts are encrypted in save memory with XChaCha20-Poly1305,
  key derived from the PIN with Argon2id (via [Monocypher](https://monocypher.org/))

Personal project; not affiliated with Google or Nintendo.

Current version: **1.0.0** (shown in Settings). See [CHANGELOG.md](CHANGELOG.md).

## Requirements

- A cartridge or emulator with a **real-time clock (RTC)**. Some emulators only enable the
  RTC for known commercial games; you may have to force it on for this ROM.
- **Set clock** (Settings, and once during first-time setup) sets the local time and the UTC
  offset. The RTC itself isn't changed: the app stores a correction and the zone. Until it's
  used, the RTC is taken as local time in UTC+7.

## Building

1. Install [devkitPro](https://devkitpro.org/wiki/Getting_Started) with the `gba-dev` group.
2. Clone with the Butano submodule:

   ```sh
   git clone --recursive git@github.com:billyadam/AuthBoy.git
   # or, in an existing clone:
   git submodule update --init
   ```

3. Build:

   ```sh
   make -j$(nproc)
   ```

   This produces `AuthBoy.gba`.

## Tests

The TOTP and encryption code is portable C++ and is tested on the host PC against official
test vectors (RFC 4226, RFC 6238, RFC 2202, FIPS 180-1, RFC 9106, RFC 7693 and the
XChaCha20-Poly1305 draft):

```sh
make -C tests
```

## Controls

| Screen | Controls |
|---|---|
| List | Up/Down: select, L/R: page up/down, A: options (rename, change key, show secret, move, delete), START: add, SELECT: settings |
| Keyboard | D-pad: move, A: type, B: delete, START: done, L: cancel (name: SELECT case, R space) |
| Settings | Sort A-Z / Z-A / Custom, Theme (A cycles through the five), Set clock, Lock now, Change password |
| Set clock | Left/Right field, Up/Down ±1 (zone ±15 min), L/R ±10 (zone ±1 h), A start the clock when your phone shows that time, then A done / B set again |

## Security notes

- The PIN is the main defence against someone who copies the save file: they can try PINs
  offline. Use **8+ random characters** (10+ is effectively uncrackable).
- A forgotten PIN can't be recovered. Keep your accounts' backup codes.
- Save files and emulator save states made before a PIN was set contain secrets unencrypted.

## Third-party code

- `butano/`: [Butano](https://github.com/GValiente/butano) 21.10.1 (git submodule), zlib license.
  The fonts come from Butano's `common` folder.
- `third_party/monocypher/`: [Monocypher](https://monocypher.org/) 4.0.2, unmodified,
  2-clause BSD or CC0 (see `third_party/monocypher/LICENCE.md`).
- `third_party/qrcodegen/`: [QR Code generator](https://github.com/nayuki/QR-Code-generator) v1.8.0
  by Project Nayuki, unmodified, MIT (see `third_party/qrcodegen/LICENSE.md`).
