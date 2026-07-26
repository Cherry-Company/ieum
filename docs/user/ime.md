<!-- SPDX-FileCopyrightText: (C) 2026 Ieum Developers -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

# Korean IME and Input Source Synchronization

Ieum protocol 1.9 treats the Korean/English switch as an input-source command,
not as a character key. The computer receiving input is the source of truth.

## Recommended setup

1. Run Ieum 1.9 on both computers. Mixed connections remain compatible, but
   IME control is disabled for peers using protocol 1.8 or earlier.
2. On macOS, grant Accessibility and Input Monitoring permission to Ieum.
3. Add a selectable Korean input source and an ASCII-capable Latin layout in
   macOS Keyboard settings.
4. Keep **Synchronize Korean/English input state** enabled in
   `Preferences > Advanced > Input language`. This one setting is read by both
   roles, so enable it on the computer sending input as well.
5. Leave **CJK raw scan codes** on **Automatic**. This bypasses layout
   translation only while the receiving computer reports an active IME *and*
   the sending computer supplies canonical scan codes.

## Screen-entry policies

- **Keep remote state** queries and preserves the receiving computer's state.
- **Force English** selects an ASCII-capable input source on entry.
- **Follow this computer** mirrors the primary computer's Korean/English state
  when entering a remote screen.

The tray notification shows `한` for an input method and `A` for a key layout.
Changing the input source locally on the secondary is reported back to the
primary through `CILS`.

## Compatibility notes

- Mixed connections negotiate down to the lower protocol version. Against a
  peer below 1.9 neither side sends IME traffic at all: the primary skips
  `DILC` and the secondary skips `CILS`, so the link behaves exactly like
  stock Deskflow.
- Raw scan codes require a primary that reports canonical PC Set-1 codes,
  which today means Windows and macOS. On an X11 or libei primary the keys
  stay on the translated path regardless of this setting, because evdev
  keycodes are not Set-1 codes and injecting them as such would type the
  wrong characters.
- The `CILS` composition flag is reserved and always reports `0`. Neither
  Windows nor macOS lets one process observe the preedit state of another,
  so Ieum reports the input source only, not composition progress.
- Secure Input fields may reject synthetic keys according to macOS policy;
  input-source control itself remains available.
- The optional macOS inter-key delay is intended only for diagnosing apps that
  process rapid synthetic events incorrectly. Keep it at `0 us` normally.
- NFC normalization applies to text clipboard payloads sent from macOS. Binary
  clipboard formats are unchanged.
