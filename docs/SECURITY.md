# Security Policy

Ieum transports keyboard, mouse, and optionally clipboard data between
computers. Those capabilities require broad operating-system permissions, so
the permission prompt is part of the security boundary rather than an
installer inconvenience.

## Supported Versions

The latest minor release is supported and receives security updates:
https://github.com/victoriousian/ieum/releases

## Package Trust

- Download packages only from the GitHub release page above.
- Compare the downloaded file with the release's `SHA256SUMS.txt`.
- Windows packages are not yet code-signed.
- macOS packages currently have an ad-hoc code seal but are not signed with a
  Developer ID certificate or Apple-notarized.
- Do not disable SmartScreen, Microsoft Defender, Gatekeeper, or macOS TCC to
  install Ieum. An explicit malware detection or a checksum mismatch is a
  stop condition, not an unsigned-package warning to bypass.

The checksum proves that the downloaded bytes match the published release
asset. It does not replace publisher code signing.

## Operating-System Permissions

### Windows

The MSI requests UAC elevation to install under `C:\Program Files\Ieum`,
register the `Ieum` background service, and add a Windows Firewall program
exception for `ieum-core.exe`. It does not disable Windows Firewall or
Microsoft Defender.

### macOS

macOS requires the user to grant these permissions manually. Ieum cannot and
does not auto-approve them.

| Permission | Purpose |
| --- | --- |
| Local Network | Connect to the Ieum server or client selected by the user |
| Accessibility | Synthesize remote input and handle KVM input events on a Mac server or client |
| Input Monitoring | Observe physical keyboard and mouse input; required on a Mac server |

A client-only Mac should not enable Input Monitoring unless macOS actually
requests it. Ieum does not request Screen Recording, Full Disk Access, Camera,
or Microphone access. Password or Touch ID confirmation shown while changing a
permission is handled entirely by macOS and is not provided to Ieum.

`Reset Previous Approval` resets only the Accessibility record for Ieum's
bundle ID, `io.github.victoriousian.ieum`. It does not alter permissions for
other applications.

See Apple's current guidance for
[Accessibility](https://support.apple.com/guide/mac-help/allow-accessibility-apps-to-access-your-mac-mh43185/mac)
and
[Input Monitoring](https://support.apple.com/guide/mac-help/control-access-to-input-monitoring-on-mac-mchl4cedafb6/mac).

## Data Flow and Privacy

- Ieum contains no advertising SDK or usage-tracking telemetry.
- On first launch, the application asks whether it may check for updates.
  Only after opt-in, it reads the repository's public `VERSION` file and sends
  the Ieum version, operating-system product name, and system locale in that
  request.
- Keyboard, mouse, and enabled clipboard data travel over the direct Ieum
  connection configured by the user. TLS and client-certificate verification
  are enabled by default.
- Clipboard sharing is enabled by default and can be disabled in Server
  Settings.
- Logs may contain device names, IP addresses, certificate fingerprints, and
  connection diagnostics. Redact them before posting a public issue.

## Reporting a Vulnerability

Please report vulnerabilities through
[GitHub Security Advisories](https://github.com/victoriousian/ieum/security/advisories/new)
when the report contains exploit details or sensitive information. Ordinary
hardening requests and non-sensitive bugs can use the
[issue tracker](https://github.com/victoriousian/ieum/issues).
