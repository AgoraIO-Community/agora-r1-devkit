# BK725x platform port

This directory adapts the agent-studio-device-firmware platform contract to the BK7258 project in this
repository. It is part of the local SDK build input and is compiled only
for `bk7258_ap`.

## Layout

```text
adapter/                platform descriptor and SDK ops adapters
modules/announce/       pairing-code announcement
modules/audio/          capture, playback, volume, Opus decoding, prompts
modules/button/         GPIO buttons
modules/controller/     provisioning, network, and SDK lifecycle owner
modules/display/        dual GC9D01 display
modules/key/            button-to-SDK event dispatch
modules/network/        Wi-Fi, connectivity, HTTPS, TLS roots, credentials
modules/storage/        embedded assets and BK environment KV
```

## Capabilities

The registered descriptor implements Wi-Fi connectivity, KV storage, key
input, 16 kHz mono PCM capture and playback, hardware volume, HTTPS with
mbedTLS, dual-screen LCD output, and Ogg prompt announcement.

The product controller owns APSTA provisioning, STA connectivity, credentials,
and the SDK lifecycle. It stops the SDK before re-entering provisioning and
starts the SDK again only after STA connectivity is restored.

The reference display adapter drives two GC9D01 SPI panels. The reference board
uses GPIO8, GPIO12, and GPIO13 buttons; see the top-level README for actions.

Video capture and local wake words are not implemented by this port. Encoded
video is disabled in the bundled SDK build configuration.

## Registration

The controller calls `bk725x_platform_adapters_register()` once before
`mybot_start()`:

```c
#include "bk725x_platform_adapters.h"

if (bk725x_platform_adapters_register() < 0) {
    /* Abort startup and report the initialization failure. */
}
```

The complete source list, include paths, and component dependencies are in
[`CMakeLists.txt`](../../CMakeLists.txt). The AP project entry point is
[`projects/mybot/ap/ap_main.c`](../../../../../projects/mybot/ap/ap_main.c).

## Prompts and configuration

Prompt files are maintained by the project under
[`projects/mybot/assets`](../../../../../projects/mybot/assets). The generated
[`mybot_assets.c`](modules/storage/mybot_assets.c) table embeds them in the AP
image.

Language selection and service endpoints are defined in
[`mybot_language.h`](modules/controller/mybot_language.h). Regional TLS roots
are defined in [`mybot_tls_roots.h`](modules/network/mybot_tls_roots.h). Build
and configuration instructions are in the top-level
[README](../../../../../README.md) and
[dependency notes](../../../../../docs/OFFICIAL_DEPENDENCIES.md).
