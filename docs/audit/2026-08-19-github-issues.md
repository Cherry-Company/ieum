# Repository-Wide Audit GitHub Issue Index

This index records the public issues created from the 2026-08-19 audit of
`Cherry-Company/ieum` at product baseline
`b94bdc1e381f2f5a749eb1f787ca5f1cf0e08081`.

All 33 issues were searched for duplicates before creation, created in the
public repository at the user's request, and read back from GitHub to verify the
title, full body, open state, and label. The final aggregate verification found
no missing numbers, duplicate titles, empty bodies, or missing baseline SHAs.

| Issue | Label | Area | Title |
| ---: | --- | --- | --- |
| [#6](https://github.com/Cherry-Company/ieum/issues/6) | `bug` | net | Make TLS write retry state connection-local and stop writable polling spins |
| [#7](https://github.com/Cherry-Company/ieum/issues/7) | `bug` | base | Fix exact-capacity formatting and log-level bounds |
| [#8](https://github.com/Cherry-Company/ieum/issues/8) | `bug` | logging | Preserve rotated logs and resume tailing after file replacement |
| [#9](https://github.com/Cherry-Company/ieum/issues/9) | `bug` | base | Enforce RFC 3629 UTF-8 range and encoding length |
| [#10](https://github.com/Cherry-Company/ieum/issues/10) | `bug` | settings | Clean unknown persisted state keys from the state store |
| [#11](https://github.com/Cherry-Company/ieum/issues/11) | `bug` | net/win | Harden address parsing, null equality, and IPv6 any-address detection |
| [#12](https://github.com/Cherry-Company/ieum/issues/12) | `bug` | win | Use a real bounded buffer when enumerating loaded module names |
| [#13](https://github.com/Cherry-Company/ieum/issues/13) | `bug` | win | Make watchdog and event-loop shutdown thread-safe |
| [#14](https://github.com/Cherry-Company/ieum/issues/14) | `bug` | win | Validate and normalize DIB/CF_HTML clipboard payloads |
| [#15](https://github.com/Cherry-Company/ieum/issues/15) | `bug` | x11 | Preserve all format-32 property items on LP64 |
| [#16](https://github.com/Cherry-Company/ieum/issues/16) | `bug` | x11 | Validate Motif and MULTIPLE clipboard property boundaries |
| [#17](https://github.com/Cherry-Company/ieum/issues/17) | `bug` | protocol | Treat readf failures as disconnect conditions before using decoded fields |
| [#18](https://github.com/Cherry-Company/ieum/issues/18) | `bug` | server | Bound each client receive dispatch to preserve event-loop fairness |
| [#19](https://github.com/Cherry-Company/ieum/issues/19) | `documentation` | docs | Document protocol 1.11 display layout and fullscreen messages |
| [#20](https://github.com/Cherry-Company/ieum/issues/20) | `bug` | gui | Save externally typed server configuration paths |
| [#21](https://github.com/Cherry-Company/ieum/issues/21) | `bug` | ipc | Preserve values containing `=` in line-based IPC messages |
| [#22](https://github.com/Cherry-Company/ieum/issues/22) | `bug` | ipc/gui | Wait for daemon start acknowledgement before reporting Started |
| [#23](https://github.com/Cherry-Company/ieum/issues/23) | `bug` | gui | Reject out-of-grid coordinates and malformed screen drag payloads |
| [#24](https://github.com/Cherry-Company/ieum/issues/24) | `bug` | wayland | Generate unique hotkey IDs and preserve locked modifiers |
| [#25](https://github.com/Cherry-Company/ieum/issues/25) | `bug` | wayland | Preserve full high-resolution and smooth-scroll deltas |
| [#26](https://github.com/Cherry-Company/ieum/issues/26) | `bug` | wayland | Restrict portal barriers to outer edges and keep coordinate mapping stable |
| [#27](https://github.com/Cherry-Company/ieum/issues/27) | `bug` | wayland | Release idle EIS emulation so client displays can sleep |
| [#28](https://github.com/Cherry-Company/ieum/issues/28) | `bug` | x11 | Release synthetic keys before refreshing the XKB map |
| [#29](https://github.com/Cherry-Company/ieum/issues/29) | `bug` | mac | Release pasteboard resources and validate item lookup results |
| [#30](https://github.com/Cherry-Company/ieum/issues/30) | `bug` | mac | Null-check TIS language arrays and invalidate event taps on disable |
| [#31](https://github.com/Cherry-Company/ieum/issues/31) | `bug` | mac | Honor selectable IME layouts, keyboard type, and modifier side |
| [#32](https://github.com/Cherry-Company/ieum/issues/32) | `bug` | mac/gui | Route native quit through close-to-tray policy |
| [#33](https://github.com/Cherry-Company/ieum/issues/33) | `bug` | win | Resynchronize key state and preserve resolved buttons across repeats |
| [#34](https://github.com/Cherry-Company/ieum/issues/34) | `bug` | ci | Trigger CodeQL and SonarCloud on the actual default branch |
| [#35](https://github.com/Cherry-Company/ieum/issues/35) | `bug` | ci | Make Valgrind failures gate the protected CI result |
| [#36](https://github.com/Cherry-Company/ieum/issues/36) | `enhancement` | ci | Reduce workflow token scope and pin third-party actions immutably |
| [#37](https://github.com/Cherry-Company/ieum/issues/37) | `enhancement` | i18n | Add translation coverage and source-sync gates |
| [#38](https://github.com/Cherry-Company/ieum/issues/38) | `enhancement` | gui | Move Tailscale status queries off the UI thread |

## Publication boundary

Three potentially sensitive candidates were intentionally excluded from this
public index. Their code locations, reproduction details, and impact chains are
not recorded in public repository artifacts. They require private re-validation
in a session capable of producing a sealed security scan before advisory
tracking.
