# E1003 SD 卡照片轮播（路线 A）

基于 Seeed_GFX 官方示例 `reTerminal_E1003_SDcard_Gray16` 改写，保留原有的
「解码 → 抖动 → pushImage → 全屏刷新」流水线，增加了目录扫描和定时轮播。

## 文件

| 文件 | 作用 |
|---|---|
| `E1003_SDcard_Slideshow.ino` | 轮播主程序（配置区在文件上半部分） |
| `upload_mode.h/.cpp` | Wi-Fi 传图模式（AP + 网页上传，见下文） |
| `dither.cpp/.h` | 16 级灰度抖动算法（从示例原样拷入） |
| `image_loader.cpp/.h` | JPEG / BMP / PNG 解码（从示例原样拷入） |
| `pngle.*` + `miniz.*` | PNG 解码后端（从示例原样拷入） |
| `driver.h` | 选中 Setup522（ED103TC2 / IT8951，1872×1404） |

示例是自包含的，不需要额外安装库；但 `TFT_eSPI.h` 仍由 **Seeed_GFX** 提供。
注意不要和 `Seeed_GFX2` 的示例混用。

## 使用

1. **准备照片**：用 `tools/prepare_photos.py` 批量缩放（默认 1100×825）
   ```powershell
   python tools\prepare_photos.py D:\Photos D:\out
   ```
2. 把输出文件夹整个拷到 SD 卡，命名为 **`/img`**
3. 插卡，上电（或按 RESET）
4. 串口监视器打开系统识别到的 USB-UART 端口，**115200**（日志走 Serial1 / GPIO43-44）

## 可调参数（.ino 顶部 USER CONFIGURATION）

| 参数 | 默认 | 说明 |
|---|---|---|
| `IMAGE_DIR` | `/img` | 扫描目录；不存在时回退到卡根目录 |
| `SLIDE_INTERVAL_MS` | `30000` | 每张停留时间，建议 ≥15000 |
| `MAX_FILES` | `128` | 最多记住多少张 |
| `DITHER_METHOD` | `DITHER_FS` | `FS / BAYER8 / NONE / JARVIS / ATKINSON` |
| `DITHER_GAMMA` | `1.0` | >1 变暗，<1 变亮 |
| `DISPLAY_FIT` | `FIT_CONTAIN` | 超出面板时缩小，不放大 |
| `DISPLAY_ANCHOR` | `ANCHOR_CENTER` | 九宫格定位 |

## 按键翻页

E1003 前面三个按键（低电平有效）：

| 按键 | GPIO | 默认功能 |
|---|---|---|
| KEY0（右键） | 3 | 长按 2 秒 → **Wi-Fi 传图模式**（短按无动作） |
| KEY1（中键） | 4 | **上一页** |
| KEY2（左键） | 5 | **下一页** |

- 手动翻页后会重置 30 秒自动轮播计时，然后继续自动播放
- 400 ms 去抖/锁定，按住不会连续翻页
- 翻页在**按下时**触发，响应即时；右键没有翻页功能，所以长按不会误翻页
- 换图需要完整走一遍解码 → 抖动 → 刷新，约 4–5 秒；刷新期间的按键会被忽略
- 想交换方向，改 `PIN_KEY_NEXT` / `PIN_KEY_PREV` 即可
- 想换进入传图模式的按键，改 `PIN_KEY_UPLOAD` 即可（默认 `PIN_KEY0`）

## Wi-Fi 传图（不用拔卡）

E1003 做不了 USB 读卡器：USB-C 口接的是 **CH340K USB-UART 桥**，而 ESP32-S3 的原生
USB 引脚 GPIO19/20 被板载 SHT4x 温湿度传感器的 I²C 占用了。所以改用 Wi-Fi：

1. **长按右键 2 秒**（或上电时按住右键；卡里一张图都没有时也会自动进入）
2. 屏幕显示 SSID、密码、网址，同时进入 AP 模式：
   - SSID `E1003-Photo`，密码 `e1003photo`，地址 `http://192.168.4.1`
3. 电脑连上这个热点，浏览器打开 `http://192.168.4.1`
4. 把照片拖进页面 → 直接写进 SD 卡的 `/img`，文件名自动续号 `011_xxx.jpg`
5. 传完 **长按右键 2 秒**退出，板子重启并开始轮播；10 分钟无操作也会自动退出

页面会顺手读取每张图的尺寸，算出 PSRAM 峰值，超标的直接标红
`⚠ 会被跳过`——不用等轮播放不出来才发现。

