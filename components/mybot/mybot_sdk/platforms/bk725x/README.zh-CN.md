# BK725x 平台移植

[English](README.md) | [简体中文](README.zh-CN.md)

本目录包含 `agent-studio-device-firmware` SDK 的 BK725x 平台实现，负责将 SDK 的公开平台接口适配到 Beken
BK725x SDK，供基于 BK Armino 的固件工程集成使用。

完整的 BK7258 固件工程维护在当前
[agora-r1-devkit 仓库](../../../../../README.zh-CN.md)中。

[`projects/mybot`](../../../../../projects/mybot/) 包含 AP/CP 启动代码、板级配置、分区表和
内嵌语音资源。BK 应用控制器位于 [`modules/controller`](modules/controller/)。

## 功能范围

该平台移植提供以下 BK725x 实现：

- Wi-Fi 配网、连接状态监控和凭据持久化；
- 基于 BK mbedTLS 集成的 HTTPS 传输；
- 16 kHz 单声道 PCM 音频采集、播放、设备音量和共享音频电源管理；
- OGG/Opus 提示音解码和配对码播报；
- LCD 渲染、语义按键事件和 BK 按键输入；
- 基于 BK 环境存储的键值存储。

`adapter/` 包含将这些实现连接到 SDK `ops` 接口的注册适配器。可复用的 BK 模块按功能
分布在 `modules/audio`、`modules/network`、`modules/display`、`modules/button`、
`modules/key` 和 `modules/storage` 目录中。

控制器和 SD 卡/USB MSC 流程属于应用集成层代码，分别位于
[`modules/controller`](modules/controller/) 和 [`modules/storage`](modules/storage/)。
AP/CP 入口、板级配置、分区表以及产品资源位于
[`projects/mybot`](../../../../../projects/mybot/)。这些均不属于平台无关的 SDK 核心。

## 平台注册

在调用 `mybot_start()` 之前，调用一次平台注册入口：

```c
#include "bk725x_platform_adapters.h"

if (bk725x_platform_adapters_register() < 0) {
    /* 中止启动并报告平台初始化失败。 */
}
```

该注册函数会安装 BK HTTPS、KV、Wi-Fi、音频、播报、按键和 LCD 实现。
[`modules/controller`](modules/controller/) 中的应用控制器会在启动流程中完成该注册。

## BK 构建环境

该平台移植由 BK Armino/AVDK 构建系统编译。
固件工程必须提供 BK725x 工具链以及以下平台组件：

```text
bk_rtos, bk_common, bk_uid, bk_wifi, bk_event, bk_netif,
bk_display, psa_mbedtls, lwip, easy_flash,
audio_pipeline, onboard_mic_stream, onboard_speaker_stream,
raw_stream, multimedia, json, bk_vfs, fatfs
```

[`projects/mybot`](../../../../../projects/mybot/) 包含项目入口和板级文件。组件源文件清单以及
`armino_component_register()` 配置位于
[`components/mybot/mybot_sdk/CMakeLists.txt`](../../CMakeLists.txt)。
按照 [R1 构建说明](../../../../../README.zh-CN.md#构建固件)准备固定版本的 BK 依赖，
再使用 BK 头文件、库和工具链编译固件。

## 内嵌提示音

R1 工程会将 [`projects/mybot/assets/locales`](../../../../../projects/mybot/assets/locales/)
下的 OGG 提示音转换为 C 资源表，并放置在
BK 存储模块中。平台提示音和播报代码在播放时读取内嵌 OGG 字节，并将其解码到 PSRAM 中
的 PCM 缓冲区。压缩后的资源字节会成为 AP 固件镜像的一部分，因此会占用 Flash 空间。

新增或替换提示音时，使用
[`projects/mybot/scripts`](../../../../../projects/mybot/scripts/) 中的转换和生成脚本，按照
[资源重新生成说明](../../../../../README.zh-CN.md#内嵌语音资源)审查生成文件及摘要后，
重新构建 BK 固件。生成的资源源码属于具体产品数据，不应脱离 BK 产品配置单独加入 SDK 核心。

## 配置说明

- 当前 R1 工程默认关闭 `CONFIG_USBD_MSC`。USB MSC 和 SD 卡共享访问属于可选产品功能，
  不是 SDK 平台接口的必需项。
- BK 专用的内存分配、RTOS、网络和显示头文件均限制在本目录内。
- 应用代码应使用 SDK 公开 API（包括 `mybot_get_state()` 和语义化会话按键事件），不要
  直接使用 SDK 私有 lifecycle 接口。

完整的工程目录、板级配置、分区表、固件构建命令和设备验证流程，请参阅
[R1 README](../../../../../README.zh-CN.md)。
