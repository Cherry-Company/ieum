<!-- SPDX-FileCopyrightText: (C) 2026 Ieum Developers -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

<p align="center">
  <img src="artwork/ieum-icon-1024.png" alt="Ieum icon" width="156">
</p>

<h1 align="center">Ieum</h1>

<p align="center"><strong>Software KVM across Windows, macOS, and Linux</strong></p>

<p align="center">
  <a href="README.md">한국어</a> · English · <a href="README.zh-CN.md">简体中文</a>
</p>

<p align="center">
  <a href="https://github.com/victoriousian/ieum/releases/tag/v0.1.0-alpha.6"><strong>Download Ieum</strong></a>
  · <a href="#support-development"><strong>Support development</strong></a>
</p>

<p align="center">
  <a href="https://github.com/victoriousian/ieum/actions/workflows/continuous-integration.yml"><img src="https://github.com/victoriousian/ieum/actions/workflows/continuous-integration.yml/badge.svg?branch=ieum%2Fmain" alt="CI"></a>
  <a href="https://github.com/victoriousian/ieum/releases"><img src="https://img.shields.io/github/v/release/victoriousian/ieum?include_prereleases&label=release" alt="Release"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-GPL--2.0--only-blue" alt="GPL-2.0-only"></a>
</p>

Ieum lets one keyboard and mouse move across Windows, macOS, and Linux computers. It goes beyond crossing a
screen edge: it connects **Korean/English mode, IME composition sessions, physical key positions, and Unicode
clipboard data** into one consistent input path across operating systems.

> The current release is `v0.1.0-alpha.6`. Automated builds and unit tests pass, but the long-running physical
> Windows/macOS input matrix and production code signing are not complete.

## Product identity and interface

The Korean UI presents **이음 (Ieum)** with a Korean/CJK input-focused description. Every other UI language
presents **Ieum** with “Software KVM across Windows, macOS, and Linux.” Executable names and settings paths
remain `Ieum` for compatibility.

The main window now uses system-palette materials, a server/client segmented control, clear functional layers,
and stable responsive dimensions. It adapts Apple's WWDC26 Liquid Glass guidance on hierarchy, standard
controls, restrained effects, resizable windows, and accessibility to Qt rather than imitating Apple pixels.
See the [interface principles](docs/design/visual-system.md) for implementation rules and official sources.

## Support development

**If Ieum is useful to you, please consider supporting its development.** Funding will be used for Windows code
signing, the Apple Developer Program and notarization, physical test hardware, relay infrastructure, and ongoing
maintenance.

The `victoriousian` GitHub Sponsors payment profile is currently being prepared. Ieum will not present an
unapproved payment link or personal bank account as an official funding channel. Once the profile is active,
one-time and recurring sponsorships will be available here and through the repository Sponsor button. Stars,
field-test results, usage reports, and actionable bug reports are valuable contributions in the meantime.

## Korean input is not just key mapping

Most software KVMs translate a physical key into a character identifier and synthesize a key on the remote
computer. That works for static keyboard layouts. Korean, Chinese, and Japanese text is instead produced by an
**IME state machine and an active composition session (preedit)**. If that state is missing, keys may arrive while
the intended text does not.

| Symptom | Structural cause |
| --- | --- |
| The Windows Korean/English key does not switch the Mac input source | The key is a mode command, not a character, but is transported as an ordinary key event |
| The server indicator and the focused remote field disagree | No channel reports the client's actual input source back to the server |
| A syllable such as `한` intermittently becomes decomposed Jamo | Source changes, mixed injection paths, or event reordering terminate the composition session |
| Korean clipboard text from macOS is decomposed on Windows | Decomposed macOS text reaches the wire without NFC normalization |

Ieum treats this as a protocol-level **input state synchronization problem**, not a missing special-key mapping.

## What Ieum changes

### 1. Input-source control channel

Protocol 1.9 adds `DILC` and `CILS`: the server requests an input-source change, and the client reports the source
that the operating system actually selected. The client state, not a server-side guess, is the source of truth.

### 2. IME-native key path

While an IME is active, Ieum can bypass character `KeyID` translation and transport the physical key as a PC
Set-1 scancode. The remote computer's native IME owns composition, without layout translation interfering with
CJK typing.

### 3. Consistent macOS event synthesis

IME input uses one persistent `CGEventSource` and FIFO path, with explicit timestamps, keyboard type, modifier
flags, and repeat state. This avoids switching between unrelated event injection layers from key to key.

### 4. Unicode clipboard normalization

UTF-8 text leaving macOS is normalized to NFC to reduce decomposed Korean text in Windows applications and
file names.

```mermaid
flowchart LR
    A["Physical keyboard"] --> B["Ieum Server"]
    B -->|"Keys and scancodes"| C["Encrypted KVM channel"]
    B -->|"DILC source request"| C
    C --> D["Ieum Client"]
    D --> E["Windows IMM32 / macOS TIS"]
    E -->|"CILS actual state"| D
    D -->|"State report"| B
```

## Project status

