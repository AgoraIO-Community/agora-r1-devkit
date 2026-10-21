# mybot-bk7258

[![License](https://img.shields.io/github/license/junlon2006/mybot-bk7258)](LICENSE)
[![Upstream](https://img.shields.io/badge/upstream-mybot-0969da)](https://github.com/junlon2006/mybot)
[![Platform](https://img.shields.io/badge/platform-BK7258-2f6f4e)](https://github.com/bekencorp/bk_avdk_smp)

**[简体中文](README.zh-CN.md) | English**

**mybot-bk7258** is the BK7258 SMP reference implementation of the open-source
[mybot](https://github.com/junlon2006/mybot) AI voice conversation SDK. This repository organizes
the official Beken SDK and AI solution as pinned Git submodules. The mybot firmware project,
platform port, runtime snapshots, and SDK patches are maintained in this repository. It is
intended to provide reproducible builds, a practical platform development baseline, and a shared
foundation for community collaboration.

> This project is an actively developed BK7258 reference implementation. Before using it in a
> production product, complete the board-level adaptation, security review, and stability
> validation for the target hardware, and verify the licensing and commercial-use terms of all
> third-party components.

## Relationship to upstream mybot

[mybot](https://github.com/junlon2006/mybot) is a cross-platform AI voice conversation SDK for
edge devices. Its core is written in C99, uses AOSL as its portable runtime, and obtains device
capabilities such as Wi-Fi, persistent storage, buttons, displays, audio, and network transport
through platform `ops` interfaces.

The local snapshot uses mybot SDK 1.3.0 and retains the latest BK7258 audio lifecycle,
device identity implementation, and LCD status badges from the source baseline.

This repository does not redefine the mybot SDK. It provides the complete implementation required
to run mybot on the BK7258 platform:

- BK7258 AP/CP dual-core startup, memory layout, and Flash partition configuration.
- BK725x Wi-Fi APSTA provisioning, network reconnection, and credential persistence.
- Microphone capture, speaker playback, volume control, and audio power management.
- Button, dual-display, EasyFlash KV, HTTPS, and device UID adapters.
- Full-duplex AI voice sessions over Agora RTSA, including user interruption of AI responses.
- Embedded Chinese and English OGG assets for provisioning prompts and pairing-code announcements.
- A complete flash image and an OTA package containing both the CP and AP firmware.

The device service, Agora RTC cloud service, and cloud AI agent are not part of this repository. A
device service compatible with the mybot protocol is required to run the complete workflow.

## System architecture

```mermaid
flowchart LR
    user["User"] <--> device["BK7258 device"]
    device --> platform["BK725x platform port<br/>Wi-Fi · Audio · Key · LCD · KV · HTTPS"]
    platform --> core["mybot SDK<br/>Provisioning · Pairing · Session state machine"]
    core <--> rtc["Agora RTC"]
    rtc <--> agent["Cloud AI agent<br/>ASR · LLM · TTS"]
```

The BK7258 firmware uses an AP/CP dual-core architecture:

- **CP** performs base system initialization and SMP startup control.
- **AP** initializes the media service and starts the mybot controller. It hosts the platform
  adapters, device lifecycle, networking, audio, display, and RTC session.
- **Controller** selects APSTA provisioning or normal STA mode according to the saved Wi-Fi
  credentials. After the network connects, it starts the mybot SDK asynchronously and manages
  network recovery and unexpected SDK restarts.

## Repository layout

| Path | Responsibility | Source |
| --- | --- | --- |
| `bk_avdk_smp/` | BK7258 SMP SDK and platform foundation components | [bekencorp/bk_avdk_smp](https://github.com/bekencorp/bk_avdk_smp) |
| `bk_solution_ai/` | Official AI solution components | [bekencorp/bk_solution_ai](https://github.com/bekencorp/bk_solution_ai) |
| `projects/mybot/` | AP/CP entry points, board configuration, partition tables, and prompt assets | This repository |
| `components/mybot/mybot_sdk/` | mybot SDK 1.3.0 and the `platforms/bk725x` implementation | Local source snapshot |
| `components/mybot/mybot_aosl/` | AOSL runtime and BK7258 adaptation | Local source snapshot |
| `components/mybot/mybot_rtsa/` | Agora RTSA headers and binary, build `1292388` | Local snapshot |
| `patches/` | Small patches applied when preparing the SDK | This repository |
| `scripts/` | Vendor preparation and language-specific firmware builds | This repository |

The official SDK baselines are fixed at `d2ded037798530175e5dc5cde6fa1878f5d5ef35`
(`bk_avdk_smp`) and `5e3ad9e818d3a3962eb45db23fd71b61238db5af` (`bk_solution_ai`).
See [official dependencies](docs/OFFICIAL_DEPENDENCIES.md) for the patch scope and update procedure.

The project, BK725x adapters, AOSL, and RTSA inputs preserve main revision `04c8283` and its pinned
Solution `438f1325` and AVDK `a9529f12` snapshots. Their sources and binaries are recorded by
checksum in `components/mybot/snapshots.json`.
The mybot SDK `include/` and `src/` snapshot is updated separately to upstream `3f9a1ad`
(version 1.3.0), as recorded in `components/mybot/mybot_sdk/SDK_REVISION`.

The local runtime components build for AP only. `CONFIG_AGORA_IOT_SDK` stays disabled because
`mybot_aosl` and `mybot_rtsa` provide the runtime used by this project.

## Requirements

- Git with Git submodule support.
- Python 3 and GNU Make.
- A Linux build environment and the cross-compilation toolchain required by `bk_avdk_smp`.
- A BK7258 development board or compatible hardware using the same peripheral connections.
- Access to a compatible mybot device service and valid Agora service configuration.

For BK7258 SDK environment setup and flashing tools, refer to the
[Beken BK7258 SMP documentation](https://docs.bekencorp.com/arminodoc/bk_avdk_smp/smp_doc/bk7258/en/v3.1.1/index.html).

## Get the source

Clone the repository and its submodules over HTTPS:

```bash
git clone --recurse-submodules https://github.com/junlon2006/mybot-bk7258.git
cd mybot-bk7258
```

If the top-level repository has already been cloned without its submodules, run:

```bash
git submodule sync --recursive
git submodule update --init --recursive
```

After cloning, inspect the pinned revisions with:

```bash
git submodule status
```

## Build the firmware

Run the following command from the repository root to build both language versions:

```bash
python3 scripts/build_all.py
```

`--language both` is the default. To build one version:

```bash
python3 scripts/build_all.py --language zh-CN
python3 scripts/build_all.py --language en-US
```

The build prepares separate project copies for each language, applies the repository's SDK
patches, and builds the CP and AP firmware. It keeps tracked files in the official submodules
clean and leaves the source project configuration unchanged. The complete flash images are:

| File | Purpose |
| --- | --- |
| `releases/bk7258-zh-CN.bin` | Chinese firmware containing the bootloader, CP, and AP |
| `releases/bk7258-en-US.bin` | English firmware containing the bootloader, CP, and AP |

The script also supports `--dry-run`, `--no-clean`, `--build-root`, and `--output-root`.
Images are published only after all requested builds succeed.

For manual builds, prepare one language and run the Make command printed by the script:

```bash
python3 scripts/prepare_vendor.py --language en-US
```

The generated project's `build/bk7258/mybot/package/` directory also contains `all-app.bin`,
`app_pack.rbl` (CP/AP OTA package), and `build_summary.txt` (memory usage report). Individual
CP and AP images are under `build/bk7258/mybot/bk7258/` and `build/bk7258/mybot/bk7258_ap/`.

Both language builds passed on 2026-10-21; hardware testing remains outstanding. See the
[validation record](docs/OFFICIAL_DEPENDENCIES.md#validation) for checks and firmware hashes.

## Flash and run

Use the Beken flashing tool to write the desired `releases/bk7258-*.bin` image to the device.
These files are copies of the corresponding complete `all-app.bin` images. Follow the instructions for the
target development board and the
[Beken flashing documentation](https://docs.bekencorp.com/arminodoc/bk_avdk_smp/smp_doc/bk7258/en/v3.1.1/get-started/index.html).

The reference device workflow is:

1. On first boot, or when no valid Wi-Fi credentials exist, the device enters APSTA provisioning
   mode and creates a `mybot-xxx` SoftAP.
2. Connect to the SoftAP and open `http://192.168.4.1/`, then select the target network and submit
   its credentials.
3. After connecting to the network, the device starts the mybot SDK and performs device
   registration, pairing, and authentication.
4. An unclaimed device displays and announces its pairing code. After the device is claimed, use
   the action button to start an AI voice conversation.

The current reference-board button mapping is:

| GPIO | Action | Function |
| --- | --- | --- |
| GPIO13 | Short press | Increase volume |
| GPIO8 | Short press | Decrease volume |
| GPIO12 | Short press | Start or stop a conversation |
| GPIO12 | Long press, approximately 3 seconds | Re-enter provisioning mode |

GPIO assignments and display and audio peripheral connections are board-level configuration.
Porting to different BK7258 hardware requires corresponding configuration and platform changes.

## Configuration

The primary project configuration files are:

- AP: `projects/mybot/ap/config/bk7258_ap/config`
- CP: `projects/mybot/cp/config/bk7258/config`
- Flash partitions: `projects/mybot/partitions/bk7258/auto_partitions.csv`
- SRAM/PSRAM layout: `projects/mybot/partitions/bk7258/ram_regions.csv`

The mybot Kconfig currently provides:

- `CONFIG_MYBOT_LANGUAGE_ZH_CN`: Chinese service region and Chinese prompt assets.
- `CONFIG_MYBOT_LANGUAGE_EN_US`: English service region and English prompt assets.
- `CONFIG_MYBOT_DEBUG_CPU`: Periodically print CPU usage diagnostics; disabled by default.

The language option selects both the service region and the prompt asset directory. Exactly one of
the two language options must be enabled. The source configuration retains its English selection;
the build scripts set the requested language in each generated project copy. Camera and video
variants are not part of these build commands.

| Firmware language | Device-service region | Device-service base URL | TLS root certificate |
| --- | --- | --- | --- |
| `zh-CN` | Mainland China | `https://botstation.shengwang.cn/api` | DigiCert Global Root G2 |
| `en-US` | Overseas | `https://botstation.agora.io/api` | Go Daddy Root Certificate Authority - G2 |

The base URL includes `/api` without a trailing slash, as specified by the upstream SDK.
Each firmware compiles only the root certificate for its region.

## Embedded voice assets

Prompt assets are stored under `projects/mybot/assets/locales/` as 16 kHz mono
Opus-in-Ogg files. They are compiled into the AP firmware as read-only C arrays, require no SD
card, and use no separate `assets_data` partition. Decoded PCM buffers are allocated from PSRAM
at runtime.
The Chinese and English pairing prompt and digit sounds are converted from SDK `3f9a1ad` PCM
assets using the project's existing Opus settings.

After modifying or adding an OGG file, regenerate the C arrays manually before building:

```bash
python3 projects/mybot/scripts/generate_assets_c.py \
  projects/mybot/assets \
  components/mybot/mybot_sdk/platforms/bk725x/modules/storage/mybot_assets.c
```

Intentional source or asset changes require reviewing the changed source, including generated
`mybot_assets.c`, and updating the matching SHA-256 entries in `components/mybot/snapshots.json`.
Use `sha256sum` to calculate the reviewed files' hashes. Builds verify this manifest and do not
silently regenerate it.

The generator collects only `locales/**/*.ogg`. See
[`projects/mybot/assets/README.md`](projects/mybot/assets/README.md) for the audio
format and update procedure.

## Submodule version management

Regular users should keep the revisions pinned by the top-level repository:

```bash
git pull --ff-only
git submodule sync --recursive
git submodule update --init --recursive
```

Do not use `git submodule update --remote` for routine setup or builds. An SDK update is a separate
change: choose official commits, check the local patches and runtime integration, build both
languages, and validate on hardware before updating the gitlinks. Product changes belong in
`projects/mybot/` and `components/mybot/`; SDK changes belong in reviewed patches under `patches/`.
Do not commit generated vendor trees, `build/`, or `__pycache__/` directories. See
[official dependencies](docs/OFFICIAL_DEPENDENCIES.md).

## Documentation

- [mybot project](https://github.com/junlon2006/mybot)
- [mybot English documentation](https://github.com/junlon2006/mybot/blob/main/README.md)
- [mybot porting guide](https://github.com/junlon2006/mybot/blob/main/docs/PORTING.md)
- [mybot embedded integration guide](https://github.com/junlon2006/mybot/blob/main/docs/EMBEDDED.md)
- [BK7258 platform component notes](components/mybot/mybot_sdk/README.md)
- [Official dependencies and local patches](docs/OFFICIAL_DEPENDENCIES.md)

## Contributing

Issues and pull requests are welcome:

- Submit platform-independent SDK features and public API changes to the
  [upstream mybot project](https://github.com/junlon2006/mybot).
- Submit BK7258 platform and firmware changes to this repository. Keep product code in the local
  project and components, and keep required official SDK changes as small patches. General SDK
  fixes can also be proposed to [Beken](https://github.com/bekencorp).
- Report integration, build, and documentation issues in
  [mybot-bk7258 Issues](https://github.com/junlon2006/mybot-bk7258/issues).

Keep each change focused. Describe the hardware, configuration, and validation used, and do not
commit build artifacts.

## License and third-party dependencies

Original content in this top-level repository is licensed under the
[Apache License 2.0](LICENSE).

The two submodules and their third-party components are governed by their respective licenses and
terms of use, including but not limited to:

- [bk_avdk_smp license](bk_avdk_smp/LICENSE)
- [bk_solution_ai license](bk_solution_ai/LICENSE)
- [mybot SDK license](components/mybot/mybot_sdk/LICENSE)
- [AOSL license and additional terms](components/mybot/mybot_aosl/aosl/LICENSE)
- [mybot third-party notices](https://github.com/junlon2006/mybot/blob/main/THIRD_PARTY_NOTICES.md)

Prebuilt binaries such as Agora RTSA may be subject to evaluation periods, redistribution
restrictions, and commercial licensing requirements. Before using this project in a product or
distributing firmware, independently review and comply with all applicable terms.
