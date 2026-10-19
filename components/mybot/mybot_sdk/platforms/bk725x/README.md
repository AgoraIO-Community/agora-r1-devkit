# BK725x Platform Port

[English](README.md) | [简体中文](README.zh-CN.md)

This directory contains the BK725x platform implementation for the `agent-studio-device-firmware` SDK. It adapts the
public SDK platform contracts to the Beken BK725x SDK and is intended to be integrated by a BK
Armino-based firmware project.

The complete BK7258 firmware project is maintained in this
[agora-r1-devkit repository](../../../../../README.md).

[`projects/mybot`](../../../../../projects/mybot/) contains the AP/CP startup code, board
configuration, partition tables, and embedded audio assets. The BK application controller is in
[`modules/controller`](modules/controller/).

## Scope

The port provides BK725x implementations for:

- Wi-Fi provisioning, connectivity monitoring, and credential persistence;
- HTTPS transport using the BK mbedTLS integration;
- 16 kHz mono PCM capture and playback, device volume, and shared audio power management;
- OGG/Opus prompt decoding and pairing-code announcement;
- LCD rendering, semantic key events, and BK button input;
- BK environment-backed key-value storage.

`adapter/` contains the registration adapters that connect these implementations to the SDK
`ops` interfaces. The reusable BK modules are grouped under `modules/audio`, `modules/network`,
`modules/display`, `modules/button`, `modules/key`, and `modules/storage`.

The controller and SD-card/USB MSC workflows are application-level code in
[`modules/controller`](modules/controller/) and [`modules/storage`](modules/storage/).
AP/CP entry points, board configuration, partition tables, and product resources are in
[`projects/mybot`](../../../../../projects/mybot/). These are not part of the platform-neutral SDK core.

## Registration

Call the platform registration entry point once, before `mybot_start()`:

```c
#include "bk725x_platform_adapters.h"

if (bk725x_platform_adapters_register() < 0) {
    /* Abort startup and report the platform initialization failure. */
}
```

The registration function installs the BK HTTPS, KV, Wi-Fi, audio, announcement, key, and LCD
implementations. The application controller in [`modules/controller`](modules/controller/) performs this registration as
part of its startup sequence.

## BK build environment

This port is built by the BK Armino/AVDK build system. The firmware project must provide the
BK725x toolchain and the following platform
components:

```text
bk_rtos, bk_common, bk_uid, bk_wifi, bk_event, bk_netif,
bk_display, psa_mbedtls, lwip, easy_flash,
audio_pipeline, onboard_mic_stream, onboard_speaker_stream,
raw_stream, multimedia, json, bk_vfs, fatfs
```

[`projects/mybot`](../../../../../projects/mybot/) contains the project entry points and board-level files.
The component source list and `armino_component_register()` configuration are in
[`components/mybot/mybot_sdk/CMakeLists.txt`](../../CMakeLists.txt).
Follow the [R1 build instructions](../../../../../README.md#build-the-firmware) to prepare the pinned
BK dependencies and compile the firmware with the BK headers, libraries, and toolchain.

## Embedded prompts

The R1 project converts the OGG prompt files under
[`projects/mybot/assets/locales`](../../../../../projects/mybot/assets/locales/) into a C
asset table located in the BK storage module. The platform prompt and announcement code reads the
embedded OGG bytes and decodes them into PSRAM-backed PCM buffers at playback time. The compressed
asset bytes remain part of the AP firmware image and therefore consume Flash space.

To add or replace prompts, use the conversion and generation scripts in
[`projects/mybot/scripts`](../../../../../projects/mybot/scripts/) and follow the
[asset regeneration instructions](../../../../../README.md#embedded-voice-assets) before rebuilding the BK firmware. The
generated asset source is product data and should not be added to the SDK core independently of a
BK product configuration.

## Configuration notes

- `CONFIG_USBD_MSC` is disabled by default in the current R1 project. USB MSC and SD-card sharing
  are optional product features and are not required by the SDK platform contracts.
- BK-specific allocation, RTOS, networking, and display headers are confined to this directory.
- Application code should use the public SDK APIs, including `mybot_get_state()` and the semantic
  conversation key events, instead of SDK-private lifecycle interfaces.

For the complete project layout, board configuration, partition table, firmware build commands,
and device validation workflow, see the [R1 README](../../../../../README.md).