| Area | Status |
| --- | --- |
| Ieum brand, icon, application and installer names | Complete |
| Windows x64/ARM64 and macOS Intel/Apple Silicon packages | CI build and package checks pass |
| `DILC`/`CILS`, raw scancode, macOS event source, NFC path | Implemented with automated tests |
| Linux and Flatpak packages | Experimental |
| Ten-minute physical Windows ↔ macOS input matrix | **Pending** |
| Windows signing and Apple signing/notarization | **Pending** |
| iPadOS | Unsupported because public APIs do not provide system-wide input injection |

The physical acceptance targets are zero mismatches across 20 input-source switches, zero decomposed syllables
during ten minutes of typing per application, and less than 2 ms p95 added input latency. This alpha does not
claim those hardware results before the matrix is run.

## Download

[이음 (Ieum) v0.1.0-alpha.6 release](https://github.com/victoriousian/ieum/releases/tag/v0.1.0-alpha.6)

| Operating system | Installer |
| --- | --- |
| Apple Silicon Mac | `Ieum-0.1.0-alpha.6-macos-arm64.dmg` |
| Intel Mac | `Ieum-0.1.0-alpha.6-macos-x86_64.dmg` |
| Intel/AMD 64-bit Windows | `Ieum-0.1.0-alpha.6-win-x64.msi` |
| Intel/AMD 64-bit Windows, Korean installer UI | `Ieum-0.1.0-alpha.6-win-x64-ko-KR.msi` |
| ARM64 Windows | `Ieum-0.1.0-alpha.6-win-arm64.msi` |
| ARM64 Windows, Korean installer UI | `Ieum-0.1.0-alpha.6-win-arm64-ko-KR.msi` |

Windows portable archives and experimental Linux packages are included. Verify downloads with the accompanying
`SHA256SUMS.txt`.

The `alpha.5` Mac DMG modified the app bundle after signing and therefore had a broken code seal. Delete that
version and download `alpha.6`. The final `alpha.6` app passes strict code-signature verification, but it is not
yet signed with a Developer ID certificate or Apple-notarized. Move it to `/Applications`, try to open it once,
then use **System Settings → Privacy & Security → Open Anyway** if macOS blocks it. Accessibility and Input
Monitoring permissions are also required.

Windows `alpha.6` uses MSI version `0.1.106` so it can replace `alpha.4` and `alpha.5`. The global installer
appears as **Ieum** in Installed apps and the Start menu; the Korean installer appears as
**이음 (Ieum)**. The installation directory is `C:\Program Files\Ieum`, the Windows service name is `Ieum`,
and the Start menu includes an uninstall shortcut. Windows packages are not yet code-signed, so SmartScreen may
show a warning.

## Quick start

1. Install Ieum on every computer you want to control.
2. Select `Server` on the computer with the physical keyboard and mouse.
3. Select `Client` on the other computers and enter the server address.
4. Arrange the screens on the server, then start Ieum.

See the [IME guide](docs/user/ime.md) for input behavior and platform permissions.

## Open source and paid product direction

The local KVM core and current distribution are published under
`GPL-2.0-only WITH LicenseRef-OpenSSL-Exception`. Ieum will not place closed-source core features inside the same
GPL-covered executable.

The following commercial areas are **planned directions**, not products currently for sale.

| Area | Open/paid direction |
| --- | --- |
| Community | Local-network KVM, IME synchronization, clipboard, and protocol core remain GPL-licensed |
| Official distribution | Signed and notarized builds, stable update channels, easier onboarding, and verified binaries may be paid conveniences |
| Teams/Cloud | Hosted relay and discovery, team policy, SSO, audit history, and a management console may be separate services |
| Support | Priority support, deployment assistance, enterprise integration, and maintenance contracts may be paid |

GPL binaries may be sold, but recipients retain the right to receive corresponding source and redistribute their
copies. Sustainable paid value therefore comes from **official trust, operational convenience, hosted services,
and support**, not from restricting source access. Any proprietary component must remain at a defensible separate
process or network-service boundary and receive a license review before release. See the
[GNU GPL v2 FAQ](https://www.gnu.org/licenses/old-licenses/gpl-2.0-faq.en.html) for the underlying distribution
rules. This project direction is not legal advice; commercial distribution requires a separate copyright,
trademark, and service-boundary review.

## Roadmap

- [x] Fork bootstrap and full Ieum branding
- [x] Input-source control protocol and platform controllers
- [x] IME raw-scancode path, macOS event path, and NFC normalization
- [x] Automated Windows, macOS, and Linux packages
- [ ] Publish the physical Windows ↔ macOS input matrix
- [ ] Code signing, Apple notarization, and a stable update channel
- [ ] Define the product boundary for hosted relay and device discovery
- [ ] Finalize `v1.0.0` acceptance criteria

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

See the [build guide](docs/dev/build.md), [protocol reference](docs/dev/protocol_reference.md), and
[Phase 0 report](phase0_report.md). GitHub Actions validates Windows x64/ARM64, macOS Intel/Apple Silicon,
multiple Linux distributions, Flatpak, and FreeBSD.

## Lineage and license

Ieum is developed from `deskflow/deskflow` at commit `39bf4fb`. Some internal binary names and the
`org.deskflow.deskflow` identifier remain for protocol compatibility and existing macOS permission continuity.
The user-facing product name, icon, installers, and releases are Ieum.

The project is distributed under `GPL-2.0-only WITH LicenseRef-OpenSSL-Exception`, preserving upstream copyright
and license notices.
