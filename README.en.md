<!-- SPDX-FileCopyrightText: (C) 2026 Ieum contributors -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

<p align="center"><img src="artwork/ieum-icon-1024.png" alt="Ieum" width="112"></p>
<h1 align="center">Your computers. One keyboard and mouse.</h1>
<p align="center">Move between Windows, Mac, and Linux with Ieum.</p>
<p align="center"><a href="https://github.com/Cherry-Company/ieum/releases"><strong>Download</strong></a> · <a href="#connect-your-computers">Get started</a> · <a href="docs/dev/build.md">Build guide</a></p>
<p align="center"><a href="README.md">한국어</a> · English · <a href="README.zh-CN.md">简体中文</a></p>

## Move the pointer. Keep working.

Push your pointer across a screen edge to control the next computer. Your keyboard follows.
Copy text on one computer and paste it on another. Each computer keeps its own display and apps;
Ieum shares input rather than streaming a remote desktop.

| What you need | What Ieum provides |
| --- | --- |
| One set of input devices for your desk | Keyboard and mouse sharing across screen edges |
| Korean and CJK input across operating systems | Input-language synchronization and IME handling |
| Text between computers | Shared clipboard and Unicode normalization |
| Computers on different networks | Host selection through Tailscale |
| Files between Windows and Mac | Pro Local file drag and transfer |

## Connect your computers

Install Ieum on every computer. The **host** has the keyboard and mouse you want to share.
A **client** receives that input.
Open **First connection** in the app to revisit these steps and Mac permission guidance.

1. On the host, choose **Share my keyboard and mouse**, then **Start**.
2. On the other computer, choose **Use a shared keyboard and mouse**. Enter the host’s name or IP
   for a local network, or select it from the Tailscale list. Press **Connect** and verify the fingerprints on both computers.
3. On the host, open **Arrange computers**, enter the client’s exact **This device** name, choose its side,
   and press **Add computer**. Save, then move the pointer toward that screen edge.

For Tailscale, sign both computers into the same tailnet and choose **Tailscale** as the connection method in both apps.
An online Tailscale device also needs Ieum running as a host.

To change hosts, select another computer in the client’s host list. Ieum closes the previous connection and connects
to the new host automatically. A client connects to one host at a time. If a host keeps waiting, check the host
selected on the client, including whether it is already connected elsewhere.

## Install

Choose a package from [Releases](https://github.com/Cherry-Company/ieum/releases).

| Computer | Package |
| --- | --- |
| Windows 10/11, Intel or AMD | x64 MSI |
| Windows 10/11, ARM | ARM64 MSI |
| Apple Silicon Mac | macOS arm64 DMG; current packages target macOS 14+ |
| Intel Mac | macOS x86_64 DMG; current packages target macOS 12+ |
| Linux | Distribution package; experimental support |

On Mac, move Ieum to Applications and grant Accessibility permission. Windows may need the matching Visual C++ runtime.
Alpha packages are awaiting Windows code signing and Apple signing/notarization.
[Detailed installation and permissions, in Korean](docs/user/installation.md).

## Find a setting

Use the shortcuts on the main screen or search in **All settings**. Select a search result to jump to that control.

| Change | Where |
| --- | --- |
| Host and connection method | Main screen |
| Screen positions, switching edges, pointer speed | **Arrange computers** on the host |
| A computer’s name, aliases, and key mapping | **Arrange computers → Computer settings** |
| Input language and clipboard text | **Input and clipboard** |
| Client scrolling and reconnection | **Receiving options** on the client |
| Encryption, certificates, ports | **Network settings → Connection & security** |
| File destination and Pro Local license | **Files** |
| Start at login | **All settings → General** |

Keyboard, mouse, input synchronization, and clipboard sharing are free. Windows ↔ macOS file transfers require
Pro Local activation on both computers and TLS. Pro Local Early Access is USD 30 once, with a license issued
manually by email after payment verification. [Activation and scope](docs/dev/pro_local_file_transfer.md).

## When a connection gets stuck

- **Host keeps waiting:** start sharing on the host and select that host on the client. If the client is connected elsewhere, switch hosts there.
- **Tailscale device is online but Ieum cannot connect:** run Ieum in Host mode on that device and allow TCP `24800` through its firewall and tailnet ACL.
- **Mac receives no input:** check Accessibility approval for the current app in Applications.
- **Report a problem:** use **Help → Report bug**, review the diagnostic, and select **Copy and open GitHub**.
  Paste it into the issue, describe what happened, and submit. If the browser fails, the same window keeps the address
  and lets you save a copy. Nothing is uploaded automatically.

Ieum is alpha software. Physical-device stability, sleep/wake behavior, and OS restarts still need platform testing.
[Report an issue](https://github.com/Cherry-Company/ieum/issues).

## Development and support

[Build and test](docs/dev/build.md) · [IME guide](docs/user/ime.md) · [Protocol](docs/dev/protocol_reference.md) ·
[Security](docs/SECURITY.md) · [GitHub Sponsors](https://github.com/sponsors/victoriousian)

Maintained by Cherry Company. Sponsorship supports signing, notarization, device testing, and releases.
[Sponsorship impact](docs/release/sponsorship-impact.md).

Ieum is forked from Deskflow `39bf4fb` and distributed under
[GPL-2.0-only WITH LicenseRef-OpenSSL-Exception](LICENSE), with upstream notices preserved.
