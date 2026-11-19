# E1004 SD 卡照片轮播（六色墨水屏）

reTerminal E1004：13.3 英寸 **Spectra 6 六色** 面板，1200 × 1600，黑白绿红黄蓝六种颜色
（没有灰阶），所以照片看起来是"海报/漫画"风格，这是面板本身的特性，不是 bug。

基于 Seeed_GFX 官方示例 `reTerminal_E1004_SDcard_Color6` 改写，保留原有的
「解码 → 六色量化抖动 → pushImage → 全屏刷新」流水线，增加了目录扫描、定时轮播、
按键翻页和 Wi-Fi 传图。

## 文件

| 文件 | 作用 |
|---|---|
| `E1004_SDcard_Slideshow.ino` | 轮播主程序（配置区在文件上半部分） |
| `upload_mode.h/.cpp` | Wi-Fi 传图模式（AP + 网页上传，和 E1003 版共用） |
| `dither.cpp/.h` | 六色量化抖动（从官方示例原样拷入） |
| `image_loader.cpp/.h` | JPEG / BMP / PNG 解码（原样拷入） |
| `pngle.*` + `miniz.*` | PNG 解码后端（原样拷入） |
| `driver.h` | 选中 Setup523（T133A01，1200×1600，六色） |
| `tools/prepare_photos.py` | 电脑端批量缩图 |

## 使用

1. 照片拷到 SD 卡的 **`/img`**（FAT32，≤64 GB），按文件名排序播放
2. 插卡上电
3. 串口监视器打开对应 COM 口，**115200**（日志走 Serial1 / GPIO43-44）

## 按键

| 按键 | GPIO | 功能 |
|---|---|---|
| KEY0（右键） | 3 | 下一页 |
| KEY1（中键） | 4 | 上一页 |
| KEY2（左键） | 5 | 长按 2 秒 → Wi-Fi 传图模式（短按无动作） |

- 翻页在按下瞬间触发并发声，随后约 30 秒刷屏（六色面板刷新慢），期间按键不响应
- 手动翻页后 30 秒自动轮播重新计时

## Wi-Fi 传图

长按左键 2 秒进入：

| 项目 | 值 |
|---|---|
| 热点 | `E1004-Photo` |
| 密码 | `e1004photo` |
| 网址 | `http://192.168.4.1` |

网页会把手机/电脑原图**先缩到板子允许的尺寸**再上传：

| 抖动 | 上传上限（约） |
|---|---|
| `DITHER_YLO`（当前默认，三墨混色） | 1110 × 1482 |
| `DITHER_FS` | 1110 × 1482 |
| `DITHER_BN` | 1110 × 1482 |
| `DITHER_BAYER8` | 1110 × 1482 |

误差扩散现在只用 **2–3 行的环形缓冲（13–20 KB）**，不再按整图分配，所以 FS / Jarvis 也能跑满尺寸。
YLO 会在第一次处理照片时建一张 32³ 的墨水组合查找表（约 98 KB PSRAM，一次性 1–2 秒），之后每张约 2 秒。
上限不是写死的，由 `.ino` 里的 `UPLOAD_BUDGET_BYTES`、抖动算法和面板尺寸实时算出，
改抖动或改预算后网页会自动跟着变。传完长按左键 2 秒退出，板子重启开始轮播；
10 分钟无操作自动退出。

**画幅（这块屏最关键的一项）**：面板是竖幅 3:4，而手机/相机原图多是 4:3 横图。
网页默认勾选 **「填满屏幕」**，会按 1110×1482 的比例居中裁剪再上传，照片铺满整屏；
不勾则保留完整画面，上下会出现大片白边（画面只占 47%）。

电脑端批量处理同理：

```powershell
python tools\prepare_photos.py D:\Photos D:\out --mode cover --max-width 1110 --max-height 1482 --dither bayer8 --sprite-mb 0.92
```

## 内容自适应抖动

六色屏上不是所有画面都适合同一种抖动。开机会对每张图做一次统计，日志里能看到：

```
[dither] content: extreme=0.92 color=0.00 edges=0.03 uniq=81 -> text/UI (no dither)
[dither]   thin strokes would speckle -- using nearest colour
```

判据：绝大部分像素落在黑/白两端、几乎没有彩度、四阶量化后颜色数很少、边缘密集
→ 判定为文字/UI/线稿，**直接用最近色，不抖动**，笔画保持清晰。
照片（颜色数成百上千、彩度高）走原来的 Bayer 抖动，不受影响。

实测样本：网页截图 `0.96 / 0.02 / 538`、扫描文档 `0.80 / 0.04 / 386`、
照片 `0.60 / 0.19 / 1273`。分类本身约 0.4 秒（每张图一次）。

## 可调参数（`.ino` 顶部）

| 参数 | 默认 | 说明 |
|---|---|---|
| `IMAGE_DIR` | `/img` | 扫描目录 |
| `SLIDE_INTERVAL_MS` | `30000` | 每张停留时间 |
| `DITHER_METHOD` | `DITHER_YLO` | YLO = 逐像素最优墨水对混色（色准最好）；还有 FS / BN / BAYER8，都支持满尺寸 |
| `DITHER_GAMMA` | `1.0` | >1 变暗，<1 变亮 |
| `DISPLAY_FIT` | `FIT_CONTAIN` | 超出面板时缩小，不放大 |
| `AP_SSID` / `AP_PASSWORD` | `E1004-Photo` / `e1004photo` | 热点凭据 |
| `UPLOAD_HOLD_MS` | `2000` | 长按多久进传图模式 |
| `UPLOAD_BUDGET_BYTES` | 7.2 MB | 传给网页做尺寸预算 |

## 内存上限（8 MB PSRAM）

峰值 ≈ `0.92 MB + 4 × 宽 × 高`（所有抖动算法；误差扩散的缓冲只有几行）

整屏 1200×1600 = 192 万像素，即便 BAYER8 也要 8.6 MB，**装不下**，所以照片必须先缩小。
网页传图会自动处理；直接拷卡的话用 `tools/prepare_photos.py`：

```powershell
python tools\prepare_photos.py D:\Photos D:\out --max-width 1110 --max-height 1480 --dither bayer8
```

## 编译烧录

```powershell
arduino-cli compile --fqbn "esp32:esp32:XIAO_ESP32S3:FlashSize=8M,PartitionScheme=default_8MB,PSRAM=opi" .
arduino-cli upload  -p COMx --fqbn "esp32:esp32:XIAO_ESP32S3:FlashSize=8M,PartitionScheme=default_8MB,PSRAM=opi" .
```

## 已知行为

- 六色面板只有 6 种颜色，没有中间灰，人脸/天空容易出现色带和颗粒，属正常
- `epaper.update()` 内部会先推 old colors 再推 new colors，结束自动 sleep
- 运行中拔卡不会自动重新挂载，插回后按 RESET
- 坏图/超大图跳过，5 秒后自动尝试下一张
- 卡里没有图片时开机会自动进入 Wi-Fi 传图模式