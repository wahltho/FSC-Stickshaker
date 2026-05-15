# FSC Stickshaker

Standalone X-Plane plugin project for driving FSC Stick Shaker hardware.

This project is intentionally separate from `FSCB738TQ-Nextgen` and from the CPFlight plugin. The current goal is to build a small, self-contained plugin that listens to a stall-warning trigger in X-Plane/Zibo and sends simple ON/OFF commands to the stick shaker device.

## Current Status

- Initial C++ plugin skeleton exists.
- No real hardware is currently available for validation.
- Serial and TCP protocol handling is still stubbed/log-only in the current skeleton.

## Implementation Constraints

- Implementation language: C++.
- Target platforms: macOS, Windows, and Linux X-Plane plugin builds.
- Runtime configuration should use an X-Plane `.prf` file.
- Local build directories should live under:
  - `/Users/wahltho/dev/FSC Stickhaker`

## Confirmed Findings

- The original Windows driver supports two transports:
  - serial COM
  - TCP/IP
- Serial transport sends 3 raw bytes:
  - `FF 01 00` = likely OFF
  - `FF 01 01` = likely ON
- TCP transport sends the same logical message as 6 ASCII hex characters:
  - `FF0100` or `FF0101`
  - followed by a second frame with the middle byte incremented:
  - `FF0200` or `FF0201`
- No protocol readback, ACK/NACK, or checksum handling has been identified from static analysis.
- For Zibo, the best current runtime trigger candidate is:
  - `sim/cockpit2/annunciators/stall_warning`

## Architecture Decision

This should remain a dedicated stick-shaker project, not an extension of the FSC throttle quadrant plugin.

Reasons:

- different hardware
- different transport model
- some users may have only the stick shaker and no throttle quadrant
- the throttle quadrant repo currently has one global FSC enable/lifecycle path, which would become awkward if reused for unrelated shaker-only users

## Recommended First Implementation Slice

1. Define plugin identity and `.prf` prefs format.
2. Implement a small transport abstraction:
   - serial
   - TCP
3. Implement a simple shaker state machine:
   - trigger false -> OFF
   - trigger true -> ON
4. Add manual test commands:
   - force ON
   - force OFF
   - reload prefs
5. Validate against real hardware.

## Aircraft Gating

Automatic shaker output is gated to configured aircraft tail numbers before the stall-warning trigger is used. Defaults are Zibo-oriented:

- `ZB738`
- `B738`

The plugin also retries missing X-Plane/Zibo DataRefs from the flight loop so plugin load order does not permanently disable the trigger path.

## Build

The build follows the same CMake toolchain style as `FSCB738TQ-Nextgen`.

```sh
cmake -S . -B "/Users/wahltho/dev/FSC Stickhaker/build-mac-universal" -DCMAKE_BUILD_TYPE=Release -DXPLANE_SDK_ROOT="../SDKs/XPlane_SDK" -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
cmake --build "/Users/wahltho/dev/FSC Stickhaker/build-mac-universal" --config Release
ctest --test-dir "/Users/wahltho/dev/FSC Stickhaker/build-mac-universal" --output-on-failure
```
