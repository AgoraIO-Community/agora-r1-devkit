# agora-r1-devkit 固件内嵌语音资源

本目录保存 agora-r1-devkit 的提示语音资源。构建前，
`projects/mybot/scripts/generate_assets_c.py` 会把
`locales/` 下的 OGG 文件转换为 C 源文件
`components/mybot/mybot_sdk/platforms/bk725x/modules/storage/mybot_assets.c`；该源文件随 AP 固件一起编译，设备直接从
固件中的只读数组读取并解码语音，不需要在 SD 卡上保存资源文件。

生成器只收集 `locales/**/*.ogg`，本说明文件不会被编译进固件。

## 音频格式

所有音频为 **Opus-in-Ogg**：由 16 kHz 单声道 s16le 源经
`projects/mybot/scripts/convert_pcm_to_ogg.sh` 编码（libopus 24 kbps VBR、`-application voip`），
设备侧由 `mybot_ogg_pcm_bk725x.c` 解码为 16 kHz 单声道 PCM。解码器依赖 OpusHead
（mono、pre_skip 裁剪）与结尾 granule position，因此始终使用脚本转换，不要手工
指定其它编码参数。

`zh-CN` 与 `en-US` 均包含：

- `wificonfig.ogg`：APSTA 配网开始时播放一次（`mybot_prompt_player_bk725x.c`）。
- `success.ogg`：配网成功提示（同上）。
- `prompt.ogg` 与 `0.ogg`~`9.ogg`：播报配对码
  （`mybot_announce_pcm_bk725x.c` 的 `sound_file_name()`）。

## 修改现有语音

以下命令均从仓库根目录执行。

1. 准备 16 kHz 单声道 s16le PCM 源文件，放到对应 locale 目录下同名
   （如 `prompt.pcm`）。
2. 运行 `bash projects/mybot/scripts/convert_pcm_to_ogg.sh`，生成或覆盖同名 `.ogg`。
3. 重新生成 C 数组：

   ```bash
   python3 projects/mybot/scripts/generate_assets_c.py \
     projects/mybot/assets \
     components/mybot/mybot_sdk/platforms/bk725x/modules/storage/mybot_assets.c
   ```

4. 审查生成的 `mybot_assets.c` 差异，再用 `sha256sum` 计算其 SHA-256，更新
   `components/mybot/snapshots.json` 中对应记录。如同时修改了其他受管源码，
   也需审查差异并更新对应记录；构建会校验清单。
5. 按[根 README 的构建说明](../../../README.zh-CN.md#构建固件)重新编译并烧录。
   生成的数组会随 AP 固件更新。

## 新增语音

1. 按上面的步骤生成新的 `.ogg`，并放入对应的 `locales/<lang>/` 目录。
2. 重新运行 `generate_assets_c.py`。只有被设备代码引用的文件才会播放：
   - `wificonfig.ogg` / `success.ogg`：
     `components/mybot/mybot_sdk/platforms/bk725x/modules/audio/mybot_prompt_player_bk725x.c`。
   - `prompt.ogg` / `0.ogg`~`9.ogg`：
     `components/mybot/mybot_sdk/platforms/bk725x/modules/announce/mybot_announce_pcm_bk725x.c`
     的 `sound_file_name()`（对应 `mybot_announce_sound_t` 枚举）。
3. 完成上述源码差异审查及校验清单更新后，按
   [根 README 的构建说明](../../../README.zh-CN.md#构建固件)重新编译并烧录。

## 固件空间

数组保存的是压缩后的 OGG 字节，实际占用 AP 固件的 Flash 空间；解码后的 PCM
缓冲区仍在运行时分配到 PSRAM。新增或替换较多音频后，请检查 AP 固件的 Flash
使用率，确保仍有足够空间。
