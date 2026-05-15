# FSC Stickshaker

Standalone X-Plane plugin for FSC Stick Shaker hardware.

The plugin is intended for the Zibo 737 and drives the stick shaker from X-Plane's stall warning state. It is independent from the FSC throttle quadrant plugin and does not require CPFlight integration.

## Features

- Drives FSC Stick Shaker hardware from X-Plane/Zibo stall warning.
- Supports serial COM and TCP/IP transport.
- Uses a simple ON/OFF output model.
- Gates automatic operation to Zibo aircraft tail numbers:
  - `ZB738`
  - `B738`
- Retries X-Plane/Zibo DataRef lookup after plugin startup so load order does not block operation.
- Provides manual test commands for setup and diagnostics.

## Requirements

- X-Plane 12.
- Zibo 737.
- FSC Stick Shaker hardware connected by serial COM or TCP/IP.

## Install

Copy the plugin folder to:

```text
<X-Plane>/Resources/plugins/FSCStickShaker
```

The platform binary must be located in:

```text
<X-Plane>/Resources/plugins/FSCStickShaker/64/<mac|lin|win>.xpl
```

## Preferences

Preferences are stored in:

```text
<X-Plane>/Output/preferences/FSCStickShaker.prf
```

Default preferences:

```ini
shaker.enabled=0
shaker.transport=serial
shaker.serial.port=
shaker.serial.baud=115200
shaker.serial.data_bits=8
shaker.serial.parity=none
shaker.serial.stop_bits=1
shaker.tcp.ip=192.168.0.10
shaker.tcp.port=12345
shaker.debug=0
```

`shaker.transport` accepts:

- `serial`
- `tcp`
- `log`

## Commands

- `FSCStickShaker/reload_prefs`
- `FSCStickShaker/test_on`
- `FSCStickShaker/test_off`
- `FSCStickShaker/test_pulse`

## Build

The project uses CMake and the X-Plane SDK.

```sh
cmake -S . -B "/Users/wahltho/dev/FSC Stickhaker/build-mac-universal" -DCMAKE_BUILD_TYPE=Release -DXPLANE_SDK_ROOT="../SDKs/XPlane_SDK" -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
cmake --build "/Users/wahltho/dev/FSC Stickhaker/build-mac-universal" --config Release
ctest --test-dir "/Users/wahltho/dev/FSC Stickhaker/build-mac-universal" --output-on-failure
```
