# agora-r1-devkit

**[简体中文](README.zh-CN.md) | English**

BK7258 firmware reference for the
[agent-studio-device-firmware](https://github.com/AgoraIO-Community/agent-studio-device-firmware)
AI voice SDK. The repository combines pinned Beken SDK submodules with a local SDK snapshot, a BK725x platform
port, and the firmware project used to build the AP and CP images.

This is a development and porting baseline. Board-level adaptation, security
review, hardware validation, and third-party licensing must be checked for the
target product. The device service, Agora cloud service, and cloud AI agent are
external dependencies.

## Repository layout

| Path | Content |
| --- | --- |
| `bk_avdk_smp/` | Pinned Beken AVDK submodule |
| `bk_solution_ai/` | Pinned Beken solution submodule |
| `projects/mybot/` | BK7258 AP/CP project, board configuration, partitions, and prompt assets |
| `components/mybot/mybot_sdk/` | agent-studio-device-firmware snapshot and `platforms/bk725x` port |
| `components/mybot/mybot_aosl/` | AOSL runtime snapshot and BK725x HAL |
| `components/mybot/mybot_rtsa/` | Agora RTSA header and ARM library |
| `patches/` | AVDK patches applied to generated build sources |
| `scripts/` | Source preparation, build, and verification scripts |

The BK7258 firmware uses AP/CP separation. The AP entry point initializes the
media service and starts the SDK controller. The controller owns APSTA
provisioning, STA connectivity, credential persistence, and SDK start/stop.
The SDK supplies pairing, conversation state, audio streaming, and RTC session
handling through the registered BK725x platform descriptor.

## Requirements

- Git with submodule support
- Python 3, GNU Make, and the Beken build environment
- `click_option_group` for the Armino build
- A BK7258 development board with matching board and peripheral configuration
- A compatible SDK device service and Agora service configuration

## Get the source

```bash
git clone --recurse-submodules https://github.com/AgoraIO-Community/agora-r1-devkit.git
cd agora-r1-devkit
```

For an existing clone:

```bash
git submodule sync --recursive
git submodule update --init --recursive
```

Do not use `git submodule update --remote` for routine builds. The checked-out
gitlinks, not the branch names in `.gitmodules`, select the dependency versions.
See [official dependencies](docs/OFFICIAL_DEPENDENCIES.md) for exact commits,
patches, and local snapshot provenance.

## Build

From the repository root, build both languages:

```bash
python3 scripts/build_all.py
```

Build one language:

```bash
python3 scripts/build_all.py --language zh-CN
python3 scripts/build_all.py --language en-US
```

The script creates isolated build directories, applies the AVDK patches,
selects one language in each generated project copy, and checks the resulting
AP configuration and firmware. On success it writes these ignored build
artifacts:

```text
releases/bk7258-zh-CN.bin
releases/bk7258-en-US.bin
```

Other options are shown by `python3 scripts/build_all.py --help`. To prepare one
variant and inspect its Make command without building it:

```bash
python3 scripts/prepare_vendor.py --language en-US
```

The generated project also contains `all-app.bin`, `app_pack.rbl`, individual
AP/CP images, and `build_summary.txt` under its `build/bk7258/mybot/` tree.
These paths are build outputs and are not tracked by this repository.

## Flash and run

Flash the generated `releases/bk7258-<language>.bin` with the Beken flashing
tool. The image is copied from the complete `all-app.bin` produced by the build.

The reference flow is:

1. With no saved Wi-Fi credentials, the device starts APSTA provisioning and
   exposes a `mybot-xxx` SoftAP.
2. Connect to the SoftAP, open `http://192.168.4.1/`, and submit Wi-Fi
   credentials.
3. After the STA receives a usable IPv4 address, the controller starts the
   SDK for device pairing and authentication.
4. After the device is claimed, the keys below control volume and conversation
   state.

Reference-board keys:

| GPIO | Input | Action |
| --- | --- | --- |
| GPIO13 | Short press | Volume up |
| GPIO8 | Short press | Volume down |
| GPIO12 | Short press | Start or stop a conversation |
| GPIO12 | Hold about 3 seconds | Stop the SDK and re-enter provisioning |

GPIO, display, and audio wiring are board configuration. Other hardware needs a
corresponding port change and validation.

## Configuration

Main configuration and layout files:

```text
projects/mybot/ap/config/bk7258_ap/config
projects/mybot/cp/config/bk7258/config
projects/mybot/partitions/bk7258/auto_partitions.csv
projects/mybot/partitions/bk7258/ram_regions.csv
```

The SDK Kconfig choice selects exactly one language:

| Setting | Device-service base URL | TLS root |
| --- | --- | --- |
| `CONFIG_MYBOT_LANGUAGE_ZH_CN` | `https://botstation.shengwang.cn/api` | DigiCert Global Root G2 |
| `CONFIG_MYBOT_LANGUAGE_EN_US` | `https://botstation.agora.io/api` | Go Daddy Root Certificate Authority - G2 |

`CONFIG_MYBOT_DEBUG_CPU` enables a periodic CPU diagnostic thread and is off by
default. The source project currently enables `en-US`; build scripts override
the selection only in generated copies.

## Prompt assets

The repository contains 26 Opus-in-Ogg prompt files under
`projects/mybot/assets/locales/{zh-CN,en-US}/`. They are compiled into the AP
image through
`components/mybot/mybot_sdk/platforms/bk725x/modules/storage/mybot_assets.c`.
There is no separate asset partition.

After changing an Ogg file, regenerate the C table and update the matching
snapshot checksum:

```bash
python3 projects/mybot/scripts/generate_assets_c.py \
  projects/mybot/assets \
  components/mybot/mybot_sdk/platforms/bk725x/modules/storage/mybot_assets.c
sha256sum components/mybot/mybot_sdk/platforms/bk725x/modules/storage/mybot_assets.c
```

See [prompt assets](projects/mybot/assets/README.md) for the accepted format,
conversion script, and file roles.

## Verification

Run the source and snapshot checks before a build or review:

```bash
python3 scripts/check_sources.py
python3 -m unittest discover -s scripts -p 'test_*.py'
```

The build script also verifies that each generated AP ELF contains the selected
regional endpoint and only its configured TLS root. No board-level validation
result is stored in this repository; run and record it on the target hardware.

## Documentation and license

- [Official dependencies and snapshots](docs/OFFICIAL_DEPENDENCIES.md)
- [SDK component](components/mybot/mybot_sdk/README.md)
- [BK725x platform port](components/mybot/mybot_sdk/platforms/bk725x/README.md)
- [Prompt assets](projects/mybot/assets/README.md)
- [agent-studio-device-firmware](https://github.com/AgoraIO-Community/agent-studio-device-firmware)

Original content in this repository is licensed under
[Apache License 2.0](LICENSE). The submodules, SDK snapshot, AOSL, Opus, and
Agora RTSA retain their own licenses and terms. Review the applicable files
before distributing a product or firmware image.
