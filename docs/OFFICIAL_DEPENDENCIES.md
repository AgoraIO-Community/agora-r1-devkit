# BK7258 official dependencies

The BK7258 firmware uses official Beken submodules and keeps product code in this repository.
The BK725x platform and AOSL files match the published
[R1 snapshot at `0db7814`](https://github.com/AgoraIO-Community/agora-r1-devkit/tree/0db7814501093d67cd614944996832c018c31f99/components/mybot).
The RTSA library is preserved byte-for-byte from that fixed snapshot, as described in
[snapshot provenance](../components/mybot/SNAPSHOTS.md).

The SDK `include/` and `src/` match official
[agent-studio-device-firmware v1.3.0](https://github.com/AgoraIO-Community/agent-studio-device-firmware/tree/v1.3.0),
commit `32ac74bc59cb72db70e9a68337753561b59b13d0`. Its official ref, source trees and
archive checksum are recorded in `components/mybot/mybot_sdk/SDK_REVISION`.

## Fixed SDK baselines

| Dependency | Official repository | Commit |
| --- | --- | --- |
| `bk_avdk_smp` | <https://github.com/bekencorp/bk_avdk_smp> | `d2ded037798530175e5dc5cde6fa1878f5d5ef35` |
| `bk_solution_ai` | <https://github.com/bekencorp/bk_solution_ai> | `5e3ad9e818d3a3962eb45db23fd71b61238db5af` |
| SDK core | <https://github.com/AgoraIO-Community/agent-studio-device-firmware> | `32ac74bc59cb72db70e9a68337753561b59b13d0` (`v1.3.0`) |

The Beken commits pin the platform dependencies; the SDK core is locked independently to its
official release. The top-level gitlinks pin the Beken commits; routine setup uses
`git submodule update --init --recursive`, without `--remote`.

`vendor.lock.json` records the official URLs, commits, patch order, and patch SHA-256 checksums.
Preparation checks it against `.gitmodules`, the checked-out revisions, and tracked working-tree
changes before creating a build workspace.

## Local product code and runtime snapshots

| Path | Content |
| --- | --- |
| `projects/mybot/` | AP/CP entry points, board configuration, Flash/RAM layouts, OGG assets, and asset-generation scripts |
| `components/mybot/mybot_sdk/` | agent-studio-device-firmware SDK 1.3.0 snapshot, BK725x adapters and modules, and embedded prompt arrays |
| `components/mybot/mybot_aosl/` | BK7258 AOSL runtime and platform adaptation from the published R1 baseline |
| `components/mybot/mybot_rtsa/` | BK7258 RTSA header and unchanged library, build `1292388`, from the published R1 baseline |

The integration uses the BK7259 repository's approach of separating official dependencies from
product code. It retains the BK7258 runtime versions, board configuration, and AP/CP architecture.
The BK7259 RTSA binary is not substituted. Runtime snapshots retain their licenses and must be
reviewed separately when updated.

The local component wrappers build for `bk7258_ap` only. AOSL keeps its original 48-source
selection without additional compiler definitions. `CONFIG_AGORA_IOT_SDK` stays disabled so the
official component does not introduce a second AOSL or RTSA copy.

`components/mybot/snapshots.json` records the preserved source and binary checksums.
`python3 scripts/check_sources.py` verifies these snapshots and the official dependency state.

The RTSA library identifies its package as
`Agora-RTSALite-SRmRcAcAj-arm-rtos-gnu-1021-bk7258-v1.10.1-20260911_153542-1292388`.
The upstream `readme.txt` and `PACKAGE_INFO` retain older build `1264541` metadata, and the
`CHANGELOG.md` also predates this package. They are preserved verbatim. Use the binary identity
and manifest checksums to identify the actual runtime inputs:

| Input | Size | SHA-256 |
| --- | --- | --- |
| `mybot_rtsa/include/agora_rtc_api.h` | — | `29b1c21cbe416ee703eaa197ce623469619b1ed68cd3e9c66cb3abe2fafec7db` |
| `mybot_rtsa/lib/arm/libagora-rtc-sdk.a` | 9,283,998 bytes | `fb3d08126e14cf2eb99c531390822a591b094537d54abf8df95c5a5f587be8ec` |

The product project and components live in this repository. `bk_solution_ai` needs no local
patches because its existing solution components are used as delivered.

## SDK patches

The preparation script applies the following AVDK patches to generated SDK sources:

| Patch | Purpose |
| --- | --- |
| [0001-captive-portal-dns.patch](../patches/bk_avdk_smp/0001-captive-portal-dns.patch) | Preserve captive-portal DNS handling used during Wi-Fi provisioning |
| [0002-spi-display-completion.patch](../patches/bk_avdk_smp/0002-spi-display-completion.patch) | Preserve SPI display transfer completion handling used by the reference displays |

No `bk_solution_ai` patches are applied. Product components are integrated through the local
project's component directories rather than committed inside an official submodule. Preparation
keeps the official submodules' tracked files clean; generated SDK/project copies and build output
are local artifacts.

## Preparing and building

From the repository root:

```bash
git submodule sync --recursive
git submodule update --init --recursive
python3 scripts/build_all.py
```

The build defaults to `--language both`. Single-language builds use `--language zh-CN` or
`--language en-US`. The complete flash images are written to:

- `releases/bk7258-zh-CN.bin`
- `releases/bk7258-en-US.bin`

To prepare sources and inspect the Make command before compiling:

```bash
python3 scripts/prepare_vendor.py --language en-US
```

Language-specific configuration is applied to generated project copies. The source
`projects/mybot/ap/config/bk7258_ap/config` keeps its existing English selection. Building both
languages does not rewrite that file. These scripts provide language selection only; they do not
add camera or video variants.

| Language | Device-service base URL | TLS root certificate |
| --- | --- | --- |
| `zh-CN` | `https://botstation.shengwang.cn/api` | DigiCert Global Root G2 |
| `en-US` | `https://botstation.agora.io/api` | Go Daddy Root Certificate Authority - G2 |

The base URL includes `/api` without a trailing slash. Each variant includes its region's root
certificate only.

Prompt assets remain part of the AP firmware. After changing an OGG file, regenerate the source
arrays before preparing a new build:

```bash
python3 projects/mybot/scripts/generate_assets_c.py \
  projects/mybot/assets \
  components/mybot/mybot_sdk/platforms/bk725x/modules/storage/mybot_assets.c
```

Intentional source or asset changes require reviewing the source differences, including the
generated `mybot_assets.c`, and updating the matching SHA-256 entries in
`components/mybot/snapshots.json`. Calculate the hashes with `sha256sum` after review. Builds
verify the manifest and do not silently regenerate its checksums.

## Updating dependencies

Treat an SDK version update as a separate change. Select exact official commits, verify that the
patches still apply, check the local component integration and runtime versions, and build both
languages. Validate provisioning, saved Wi-Fi reconnection, prompts, displays, buttons, and RTC
audio on the reference board before changing the pinned baseline.

Product changes belong in the root project and components. Keep necessary SDK changes in small
patches under `patches/`, with a clear reason for each change. Do not commit changes directly in
the official submodules or follow upstream branches automatically during a build.

Update `vendor.lock.json` alongside intentional baseline or patch changes. Changes to managed
runtime snapshot inputs also require an intentional update of `components/mybot/snapshots.json`.
Review source and binary differences before accepting new checksums.

## Validation

The SDK mapping, all 256 snapshot checksums, and 30 host tests are checked locally.
The SDK code and prebuilt RTSA library remain unchanged.

The following firmware build results were recorded on 2026-10-19 for the same SDK and
unchanged RTSA library. The provenance update does not rerun the complete firmware build:

- All 256 source and binary snapshot checksums passed, together with 30 host tests.
- The upstream `mybot_http_client_test`, `mybot_json_test`, and
  `mybot_public_headers_test` passed, including the runtime SDK version check.
- A fresh `python3 scripts/build_all.py` completed both AP/CP builds and complete firmware
  packages: Chinese in 39 seconds, English in 39 seconds, total elapsed time 1 minute 19 seconds.
- The generated AP configuration and firmware were checked for the selected language, matching
  regional service endpoint, and exactly one corresponding TLS trust anchor: DigiCert Global
  Root G2 for Chinese and Go Daddy Root Certificate Authority - G2 for English.
- Both official submodules retained clean tracked working trees, and the source project's
  English configuration remained unchanged.

| Complete flash image | SHA-256 |
| --- | --- |
| `releases/bk7258-zh-CN.bin` | `266a9c780829fb8b4b173d2a6d860dac6687654bb1a032bc366bae8b1c2cce32` |
| `releases/bk7258-en-US.bin` | `b92ccc0a09c65b1b40293fec20a617aa05b8462c8a3eca585a16e3c67ec386aa` |

Board testing has not been performed for this SDK refresh.
