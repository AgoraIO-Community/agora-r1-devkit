# BK7258 official dependencies

The BK7258 firmware uses official Beken submodules and keeps product code in the top-level
repository. Building no longer requires the `junlon2006/bk_avdk_smp` or
`junlon2006/bk_solution_ai` forks.

The migration starts from the updated BK7258 main revision
`04c828300b1a80cfc6e8cca70e98e2cf1d14b99c`. Its product source has been refreshed from the pinned
solution revision `438f132542bf3f0bb2f79aee49154d83cd6fdaf9`, preserving mybot SDK 1.2.0,
the audio lifecycle, device identity implementation, LCD status badges, and regional service
configuration.

The MyBot SDK `include/` and `src/` snapshot is subsequently updated directly from upstream
`3f9a1add55f1b946d59655b5b88f4662d780fb31` (version 1.3.0). Its exact source revision and
archive checksum are recorded in `components/mybot/mybot_sdk/SDK_REVISION`.
The Chinese and English pairing prompt/digit PCM assets from that revision are converted
to Opus-in-Ogg with the project's existing settings and embedded into the AP firmware.

The AVDK integration and local AOSL/RTSA inputs preserve the exact pinned source revision
`a9529f1224747604192b46668471d3473d80050c`. The official bases, local snapshots, and patches
reproduce these product inputs without depending on either SDK fork during a build.

## Fixed SDK baselines

| Dependency | Official repository | Commit |
| --- | --- | --- |
| `bk_avdk_smp` | <https://github.com/bekencorp/bk_avdk_smp> | `d2ded037798530175e5dc5cde6fa1878f5d5ef35` |
| `bk_solution_ai` | <https://github.com/bekencorp/bk_solution_ai> | `5e3ad9e818d3a3962eb45db23fd71b61238db5af` |

These are the official bases of the pinned BK7258 source snapshots. Moving to official
repositories changes build integration without upgrading the SDK. The top-level gitlinks pin
the commits; routine setup uses `git submodule update --init --recursive`, without `--remote`.

`vendor.lock.json` records the official URLs, commits, patch order, and patch SHA-256 checksums.
Preparation checks it against `.gitmodules`, the checked-out revisions, and tracked working-tree
changes before creating a build workspace.

## Local product code and runtime snapshots

| Path | Content |
| --- | --- |
| `projects/mybot/` | AP/CP entry points, board configuration, Flash/RAM layouts, OGG assets, and asset-generation scripts |
| `components/mybot/mybot_sdk/` | mybot SDK 1.3.0 snapshot, BK725x adapters and modules, and embedded prompt arrays |
| `components/mybot/mybot_aosl/` | BK7258 AOSL runtime snapshot and platform adaptation from the pinned AVDK source |
| `components/mybot/mybot_rtsa/` | BK7258 Agora RTSA headers and library, build `1292388`, from the pinned AVDK source |

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

The old `bk_solution_ai` fork added the mybot project and component without modifying existing
official solution components. Those additions now live above, so `bk_solution_ai` needs no local
patches.

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

Validated on 2026-10-21 after refreshing MyBot SDK to `3f9a1ad` (version 1.3.0):

- All 256 source and binary snapshot checksums passed, together with 30 host tests.
- The SDK `include/` and `src/` trees are unchanged from the preceding SDK snapshot and match
  the selected upstream commit byte-for-byte, apart from the generated version header.
- All 22 updated pairing audio files passed Opus header, mono channel, pre-skip and final
  granule checks. Decoding with ffmpeg to 16 kHz mono s16 produced exactly the source PCM
  sample counts. The generated C arrays match all 26 project Ogg assets (76,900 bytes total).
- A fresh `python3 scripts/build_all.py` completed both AP/CP builds and complete firmware
  packages: Chinese in 43 seconds, English in 40 seconds, total elapsed time 1 minute 24 seconds.
- The generated AP configuration and firmware were checked for the selected language, matching
  regional service endpoint, and exactly one corresponding TLS trust anchor: DigiCert Global
  Root G2 for Chinese and Go Daddy Root Certificate Authority - G2 for English.
- Both official submodules retained clean tracked working trees, and the source project's
  English configuration remained unchanged.

| Complete flash image | SHA-256 |
| --- | --- |
| `releases/bk7258-zh-CN.bin` | `68671662dfb15ffcc92465a438c2a01629f3335f9abff0699480f377d3928c4e` |
| `releases/bk7258-en-US.bin` | `f2379f81e0eca043dce6313fe394eca33ea758dd76b5b47454dd6ced1dfb8680` |

Board testing has not been performed for this SDK refresh.
