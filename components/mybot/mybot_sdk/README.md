# agent-studio-device-firmware component

This directory is the BK7258 build input for agent-studio-device-firmware. It contains:

```text
include/mybot/          public SDK and platform headers
src/                    cross-platform SDK implementation
platforms/bk725x/       BK725x adapters and device modules
CMakeLists.txt          BK7258 AP component registration
Kconfig                 language and CPU-diagnostic options
SDK_REVISION            upstream source revision and tree digests
```

The component has no Linux example, test directory, or nested AOSL submodule.
It is built by the Beken Armino project in
[`projects/mybot`](../../../projects/mybot) together with the local
`mybot_aosl` and `mybot_rtsa` components.

## Integration

The application supplies a `mybot_config_t` and calls the public lifecycle API:

```c
mybot_start(&config);
mybot_get_state();
mybot_stop();
```

Before start, the BK725x controller registers one complete platform descriptor:

```c
if (bk725x_platform_adapters_register() < 0) {
    /* Abort startup. */
}
```

Wi-Fi, KV storage, key input, audio capture, and audio playback are required by
the platform contract. The BK725x descriptor also installs HTTPS, hardware
volume, LCD, and announcement operations.

The platform port does not implement video capture or local wake words. The
bundled configuration keeps encoded video disabled and uses button input to
start conversations.

## Source rules

`include/` and `src/` are a synchronized SDK snapshot recorded by
`SDK_REVISION` and `../snapshots.json`. BK-specific product behavior belongs in
`platforms/bk725x/` or `projects/mybot/`. A source update must refresh the
selected snapshot and checksums as one reviewed change.

See the repository [README](../../../README.md),
[dependency record](../snapshots.json), and
[platform port](platforms/bk725x/README.md) for build and ownership details.
