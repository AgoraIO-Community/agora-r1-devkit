# 1. AOSL Library Introduction

The aosl library provides an abstract implementation of low-level common components including
threads, IO, logging, memory, etc. Its public API headers are in [include/api](include/api/).

This directory is the AOSL snapshot used by [agora-r1-devkit](../../../../README.md). Its BK725x
HAL and source provenance are recorded in [snapshots.json](../../snapshots.json); the AOSL licensing
terms remain in [LICENSE](LICENSE). The public [AOSL upstream project](https://github.com/AgoraIO-Community/aosl)
provides information about other platform ports.

# 2. Porting Guide

## 2.1 HAL Layer Definition

The platform directories included in this snapshot are:

- [platform/include/hal](platform/include/hal/): HAL interface definitions.
- [platform/src/bk725x/config/hal](platform/src/bk725x/config/hal/): BK725x HAL configuration.
- [platform/src/bk725x](platform/src/bk725x/): BK725x HAL implementation.

## 2.2 HAL Layer Implementation

Implement the interfaces defined in the headers in `platform/include/hal`. Porting includes
atomic, file, iomp, socket, memory, thread, time, etc. Use the included
[BK725x HAL](platform/src/bk725x/) as the implementation reference for this firmware.

# 3. Build Guide

R1 builds AOSL for the BK7258 AP through the parent
[Armino component wrapper](../CMakeLists.txt). That wrapper selects the preserved source files,
exports `aosl/include`, `aosl/platform/include`, and `aosl/platform/src/bk725x/config`, and declares
the required BK components. Follow the [R1 firmware build instructions](../../../../README.md#build)
for the pinned dependencies and BK toolchain.

The [standalone CMake file](CMakeLists.txt) retains the upstream Linux default, but this snapshot
contains the BK725x HAL. The current R1 integration uses the Armino wrapper described above.

## 3.1 CMake Build Controls

For integrations based on the standalone CMake file, the existing controls are:

| Control | Meaning |
| --- | --- |
| `AOSL_DECLARE_PROJECT` | Declare AOSL as a standalone project; defaults to `ON`. |
| `AOSL_DIR` | AOSL source root; defaults to `CMAKE_CURRENT_SOURCE_DIR`. |
| `CONFIG_PLATFORM` | Select `platform/src/<platform>`; the HAL delivered here is `bk725x`. |
| `CMAKE_TOOLCHAIN_FILE` | Select a toolchain; [cmake/toolchain_template.cmake](cmake/toolchain_template.cmake) is the upstream template. |

[cmake/build.cmake](cmake/build.cmake) defines `AOSL_ADD_OPTIONS`, `AOSL_ADD_DEFINITIONS`,
`AOSL_ADD_INCLUDES_PUBLIC`, `AOSL_ADD_INCLUDES_PRIVATE`, and `AOSL_ADD_SOURCES` for CMake consumers.
These controls describe the standalone build machinery; the R1 wrapper explicitly selects its
source list and compile configuration.

# 4. Build Example

The delivered component integration is [../CMakeLists.txt](../CMakeLists.txt), and its firmware
host is [projects/mybot](../../../../projects/mybot/). After preparing the dependencies and
toolchain according to the R1 README, run this command from the `agora-r1-devkit` repository root:

```bash
python3 scripts/build_all.py
```

The [BK725x SDK platform guide](../../mybot_sdk/platforms/bk725x/README.md) describes platform
registration and the application controller. The [R1 dependency guide](../../../../docs/OFFICIAL_DEPENDENCIES.md)
documents the build integration and validation record.

# Start History
[![Star History Chart](https://api.star-history.com/svg?repos=AgoraIO-Community/aosl&type=date&legend=top-left)](https://www.star-history.com/#AgoraIO-Community/aosl&type=date&legend=top-left)
