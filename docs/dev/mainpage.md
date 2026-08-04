<!-- SPDX-FileCopyrightText: (C) 2026 Ieum Developers -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

# Ieum Developer Guide

Ieum is an IME-native software KVM derived from Deskflow. New development targets the `ieum/main` branch and is
released from [YijiOS/ieum](https://github.com/YijiOS/ieum).

- [Build instructions](build.md)
- [IME protocol and implementation](ime.md)

Internal `deskflow` namespaces and core binary names are retained where changing them would break protocol or
package compatibility. Linux retains the upstream app ID for Flatpak compatibility; macOS uses the independent
`io.github.victoriousian.ieum` bundle ID so its TCC grants cannot collide with Deskflow.
