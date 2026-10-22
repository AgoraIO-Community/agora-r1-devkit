# mybot-bk7258

**简体中文 | [English](README.md)**

这是 [MyBot](https://github.com/junlon2006/mybot) AI 语音 SDK 的 BK7258 固件
参考实现。仓库组合了固定版本的 Beken SDK 子模块、本地 MyBot SDK 快照、
BK725x 平台适配以及用于构建 AP、CP 镜像的固件工程。

本项目当前是开发与移植基线。量产前仍需完成目标板适配、安全审查、硬件验证
和第三方许可核查。设备服务、Agora 云服务和云端 AI Agent 均为外部依赖。

## 仓库结构

| 路径 | 内容 |
| --- | --- |
| `bk_avdk_smp/` | 固定版本的 Beken AVDK 子模块 |
| `bk_solution_ai/` | 固定版本的 Beken Solution 子模块 |
| `projects/mybot/` | BK7258 AP/CP 工程、板级配置、分区表和提示音资源 |
| `components/mybot/mybot_sdk/` | MyBot SDK 快照和 `platforms/bk725x` 平台适配 |
| `components/mybot/mybot_aosl/` | AOSL 运行时快照和 BK725x HAL |
| `components/mybot/mybot_rtsa/` | Agora RTSA 头文件和 ARM 库 |
| `patches/` | 构建生成目录时应用的 AVDK 补丁 |
| `scripts/` | 源码准备、构建和校验脚本 |

BK7258 固件采用 AP/CP 分工。AP 入口初始化媒体服务并启动 MyBot controller；
controller 负责 APSTA 配网、STA 连接、凭据持久化以及 MyBot 启停。配对、
会话状态、音频流和 RTC 会话由 SDK 通过已注册的 BK725x 平台描述符处理。

## 环境要求

- 支持 submodule 的 Git。
- Python 3、GNU Make 和 Beken 构建环境。
- Armino 构建所需的 `click_option_group`。
- 板级与外设配置相匹配的 BK7258 开发板。
- 可用的 MyBot 设备服务和 Agora 服务配置。

## 获取源码

```bash
git clone --recurse-submodules https://github.com/junlon2006/mybot-bk7258.git
cd mybot-bk7258
```

已有顶层仓库时执行：

```bash
git submodule sync --recursive
git submodule update --init --recursive
```

日常构建不要使用 `git submodule update --remote`。依赖版本由已提交的
gitlink 决定，`.gitmodules` 中的分支名不能替代 gitlink。精确提交、补丁和
本地快照来源见[官方依赖说明](docs/OFFICIAL_DEPENDENCIES.md)。

## 构建

在仓库根目录构建中英文两个版本：

```bash
python3 scripts/build_all.py
```

单独构建一种语言：

```bash
python3 scripts/build_all.py --language zh-CN
python3 scripts/build_all.py --language en-US
```

脚本会创建隔离的构建目录，应用 AVDK 补丁，在生成的工程副本中选择一种语言，
并检查 AP 配置和固件。成功后写出以下未跟踪的构建产物：

```text
releases/bk7258-zh-CN.bin
releases/bk7258-en-US.bin
```

其他参数见 `python3 scripts/build_all.py --help`。如果只需准备一个变体并查看
Make 命令而不编译：

```bash
python3 scripts/prepare_vendor.py --language en-US
```

生成工程还会在 `build/bk7258/mybot/` 下产生 `all-app.bin`、`app_pack.rbl`、
AP/CP 独立镜像和 `build_summary.txt`。这些均属于构建产物，不在本仓库中跟踪。

## 烧录与运行

使用 Beken 烧录工具写入生成的 `releases/bk7258-<language>.bin`。该镜像复制自
构建产生的完整 `all-app.bin`。

参考流程：

1. 没有已保存的 Wi-Fi 凭据时，设备进入 APSTA 配网模式并创建 `mybot-xxx`
   SoftAP。
2. 连接该热点，打开 `http://192.168.4.1/` 并提交 Wi-Fi 凭据。
3. STA 获得可用 IPv4 地址后，controller 启动 MyBot SDK，进行设备配对和认证。
4. 设备完成认领后，可通过以下按键控制音量和会话。

参考板按键：

| GPIO | 输入 | 功能 |
| --- | --- | --- |
| GPIO13 | 短按 | 增大音量 |
| GPIO8 | 短按 | 减小音量 |
| GPIO12 | 短按 | 开始或结束会话 |
| GPIO12 | 长按约 3 秒 | 停止 MyBot 并重新进入配网 |

GPIO、显示屏和音频连接属于板级配置；更换硬件时需要同步修改移植代码并重新验证。

## 配置

主要配置和布局文件：

```text
projects/mybot/ap/config/bk7258_ap/config
projects/mybot/cp/config/bk7258/config
projects/mybot/partitions/bk7258/auto_partitions.csv
projects/mybot/partitions/bk7258/ram_regions.csv
```

MyBot Kconfig 必须且只能选择一种语言：

| 配置 | 设备服务地址 | TLS 根证书 |
| --- | --- | --- |
| `CONFIG_MYBOT_LANGUAGE_ZH_CN` | `https://botstation.shengwang.cn/api` | DigiCert Global Root G2 |
| `CONFIG_MYBOT_LANGUAGE_EN_US` | `https://botstation.agora.io/api` | Go Daddy Root Certificate Authority - G2 |

`CONFIG_MYBOT_DEBUG_CPU` 用于启用周期性 CPU 诊断线程，默认关闭。源码工程当前
启用 `en-US`；构建脚本只修改生成工程副本中的语言选择。

## 提示音资源

仓库在 `projects/mybot/assets/locales/{zh-CN,en-US}/` 下包含 26 个
Opus-in-Ogg 提示音，通过
`components/mybot/mybot_sdk/platforms/bk725x/modules/storage/mybot_assets.c`
编入 AP 镜像，不使用独立资源分区。

修改 Ogg 后需要重新生成 C 数组并更新快照校验值：

```bash
python3 projects/mybot/scripts/generate_assets_c.py \
  projects/mybot/assets \
  components/mybot/mybot_sdk/platforms/bk725x/modules/storage/mybot_assets.c
sha256sum components/mybot/mybot_sdk/platforms/bk725x/modules/storage/mybot_assets.c
```

格式、转换脚本及各文件用途见[提示音说明](projects/mybot/assets/README.md)。

## 校验

构建或审查前执行：

```bash
python3 scripts/check_sources.py
python3 -m unittest discover -s scripts -p 'test_*.py'
```

构建脚本还会检查每个生成的 AP ELF 是否只包含所选区域的服务地址和 TLS 根证书。
本仓库没有保存板端测试结果；需要在目标硬件上执行并记录。

## 文档与许可

- [官方依赖与快照](docs/OFFICIAL_DEPENDENCIES.md)
- [MyBot SDK 组件](components/mybot/mybot_sdk/README.md)
- [BK725x 平台适配](components/mybot/mybot_sdk/platforms/bk725x/README.md)
- [提示音资源](projects/mybot/assets/README.md)
- [MyBot 上游项目](https://github.com/junlon2006/mybot)

本仓库原创内容采用 [Apache License 2.0](LICENSE)。子模块、MyBot 快照、
AOSL、Opus 和 Agora RTSA 分别适用其自身许可和条款；分发产品或固件前请核对
对应文件。
