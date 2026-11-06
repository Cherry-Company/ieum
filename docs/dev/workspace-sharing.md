# Workspace sharing implementation

This is an unreleased alpha.32 candidate. The public release remains alpha.31.
Native features are implemented as a preview; Windows/macOS hardware acceptance
and production billing are not complete. The [user guide](../product/workspace-sharing.ko.md)
records platform requirements and operating limits.

## Transport and trust

Protocol 1.15 adds `DPRF`: four command bytes, a big-endian 32-bit payload length,
then a UTF-8 JSON envelope. The envelope is version 1 and contains exactly
`v`, `source`, `target`, `op`, `feature`, `session`, `body`. A body is at most 16
fields. Network reads reject lengths over 24 KiB before allocating. A media chunk
is at most 12 KiB before base64 encoding. Nothing opens an extra listening port.

The hub replaces the claimed source with the connected proxy's identity and
routes only to a connected peer. Accepted sessions bind both endpoints to hub-assigned
connection identities; disconnect removes their routes. A newly connected machine
with the same name cannot receive the old session’s media or input. Only the hub may return a peer list. Discovery
includes protocol 1.15 peers, capped at 64. Mixed versions retain their negotiated
basic-KVM behavior; new sharing requires the current version on the complete route.
TLS and feature entitlement are checked in both GUI and core. Discovery and
closing an existing session do not require a paid plan, but still require TLS.

The GUI-core local channel requires an exact version handshake and explicit
registration before sending or receiving session data. Windows uses the existing
authorized GUI validator; Unix uses OS peer credentials and the core's UID.
Generic IPC listeners receive no media, and media is not replayed after reconnect.
Input and media payloads are redacted from logs. Incoming requests, Qt delivery
queues and local socket writes are bounded; blocked input sequences end the
session rather than silently retaining a held key.

## Session ownership and consent

Every session uses a fresh UUID, exact peer/feature binding, and receiver consent.
Pending requests expire after 60 seconds and are capped at 12 per GUI. Saved
permissions are specific to a peer, feature and hub-attested connection identity.
They last only while that peer’s core connection stays alive. Reconnection requires
new consent even if the name is unchanged; a peer cannot supply its own connection
identity. Revocation advances a permission generation and stops all sessions.
Restarting does not resume captures.

Active sessions use a six-second heartbeat lease. Missing input sequence numbers,
disconnect, license expiry and local takeover release injected keys and buttons.
Desktop locking ends media/input sessions; wake never authenticates the user.
Ctrl+Alt+Esc is a local emergency exit. Capture ignores injected events to avoid
loops; movement is coalesced every eight milliseconds and flushed before keys,
buttons and wheel events to preserve ordering.

Only one input owner is allowed, except one source broadcasting to multiple
consenting destinations. Ordinary KVM forwarding pauses before native capture
begins. The GUI waits for the core's acknowledgment rather than an arbitrary
delay. The hub tracks accepted input leases even when neither endpoint is the
primary computer, preventing regular KVM input from colliding with a session
between secondary computers. A stale lease expires without a live GUI.

## Native backends and resource bounds

| Area | Implementation and limits |
| --- | --- |
| Screen | Primary display, JPEG quality 55, at most 1280×720 at 5 fps. One capture is reused across viewers. Frames are capped at 384 KiB; one acknowledged 12 KiB chunk is in flight per viewer. Decoder dimensions are checked before image allocation. |
| Audio | 48 kHz, stereo, signed 16-bit little-endian PCM. Windows WASAPI loopback/render; macOS ScreenCaptureKit/AudioQueue; Linux PulseAudio monitor utilities. Four unacknowledged packets; a three-second acknowledgment deadline. Playback queues are bounded to about 200 ms. |
| Input | Windows low-level hooks/SendInput, macOS CGEvent taps/injection and existing canonical scancode map, Linux XInput2/XTest. Absolute pointer positions are normalized to the desktop bounds. |
| Lock/wake | Native session lock/display wake; logind on Linux. Bounded remembered MAC addresses enable local Wake-on-LAN broadcasts. No login unlock or WAN relay. |
| Idle | No screen/audio/input capture without a session. Sharing timers stop offline; discovery runs every three seconds while connected. Linux lock-state queries use asynchronous D-Bus, not a subprocess for every input event. |

System audio excludes the microphone. A receiver accepts one audio source and
does not re-broadcast received audio, preventing feedback cycles. Audio device
changes report an error and require restarting sharing. macOS capture needs 13+;
ScreenCaptureKit is weak-linked so an older supported OS can still launch basic KVM.

These new Linux screen/input features require X11. Linux session authorization
also requires a valid logind lock state. Wayland and Flatpak support for the new
features is not implemented. Windows elevated windows require matching privileges.
macOS accessibility/screen permission behavior, IME switching across physical
keyboards and multi-monitor geometry still need real-device acceptance.

## Verification

The Linux candidate builds with Qt 6.8.2 and GCC 14. Run the existing CMake suite:

```sh
QT_QPA_PLATFORM=offscreen ctest --test-dir build/src/unittests --output-on-failure
QT_QPA_PLATFORM=offscreen build/bin/legacytests
```

`ProfessionalProtocolTests` checks fragmented reads, every truncated frame,
malformed envelopes and rejecting oversized lengths before payload reads.
`IpcServerTests` checks registration, stale versions, media non-replay and batches
that contain individually valid frames. `ProfessionalSessionTests` covers consent,
revocation, bounded audio/video, ownership acknowledgment, disconnect/key release,
simultaneous input, heartbeat/license expiry, lock behavior, hub leases, name reuse across connections and UI actions.

`NativeSessionBackendTests` is opt-in because it injects real OS input. Run it only
against an isolated Xvfb desktop and PulseAudio null sink, using
`IEUM_TEST_VIRTUAL_DESKTOP=1`, `IEUM_TEST_VIRTUAL_AUDIO=1`, the isolated `DISPLAY`
and `PULSE_SERVER`. It verifies application key delivery, key/button release,
injection-loop filtering, actual monitor audio capture and output startup. The
normal suite skips these two hardware-facing cases.

The current Linux run passes 80 CTest targets and 27 legacy cases; both opt-in
native cases also pass. Windows backend syntax passes with MinGW/Windows headers.
That check does not link an installer or run WASAPI on hardware. macOS has not been
built in this Linux workspace. Public CI for these changes cannot run until the
owner's original privacy-gate inputs are restored and the source is pushed.

## Release boundary

No new payment account, hosting credential, public endpoint, release tag or
installer has been published. Checkout remains disabled. The owner-private
issuer/signing material stays outside this repository. Source publication remains
subject to the [privacy gate](public-privacy-guard.md); its required original
deny map and frozen digest are not available in this workspace.
