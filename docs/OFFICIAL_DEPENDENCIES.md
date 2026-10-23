# Official dependencies and local snapshots

The firmware build uses two pinned official Beken submodules and local snapshots
for agent-studio-device-firmware. Product code is kept in the top-level repository.

## Pinned submodules

| Dependency | Repository | Commit |
| --- | --- | --- |
| `bk_avdk_smp` | <https://github.com/bekencorp/bk_avdk_smp> | `d2ded037798530175e5dc5cde6fa1878f5d5ef35` |
| `bk_solution_ai` | <https://github.com/bekencorp/bk_solution_ai> | `5e3ad9e818d3a3962eb45db23fd71b61238db5af` |

The top-level gitlinks are authoritative. `vendor.lock.json` repeats the
repository URLs, commits, patch list, and patch checksums so
`scripts/prepare_vendor.py` can reject a mismatched or dirty dependency tree.

## Local build inputs

| Path | Source recorded in `components/mybot/snapshots.json` |
| --- | --- |
| `projects/mybot/` | Repository-owned BK7258 project |
| `components/mybot/mybot_sdk/` | agent-studio-device-firmware 1.3.0 plus BK725x integration |
| `components/mybot/mybot_aosl/` | AOSL and BK725x HAL snapshot |
| `components/mybot/mybot_rtsa/` | Agora RTSA header, ARM library, and thread adapter |

`components/mybot/mybot_sdk/SDK_REVISION` records the upstream SDK source
commit and tree digests. `components/mybot/snapshots.json` records the checksum
of every managed SDK, AOSL, RTSA, and platform-port file. These local files are
the build inputs; the build does not read an external SDK source checkout.

The recorded runtime provenance includes:

| Input | Source |
| --- | --- |
| SDK core | `AgoraIO-Community/agent-studio-device-firmware@46e265a960fac48d93ae6755bdf694aeb09ab366`, `main` |
| BK725x platform baseline | `AgoraIO-Community/agora-r1-devkit@6f77b3853ca9a13fd2597e80ee9c494ab8c11812` |
| AOSL HAL and RTSA inputs | `AgoraIO-Community/agora-r1-devkit@0db7814501093d67cd614944996832c018c31f99` |

Those source records describe provenance and pinned content. They are not additional build
dependencies and are not used by `scripts/build_all.py`.

The RTSA library identifies itself as:

```text
Agora-RTSALite-SRmRcAcAj-arm-rtos-gnu-1021-bk7258-v1.10.1-20260911_153542-1292388
```

The copied `PACKAGE_INFO`, `readme.txt`, and `CHANGELOG.md` predate that build
and are preserved as upstream metadata. The current source archive checksum is
not recorded. The library and public-header checksums are in
`components/mybot/snapshots.json` and are checked during CMake configuration.

## Patches

| Patch | Purpose |
| --- | --- |
| `patches/bk_avdk_smp/0001-captive-portal-dns.patch` | Preserve captive-portal DNS handling used by Wi-Fi provisioning |
| `patches/bk_avdk_smp/0002-spi-display-completion.patch` | Preserve SPI display completion handling used by the displays |

There are no `bk_solution_ai` patches. The official submodules remain clean;
patches are applied only to generated build copies.

## Prepare and build

```bash
git submodule sync --recursive
git submodule update --init --recursive
python3 scripts/build_all.py
```

Use `--language zh-CN` or `--language en-US` for a single firmware. To create
one build tree without compiling:

```bash
python3 scripts/prepare_vendor.py --language en-US
```

The script prints the Make command for the generated project. `--dry-run`
checks the source state without creating build files.

The local component wrappers build only for `bk7258_ap`.
`CONFIG_AGORA_IOT_SDK` remains disabled so that the official component does not
add a second AOSL or RTSA copy.

## Updating dependencies

Treat a dependency update as a separate change:

1. Select exact official commits and update the gitlinks.
2. Check that every patch still applies.
3. Review local platform and SDK snapshot differences.
4. Update `vendor.lock.json` and, when managed files change,
   `components/mybot/snapshots.json`.
5. Build both languages and run target-hardware validation.

Do not commit generated SDK trees, `build/`, or `releases/`. Verify changes with:

```bash
python3 scripts/check_sources.py
python3 -m unittest discover -s scripts -p 'test_*.py'
```

The source check verifies both submodule commits and clean tracked trees, then
checks every managed snapshot file against its recorded SHA-256. Board-level
validation is not stored in this repository.
