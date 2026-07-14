<!-- SPDX-FileCopyrightText: (C) 2026 Ieum Developers -->
<!-- SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception -->

# Ieum Developer Guide

Ieum is an IME-native software KVM derived from Deskflow. New development targets the `ieum/main` branch and is
released from [victoriousian/ieum](https://github.com/victoriousian/ieum).

- [Build instructions](build.md)
- [IME protocol and implementation](ime.md)

Internal `deskflow` namespaces, core binary names, and reverse-DNS identifiers are retained where changing them
would break protocol compatibility, packaging upgrades, or macOS permission continuity.
