The managed runtime files in `snapshots.json` retain the following provenance:

* BK7258 MyBot integration: `https://github.com/junlon2006/bk_solution_ai.git`, commit
  `438f132542bf3f0bb2f79aee49154d83cd6fdaf9`, `components/mybot`.
  Its platform code retains this baseline. The SDK `include/` and `src/` snapshots
  are version `1.2.0`, synchronized directly from
  `https://github.com/junlon2006/mybot.git` commit
  `39d5ed122aab51d90c1c531298c4ead8dc35b104`; see `mybot_sdk/SDK_REVISION`.
* AOSL and the BK725x HAL: `https://github.com/junlon2006/bk_avdk_smp.git`, commit
  `a9529f1224747604192b46668471d3473d80050c`,
  `ap/components/bk_thirdparty/agora-iot-sdk/hal/aosl`.
* RTSA library, public header, package metadata and thread adapter: the same
  `bk_avdk_smp` commit, under `ap/components/bk_thirdparty/agora-iot-sdk`.

These URLs identify historical provenance; builds use the local snapshots and
the official BK repositories recorded by the vendor lock. The local CMake
wrappers select the original BK7258 AOSL sources explicitly, with no additional
compiler definitions. Only the AP target builds these components. The official
`CONFIG_AGORA_IOT_SDK` remains disabled to prevent a second AOSL or RTSA copy.

The RTSA package is
`Agora-RTSALite-SRmRcAcAj-arm-rtos-gnu-1021-bk7258-v1.10.1-20260911_153542-1292388`,
as identified by the package string embedded in the library binary. The copied
`PACKAGE_INFO` and `readme.txt` still describe the previous 20260824 build
1264541 and its old hashes; the copied `CHANGELOG.md` also predates this package.
These upstream metadata files are preserved verbatim, with this discrepancy
recorded in `snapshots.json`. The current source archive checksum is unknown.
The current library SHA-256 is
`fb3d08126e14cf2eb99c531390822a591b094537d54abf8df95c5a5f587be8ec`,
and the header SHA-256 is
`29b1c21cbe416ee703eaa197ce623469619b1ed68cd3e9c66cb3abe2fafec7db`.
Its library and public header are also checked at CMake configuration time.
Use `python3 scripts/check_sources.py` to verify every immutable snapshot file.
