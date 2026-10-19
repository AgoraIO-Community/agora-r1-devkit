# Runtime snapshot provenance

`snapshots.json` records these publicly available inputs:

- SDK core: [agent-studio-device-firmware v1.3.0](https://github.com/AgoraIO-Community/agent-studio-device-firmware/tree/v1.3.0),
  commit `32ac74bc59cb72db70e9a68337753561b59b13d0`. The SDK `include/` and `src/`
  match that commit, with the additional `mybot_version.h` generated from its template and
  version variables. `mybot_sdk/SDK_REVISION` records the official source trees and archive hash.
- BK725x platform: the [published R1 baseline](https://github.com/AgoraIO-Community/agora-r1-devkit/tree/0db7814501093d67cd614944996832c018c31f99/components/mybot/mybot_sdk/platforms/bk725x),
  commit `0db7814501093d67cd614944996832c018c31f99`, path
  `components/mybot/mybot_sdk/platforms/bk725x`.
- AOSL and its BK725x HAL: the same published R1 commit, path
  `components/mybot/mybot_aosl/aosl`.
- RTSA header, original library, package metadata and thread adapter: the same published R1
  commit, path `components/mybot/mybot_rtsa`. The prebuilt library is preserved byte-for-byte.

The BK725x platform READMEs and AOSL README describe the delivered files and build entry
points. Their digests match the published R1 baseline above. SDK code, AOSL implementation,
license files and platform configuration are unchanged by the provenance mapping.

Builds use these local snapshots and the official Beken repositories in `vendor.lock.json`.
The CMake wrappers select the BK7258 AOSL sources explicitly, without additional compiler
definitions. Only AP builds these components. `CONFIG_AGORA_IOT_SDK` remains disabled to
prevent a second AOSL or RTSA copy.

## RTSA package and metadata

The runtime package is
`Agora-RTSALite-SRmRcAcAj-arm-rtos-gnu-1021-bk7258-v1.10.1-20260911_153542-1292388`,
as identified by the package string embedded in its ARM code. The original `PACKAGE_INFO`
and `readme.txt` describe the older 20260824 build 1264541; `CHANGELOG.md` also predates the
runtime package. These metadata files remain verbatim, and `snapshots.json` records this
discrepancy. The original distribution archive checksum is unknown.

The header SHA-256 is
`29b1c21cbe416ee703eaa197ce623469619b1ed68cd3e9c66cb3abe2fafec7db`.
The original library in the published R1 snapshot has SHA-256
`fb3d08126e14cf2eb99c531390822a591b094537d54abf8df95c5a5f587be8ec`.

The prebuilt library, including its build and debug metadata, remains unchanged. Both header
and library hashes are checked during CMake configuration. `python3 scripts/check_sources.py`
verifies all managed snapshot files after the pinned Beken submodules are initialized.
