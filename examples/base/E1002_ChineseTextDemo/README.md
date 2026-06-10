# E1002 Chinese Text Demo

这个示例只验证一件事：reTerminal E1002 能不能从 Flash 文件系统加载 TTF 字体，并把 UTF-8 中文画到 800x480 电子纸屏幕上。

它不会录音、联网、解析备忘录，也不会修改 `VoiceMemoReminder`。

## Files

| File | Purpose |
| --- | --- |
| `E1002_ChineseTextDemo.ino` | Arduino 入口程序。初始化屏幕、挂载 SPIFFS、加载字体、绘制中文。 |
| `driver.h` | 固定 `BOARD_SCREEN_COMBO 521`，让 Seeed_GFX 使用 reTerminal E1002 屏幕驱动。 |
| `partitions.csv` | 本示例专用 8 MB Flash 分区表，给 SPIFFS 预留约 4.9 MB。 |
| `data/font_cn.ttf` | 中文 TTF 字体，上传到 SPIFFS 后运行时通过 `/font_cn.ttf` 加载。 |

## Panel Notes

- Panel: reTerminal E1002
- Controller: ED2208
- Resolution: 800x480 landscape
- Color mode: 6-color ePaper; this demo draws black text on a white background

## Dependencies

需要先安装这些 Arduino 库：

- `Seeed_GFX`
- `OpenFontRender`：Arduino Library Manager 搜不到这个库时，从 GitHub 安装 `https://github.com/takkaO/OpenFontRender`

当前字体文件约 2.2 MB，XIAO ESP32S3 默认 8 MB 分区里的 1.5 MB SPIFFS 放不下。本示例自带 `partitions.csv`，Arduino-ESP32 会在编译时优先使用 sketch 目录里的这个分区表，把 SPIFFS 扩到约 4.9 MB。

## Arduino IDE Settings

- Board: `XIAO ESP32S3`
- PSRAM: `OPI PSRAM`
- Flash Size: `8MB`
- Partition Scheme: 保持默认即可；本目录的 `partitions.csv` 会覆盖默认 1.5 MB SPIFFS

## Upload Order

1. 打开 `E1002_ChineseTextDemo.ino`。
2. 确认 `partitions.csv` 和 `.ino` 在同一个目录。
3. 先正常编译并烧录这个 sketch。编译时 Arduino-ESP32 会使用本目录的 `partitions.csv`。
4. 用下面的命令行方法上传 `data/font_cn.ttf` 到 SPIFFS。
5. 通过 reTerminal UART bridge 查看 `Serial1` 日志，波特率 `115200`，GPIO43/GPIO44。

## Upload Font To SPIFFS

不要直接使用 Arduino IDE 里的 `SPIFFS Filesystem Uploader`，如果它显示：

```text
Using partition: default_8MB
```

它实际会创建 1.5 MB 的 SPIFFS 镜像，`font_cn.ttf` 约 2.2 MB，必然失败：

```text
SPIFFS_write error(-10001): File system is full.
```

本示例的 `partitions.csv` 里 SPIFFS 分区是：

```text
spiffs, data, spiffs, 0x310000, 0x4E0000
```

所以要手动生成一个 `0x4E0000` 大小的 SPIFFS 镜像，并写入 Flash 的 `0x310000` 偏移。

### 1. 确认串口

```sh
arduino-cli board list
```

找到 reTerminal 对应的串口。常见形式类似：

```text
/dev/cu.wchusbserial130
```

下面命令里的 `/dev/cu.wchusbserial130` 请替换成你自己的端口。

### 2. 生成 SPIFFS 镜像

```sh
mkdir -p /private/tmp/e1002-chinese-spiffs

/Users/mengdu/Library/Arduino15/packages/esp32/tools/mkspiffs/0.2.3/mkspiffs \
  -c /Users/mengdu/Desktop/OSHW-reTerminal-Series-E-D/examples/base/E1002_ChineseTextDemo/data \
  -p 256 \
  -b 4096 \
  -s 0x4E0000 \
  /private/tmp/e1002-chinese-spiffs/e1002_chinese.spiffs.bin
```

成功时会看到：

```text
/font_cn.ttf
```

### 3. 写入 SPIFFS 分区

优先用稳定的 `115200` 波特率：

```sh
/Users/mengdu/Library/Arduino15/packages/esp32/tools/esptool_py/5.1.0/esptool \
  --chip esp32s3 \
  --port /dev/cu.wchusbserial130 \
  --baud 115200 \
  --before default-reset \
  --after hard-reset \
  write-flash \
  -z \
  --flash-mode keep \
  --flash-freq keep \
  --flash-size keep \
  0x310000 \
  /private/tmp/e1002-chinese-spiffs/e1002_chinese.spiffs.bin
```

成功时会看到类似：

```text
Wrote 5111808 bytes (...) at 0x00310000
Hash of data verified.
```

如果你之后在 Arduino IDE 里只重新上传程序，一般不会擦掉 SPIFFS。如果打开了“上传前擦除全部 Flash”之类的选项，字体会被擦掉，需要重新执行本节命令。

## Expected Result

屏幕会显示这些测试文字：

- `中文显示测试`
- `你好，世界。`
- `备忘录：下午三点取快递。`
- `混合文本：E1002 + OpenFontRender`

如果屏幕上的中文清晰可读，就说明 TTF 加载和中文绘制链路可用，可以继续复制 `VoiceMemoReminder` 做中文版。

## Failure Checks

- 屏幕显示 `SPIFFS mount failed`：检查分区是否是大 SPIFFS，并确认已经上传 data。
- 屏幕显示 `Font file missing`：检查 `data/font_cn.ttf` 是否上传到了 SPIFFS 根目录，运行时路径应为 `/font_cn.ttf`。
- 屏幕显示 `Font load failed`：检查是否安装了 `OpenFontRender`，以及字体文件是否完整。
