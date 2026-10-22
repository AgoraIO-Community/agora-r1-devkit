# agora-r1-devkit 提示音资源

本目录保存提示音源文件。`scripts/generate_assets_c.py` 会把
`locales/**/*.ogg` 生成为只读 C 数组
`components/mybot/mybot_sdk/platforms/bk725x/modules/storage/mybot_assets.c`，
随 AP 固件编译，不需要 SD 卡或独立资源分区。

## 文件与格式

`zh-CN` 和 `en-US` 各包含 13 个文件：

| 文件 | 用途 |
| --- | --- |
| `wificonfig.ogg` | 开始 APSTA 配网 |
| `success.ogg` | 配网成功 |
| `prompt.ogg` | 配对码播报提示 |
| `0.ogg` 至 `9.ogg` | 配对码数字 |

格式为 Opus-in-Ogg，源音频为 16 kHz 单声道 s16le。转换参数由
`scripts/convert_pcm_to_ogg.sh` 固定为 libopus 24 kbps VBR、`voip`
application。设备解码器依赖 OpusHead 中的声道、pre-skip 和最终 granule
position，因此应使用仓库脚本转换。

## 修改和生成

1. 将同名 16 kHz 单声道 s16le PCM 文件放到对应 locale 目录。
2. 运行转换脚本：

   ```bash
   projects/mybot/scripts/convert_pcm_to_ogg.sh projects/mybot/assets
   ```

3. 重新生成 C 数组：

   ```bash
   python3 projects/mybot/scripts/generate_assets_c.py \
     projects/mybot/assets \
     components/mybot/mybot_sdk/platforms/bk725x/modules/storage/mybot_assets.c
   ```

4. 更新修改文件在 `components/mybot/snapshots.json` 中的 SHA-256。
5. 从仓库根目录运行 `python3 scripts/build_all.py`，检查并烧录生成的固件。

新增文件只有在设备代码引用后才会播放。Ogg 字节占用 AP Flash，解码后的 PCM
缓冲区在运行时从 PSRAM 分配；替换较多音频后应检查 AP 固件空间。