**「填满屏幕」**（默认勾选）：按面板比例居中裁剪后再上传，画面铺满整屏；
不勾则保留完整画面，四周留白。E1003 面板是 1872×1404（4:3），上限 1170×876。

**手机拍照直接传就行**：网页会在手机/电脑上先把照片缩到板子允许的最大 4:3 框
（默认 FS 抖动时约 1170×876），再上传，所以不需要改手机相机设置。缩放用浏览器的
canvas 完成，板子这边不参与。如果照片本来就已经调好尺寸，可以勾选
「上传原图」跳过这一步。iPhone 的 HEIC 如果浏览器解不了会提示先转 JPEG
（从相册选图时 iOS 通常会自动转成 JPEG）。

可调参数（`.ino` 顶部）：

| 参数 | 默认 | 说明 |
|---|---|---|
| `AP_SSID` | `E1003-Photo` | 热点名 |
| `AP_PASSWORD` | `e1003photo` | 8–63 字符；写成 `""` 就是开放热点 |
| `UPLOAD_HOLD_MS` | `2000` | 长按多久进入传图模式 |

`upload_mode.cpp` 顶部的 `kIdleExitMs`（默认 10 分钟）控制空闲退出，`kMaxUploadBytes`
（24 MB）是单张硬上限。

传输速度实测取决于 Wi-Fi 环境，一般 0.5–1.5 MB/s；一张 300 KB 的照片不到 1 秒。
上传过程中不会刷新墨水屏（一次全刷要 2.2 秒），整批传完后才更新一次状态页。

> 注意：文件名会转成 ASCII（中文名变成 `photo`），排序靠 `NNN_` 前缀，不影响播放顺序。

### 按键提示音

板载蜂鸣器在 **GPIO45**。按下按键时立即发声，用来确认“已经收到、正在翻页”，
不会等 4 秒的屏幕刷新：

| 操作 | 音调 | 时长 |
|---|---|---|
| 左键（下一页） | 2000 Hz | 45 ms |
| 中键（上一页） | 1200 Hz | 45 ms |

- `BUZZER_FEEDBACK = false` 可整体关闭
- `BEEP_NEXT_HZ` / `BEEP_PREV_HZ` / `BEEP_MS` 可调音调和时长
- `tone()` 由 ESP32 核心的 LEDC 任务异步播放，不阻塞刷新流程

## 分辨率上限（8 MB PSRAM）

峰值内存 ≈ `1.32 MB + 6 × 宽 × 高`（FS / Jarvis / Atkinson）
峰值内存 ≈ `1.32 MB + 4 × 宽 × 高`（BAYER8 / NONE）

| 抖动 | 建议最大源图 |
|---|---|
| `DITHER_FS` | ~1100 × 825 |
| `DITHER_BAYER8` | ~1450 × 1080 |

超过上限时，RGB 解码会 OOM（跳过该图）；如果只是误差扩散缓冲分配失败，
`dither.cpp` 会**静默退化到无抖动**并打 warning。日志里出现
`error-diffusion buffer ... alloc FAILED` 就说明照片还是太大了。

## 已验证

- `arduino-cli compile --fqbn "esp32:esp32:XIAO_ESP32S3:FlashSize=8M,PartitionScheme=default_8MB,PSRAM=opi"` 通过
- 实机烧录后启动日志正常：PSRAM 8192 kB、SD 挂载成功、Gray16 sprite 正常
- 实机 10 张图：1100×824 源图，FS 抖动 955 ms，全屏刷新 2249 ms
- 按键固件已烧录，按键事件会打印 `[key] NEXT` / `[key] PREV`
- 实机验证：左键（GPIO5）下一页、中键（GPIO4）上一页，首尾环绕正常
- 提示音已确认可听见（下一页 2000 Hz / 上一页 1200 Hz）

## 已知行为

- 电子纸每张一次 GC16 全屏刷新（约 1–3 s），不适合 <5 s 的动画式切换
- `epaper.update()` 结束时会自动让面板 sleep，下一张 `update()` 自动唤醒
- 运行中拔卡不会自动重新挂载；重新插卡后请按 RESET
- 坏图/超大图会被跳过，5 秒后自动尝试下一张
- 传图模式下网页服务是单连接、阻塞式：一次只处理一个上传，传大图时按键响应会慢一点
- 传图模式和轮播不会同时跑；退出传图模式靠重启，所以新照片下次开机就绪