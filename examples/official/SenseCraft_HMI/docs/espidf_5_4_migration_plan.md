# ESP-IDF 5.4.0 迁移规划

## 背景

当前工程通过 PlatformIO 构建，应用层使用 Arduino framework，底层实际是 ESP-IDF 4.4.7。ESP-IDF 4.4 release branch 已经 EOL，继续停留在这条线上会让 SD 卡、Wi-Fi、BLE、文件系统、低功耗等问题很难和官方修复、官方示例、官方 issue 对齐。

本规划目标是彻底切换到 ESP-IDF 5.4.0 + CMake 构建体系，不再依赖 PlatformIO、Arduino framework 或 Arduino as component。同时要保证已经烧录旧版完整固件的设备可以通过只烧录 app 固件完成迁移，不要求重新烧 bootloader、partition table 或整片 flash。

已确认的线上前提：

- 当前固件没有 firmware OTA 功能，`app_download` 只负责内容 manifest 和图片下载。
- 旧设备一直运行在 `ota_0/app0`，reTerminal app offset 为 `0x90000`。
- 迁移后的发布默认只写 `ota_0/app0` app bin。

## 第一性原理约束

### 1. 已出厂设备的 bootloader 和分区表不能更新

已经烧录旧固件的设备上，bootloader 在 flash `0x0`，partition table 在 `0x8000`，reTerminal 的 `app0/ota_0` 从 `0x90000` 开始。只烧 app 固件时，新固件必须被旧 bootloader 识别并启动。

迁移验证不能只测 `idf.py flash` 的完整烧录结果，必须验证：

- 旧 PlatformIO/Arduino 4.4 bootloader + 旧 partition table + 新 ESP-IDF 5.4 app。
- 空 `otadata` 时默认启动 `ota_0`。
- `otadata -> ota_0` 时覆盖 `0x90000` 后启动新 app。
- 重启、深睡唤醒、断电重启后都仍从 `ota_0` 启动。

发布工具必须把 app-only 和 full-flash 严格分开。当前 PlatformIO 生成的 full-flash 列表包含：

| Offset | Image |
| --- | --- |
| `0x0000` | `bootloader.bin` |
| `0x8000` | `partitions.bin` |
| `0xe000` | `boot_app0.bin` |
| `0x90000` | `firmware.bin` |

其中 `0xe000` 落在 reTerminal 的 NVS 区间 `0x9000..0x86000` 内。迁移发布脚本不得复用这个 full-flash 配方，否则会写坏 NVS。对旧设备的迁移发布只能写 app offset。

### 2. 分区表是线上数据契约

保持分区表不动不仅是为了启动，还为了保留 NVS、LittleFS 内容、coredump 空间和 OTA slot 语义。

当前 reTerminal 分区表由 `partitions_reterminal.csv` 生成后实际布局为：

| Name | Type | SubType | Offset | Size |
| --- | --- | --- | --- | --- |
| `nvs` | data | nvs | `0x9000` | 500K |
| `otadata` | data | ota | `0x86000` | 8K |
| `phy_init` | data | phy | `0x88000` | 4K |
| `app0` | app | ota_0 | `0x90000` | 12M |
| `app1` | app | ota_1 | `0xC90000` | 12M |
| `spiffs` | data | spiffs | `0x1890000` | 6M |
| `coredump` | data | coredump | `0x1E90000` | 64K |

`spiffs` 这个名字和 subtype 不能随便改。当前工程实际用 `mklittlefs` 生成 LittleFS 镜像，并在运行时用 Arduino `LittleFS.begin(true)` 挂载。迁移后要继续把 LittleFS 挂到同一个 partition label 或同一个 data partition 上，不能改成真正 SPIFFS，否则旧设备里的文件无法兼容读取。

XIAO DIY 也要保持已有表：

- `partitions_diykit.csv`：factory 单 app，`factory` offset `0x90000`，size 3M。
- `partitions_diykit_new.csv`：双 OTA，`app0` offset `0x90000`，`app1` offset `0x590000`。

### 3. 直接迁移到 ESP-IDF API

这次迁移不做 Arduino 兼容阶段。所有核心边界直接切到 ESP-IDF 或普通 C/C++ API：

- 入口：`setup()/loop()` -> `app_main()`。
- 时间：`millis()/delay()` -> `esp_timer_get_time()` + `vTaskDelay()`。
- 日志：`ArduinoLog` -> `ESP_LOGx`。
- NVS：`Preferences` -> `nvs_flash` + `nvs_*`。
- LittleFS：Arduino `LittleFS` -> IDF component `esp_littlefs` 或等价可控组件。
- SD：Arduino `SD/File/SPIClass` -> `sdmmc/sdspi` + FatFs + POSIX/stdio。
- Wi-Fi：`WiFi` -> `esp_wifi` + `esp_netif` + `esp_event`。
- HTTP client：`HTTPClient/WiFiClientSecure` -> `esp_http_client`。
- Web server：`WebServer` -> `esp_http_server`。
- DNS：`DNSServer` -> 独立轻量 DNS server。
- MQTT：继续使用 IDF `esp-mqtt`，但按 ESP-IDF 5.x config 结构重写。
- BLE：`NimBLE-Arduino` -> ESP-IDF NimBLE host API。
- GPIO/I2C/SPI/ADC/LEDC：Arduino API -> IDF driver。

可以保留业务模块的领域接口，比如 `HAL::GetHAL()`、`Board`、`MiniRT::run()`，但这些接口内部不能继续包 Arduino API。接口保留是为了降低业务状态机改动，不是为了延续 Arduino 运行时。

## 迁移目标

### 必须满足

- 使用 ESP-IDF 5.4.0 工具链和 CMake 构建。
- 不使用 PlatformIO 作为正式构建和发布入口。
- 不链接 Arduino framework 或 Arduino as component。
- 生成的 app bin 可直接烧录到旧分区表 `ota_0/app0` offset。
- 不要求用户擦除整片 flash。
- 不更新旧设备 bootloader。
- 不更新旧设备 partition table。
- 保留 NVS namespace/key/type 语义。
- 保留旧 LittleFS 内容和文件路径。
- reTerminal E1001/E1002/E1003/E1004、XIAO DIY 的板级选择能力仍可用。
- SD 卡读写改到 IDF 原生 SDSPI/FatFs 体系，并保留当前的插卡检测、电源控制、共享 SPI 总线策略。
- 能输出 coredump 到现有 `coredump` partition。

### 不作为本次迁移目标

- 改分区表。
- 启用 secure boot/flash encryption/anti-rollback。
- 引入 bootloader OTA。
- 引入 firmware OTA。
- 大规模重写业务状态机。
- 为了兼容旧代码再造一套厚 Arduino facade。

## 当前工程事实

### 构建配置

`platformio.ini` 当前环境全部是 Arduino framework。reTerminal 环境关键信息：

- `board = reterminal`
- `framework = arduino`
- `board_build.f_cpu = 240000000L`
- `board_build.f_flash = 80000000L`
- `board_build.flash_mode = qio`
- `board_build.partitions = partitions_reterminal.csv`
- `board_upload.offset_address = 0x90000`
- `BOARD_HAS_PSRAM`
- `memory_type = qio_opi`
- `psram_type = opi`
- `upload.flash_size = 32MB`

当前 `reterminal_e1001` 构建产物约 1.9MB，远小于 12MB app slot。

### 代码结构

入口：`src/main.cpp`。

板级层：

- `src/boards/common/*`
- `src/boards/reterminal_e1001/*`
- `src/boards/reterminal_e1003/*`
- `src/boards/reterminal_e1004/*`
- `src/boards/xiao_diy/*`
- `src/boards/board_registry.h`

业务层：

- `src/APP/app_wifi.cpp`
- `src/APP/app_sensecraft.cpp`
- `src/APP/app_download.cpp`
- `src/APP/app_device_info.cpp`
- `src/APP/app_view.cpp`
- `src/APP/app_ble.cpp`
- `src/APP/app_atcmd.cpp`

本地库：

- `lib/minirt`
- `lib/GT911`
- `lib/SY6974`
- `lib/epd_decoder`
- `lib/ArduinoLog`
- `lib/qrcodegen`

外部 Arduino 库需要替换或重写依赖边界：

- `Adafruit_SHT4X`
- `Makuna/Rtc`
- `OneButton`
- `NimBLE-Arduino`
- `ESP32Ping`
- `U8g2_for_TFT_eSPI`
- `Seeed_GFX`
- `pngle`
- `OpenFontRender`

## 必要修改和重构

### 1. 建立 ESP-IDF 工程骨架

新增：

- 根目录 `CMakeLists.txt`
- `main/CMakeLists.txt` 或把现有 `src` 注册为主 component
- `sdkconfig.defaults`
- `sdkconfig.defaults.reterminal`
- `sdkconfig.defaults.xiao_diy`
- `components/` 目录，用于本地第三方库和重写后的硬件组件
- `idf_component.yml`，用于声明可由 IDF Component Manager 管理的依赖

不要手写或提交 `managed_components/`。它应由 IDF Component Manager 根据 `idf_component.yml` 生成。

根 CMake 需要指定：

- `set(EXTRA_COMPONENT_DIRS ...)`
- `include($ENV{IDF_PATH}/tools/cmake/project.cmake)`
- `project(seeed_reterminal_e10xx)`

主 component 要显式列出源码，不依赖 PlatformIO LDF 自动扫描。这里不要写复杂生成逻辑，先按目录聚合，保证可读可控。

### 2. 固定 sdkconfig 里的硬件和兼容参数

reTerminal 必须固定：

- target：`esp32s3`
- flash size：32MB
- flash mode：QIO
- flash frequency：80MHz
- PSRAM：OPI PSRAM enabled
- partition table：custom CSV，`partitions_reterminal.csv`
- partition table offset：`0x8000`
- app offset 由 partition table 决定，保持 `0x90000`
- FreeRTOS tick：1000Hz，便于保持旧业务调度节奏
- coredump：flash backend，使用 `coredump` partition
- USB CDC：保持旧行为

XIAO DIY 根据环境固定 16MB 或 8MB flash，并使用对应分区表。

`sdkconfig.defaults` 不应只靠 `menuconfig` 人工生成。发布前要能从 clean tree 稳定复现同一组关键配置。

### 3. 保留 app-only 烧录产物

构建产物必须明确区分：

- `bootloader.bin`
- `partition-table.bin`
- `app.bin`
- `merged.bin`

迁移验证和发布默认使用 `app.bin`。对于 reTerminal E100x，基于已确认“旧设备没有 OTA、一直运行 `ota_0`”的前提，app-only 烧录命令为：

```bash
esptool.py --chip esp32s3 --baud 460800 write_flash 0x90000 build/seeed_reterminal_e10xx.bin
```

发布脚本必须做这些保护：

- 默认产物只包含 app-only bin。
- full bin 只能用于空板、研发或产线完整烧录，文件名和 manifest 必须显式标记 `full`。
- 旧设备迁移命令不得写 `0x0000`、`0x8000`、`0xe000`。
- app-only manifest 必须写明 `offset = 0x90000`、partition profile、IDF version、git commit、sha256、app size。
- app size 必须小于目标 app partition size。

如果未来新增 firmware OTA 或存在 `ota_1` 设备，发布流程必须重新设计，不得沿用只写 `0x90000` 的假设。

### 4. 替换入口和运行时

从 Arduino 入口迁移到 IDF 入口：

- 新增 `extern "C" void app_main(void)`。
- 删除 `setup()`/`loop()` 入口。
- `MiniRT::run()` 可以继续作为主循环，但主循环放在 `app_main()` 里。

建议目标形态：

```cpp
extern "C" void app_main(void)
{
    system_init();
    app_init_all();

    while (true) {
        MiniRT::run();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
```

`system_init()` 只做 IDF 基础设施初始化，不塞业务逻辑。至少包括：

- `nvs_flash_init()`，遇到 `ESP_ERR_NVS_NO_FREE_PAGES` 或 `ESP_ERR_NVS_NEW_VERSION_FOUND` 时不能默认 erase，必须进入错误路径或显式恢复路径。
- `esp_netif_init()`。
- `esp_event_loop_create_default()`。
- 必要的 TCP/IP、Wi-Fi、BLE、timer 基础设施初始化。
- coredump 配置验证。

### 5. 替换基础 Arduino 类型和工具函数

这一步不是建立兼容层，而是把 Arduino 依赖从代码里拆掉：

- `String`：业务边界改为 `std::string`、`std::string_view`、固定 buffer 或明确长度的 `char[]`。
- `F()`/`PSTR()`：日志字符串直接使用普通字符串字面量。
- `millis()`：改为 `esp_timer_get_time() / 1000` 或模块内单调时间函数。
- `delay()`：改为 `vTaskDelay(pdMS_TO_TICKS(ms))`。
- `File`：改为 POSIX fd 或 `FILE*`。
- `pinMode/digitalWrite/digitalRead`：改为 `gpio_config/gpio_set_level/gpio_get_level`。
- `analogReadMilliVolts`：改为 `esp_adc/adc_oneshot` + calibration。
- `ledcSetup/ledcAttachPin/ledcWriteTone`：改为 IDF LEDC timer/channel API。

原则是直接替换到模块内最近的边界，不再把 Arduino API 包成新的同名 API。

### 6. SD 卡改为 IDF 原生路径

这是迁移的重点之一，因为当前诉求之一是 SD 卡读写不稳定且难排查。

当前 SD 逻辑在 `src/boards/common/sd_card.cpp`，依赖 Arduino `SD`、`SPIClass`、`fs::File`，并实现：

- SD 电源控制。
- 插卡检测。
- SPI 频率重试。
- 与显示 SPI 总线共享。
- 写探测文件。
- `open/exists/remove/totalBytes/usedBytes/cardSize`。

迁移目标：

- 使用 IDF `sdmmc` + `sdspi` + `esp_vfs_fat_sdspi_mount()`。
- 明确 `spi_host_device_t`、CS/MOSI/MISO/SCK。
- SPI bus ownership 放到 board/HAL 层，bus 只初始化一次。
- 显示屏和 SD 共 SPI 时，两者都只向同一个 bus 注册 device。
- mount/unmount 生命周期由板级 `SdCard` 管理。
- 所有读写使用 POSIX `open/read/write/fsync/close` 或 `FILE*`，不再使用 Arduino `File`。
- 写入图片时必须 `fsync` 或等价 flush，失败时保留错误码。
- 记录 `esp_err_t`、FatFs 返回码、卡类型、容量、频率、挂载耗时。

当前代码会在 SD 初始化重试中调用 `spi_->end()` 和 `spi_->begin()`。迁移时不能把这个行为照搬到 IDF，否则会破坏显示共享 SPI。应先明确 bus/device 关系，再迁移 SD。

### 7. LittleFS 保持旧数据兼容

当前分区 label 是 `spiffs`，subtype 是 `data/spiffs`，实际文件系统是 LittleFS。迁移后要做一层明确映射：

- partition label 仍为 `spiffs`。
- 不改 partition subtype。
- mount path 可用 `/littlefs` 或 `/data`，但业务路径仍保持 `/manifest.json`、图片文件名等语义。
- 不自动格式化。旧代码 `LittleFS.begin(true)` 会在 mount 失败时格式化，迁移后不能照搬。
- 对 manifest 和图片文件保留原路径和文件名。

这是 Phase 1/2 的阻塞项，不是后期优化项。必须先证明 ESP-IDF 5.4 使用的 LittleFS component 能挂载旧分区：

- ESP-IDF 5.4 本身不应假设内置 `esp_littlefs`，需要通过 `idf_component.yml` 固定依赖版本，或把组件 vendor 到 `components/`。
- 验证 `esp_vfs_littlefs_register()` 是否只按 label 查找，能否接受 subtype 为 `spiffs` 的 partition。
- 如果组件强依赖 `data/littlefs` subtype，必须在不改分区表的前提下用 partition API 找到 label 后挂载，或维护一个很小的 mount adapter。
- 在真实旧设备上验证旧 `/manifest.json` 和图片能读。
- 挂载失败时进入降级模式并输出错误，不擦除。

### 8. NVS 保持 key 和 namespace 兼容

当前 `app_device_info.cpp` 用 Arduino `Preferences` 保存 Wi-Fi、设备状态、图片版本、album 版本、设备模式等。迁移后使用原生 NVS API，但 namespace、key、类型、默认值不能变。

Namespace：

- `hmi_config`

已知 key 和类型：

| Key | Type | Default |
| --- | --- | --- |
| `isPortal` | bool/u8 | `true` |
| `deviceMode` | bool/u8 | `false` |
| `devBound` | int/i32 | `0` |
| `iotVer` | string | `"0.0"` |
| `imgVer` | string | `"0.0"` |
| `img_count` | int/i32 | `0` |
| `isimg` | bool/u8 | `false` |
| `imgCurrentIdx` | int/i32 | `0` |
| `albumVersion` | string | `"0.0"` |
| `sleepInterval` | uint/u32 | `1800` |
| `disableSleep` | uint/u32 | `0` |
| `devToken` | string | `""` |
| `wifi_ssid_0..4` | string | `""` |
| `wifi_pass_0..4` | string | `""` |

迁移任务：

- 写一个 NVS dump 工具，在旧固件和新固件上读取同一台设备比对。
- 用 `nvs_get_u8/i32/u32/str` 对齐 Arduino `Preferences` 的实际存储类型。
- 不能在迁移时顺手清理 key 或改名。
- 清空 NVS 这类恢复行为必须保留为用户明确触发，不能作为初始化失败的默认路径。

### 9. Wi-Fi、HTTP、DNS 和 MQTT 迁到 IDF 原生

当前 `app_wifi.cpp` 同时承担 STA、AP、配网 WebServer、DNS captive portal 和连接重试。迁移建议按功能拆清边界，但不要做厚包装：

- `wifi_station`：STA 连接、扫描、事件。
- `wifi_softap`：AP 模式。
- `provision_http`：配网页面和接口。
- `dns_captive`：DNS 劫持。

替换目标：

- `WiFi` -> `esp_wifi` + `esp_netif`
- `HTTPClient/WiFiClientSecure` -> `esp_http_client`
- `WebServer` -> `esp_http_server`
- `DNSServer` -> 轻量 DNS server 或 IDF 示例改造
- `ESP32Ping` -> `esp_ping`

MQTT 当前已经使用 `esp_mqtt_client_*`，但配置结构是 ESP-IDF 4.4 风格。ESP-IDF 5.x 需要重写 `esp_mqtt_client_config_t`：

- broker host/port/transport 放到 `broker.address.*`。
- CA 证书放到 `broker.verification.certificate`。
- client id、username、password 放到 `credentials.*`。
- keepalive、clean session、reconnect 策略按 5.x 字段重新设置。

MQTT 迁移不是“换库”，而是修正 IDF 5.x API 断点，并保持 topic、client id、TLS CA、重连策略和事件处理行为不变。

### 10. BLE 迁移

当前 `app_ble.cpp` 使用 `NimBLE-Arduino`。本次迁移直接改到 ESP-IDF NimBLE host API。

必须保持兼容：

- 广播 name。
- Service UUID。
- Characteristic UUID。
- 读写权限。
- notify/indicate 行为。
- 手机 app 或产测工具看到的协议行为。

### 11. 显示、触摸和外设迁移

显示链路依赖 `TFT_eSPI`、`Seeed_GFX`、`OpenFontRender`、`U8g2_for_TFT_eSPI`。既然不再保留 Arduino 运行时，就需要把显示边界改成可在 IDF 下构建的实现：

- 优先评估是否可用 ESP-IDF `esp_lcd` 承接屏幕总线和面板。
- 如果继续保留第三方 C/C++ 显示库，必须移除其 Arduino `SPI/Wire/GPIO/delay` 依赖。
- 图片解析继续使用现有 `epd_decoder/pngle`，但文件接口改为 POSIX/stdio。
- 字体渲染库如果强依赖 Arduino 类型，需要单独拆掉依赖或替换。

触摸 GT911、PMIC、SHT40、RTC 都是 I2C 设备：

- I2C bus ownership 放到 board/HAL。
- 设备读写改到 IDF `i2c_master`。
- GT911 wakeup 继续使用 RTC GPIO 和 ext wake。

外设：

- Button/LED/Buzzer 改 IDF GPIO/LEDC。
- Battery ADC 改 `esp_adc/adc_oneshot` 和 calibration。
- 低功耗唤醒路径使用 IDF sleep API 明确配置。

## 推荐迁移阶段

### Phase 0：冻结旧固件基线

目标：证明新旧差异来自迁移，而不是旧工程状态不清。

任务：

- 保存当前 PlatformIO 构建日志、包版本、bootloader.bin、partitions.bin、firmware.bin。
- 用 `gen_esp32part.py` 反解旧 `partitions.bin`，归档到 release notes。
- 记录每个板型的 app offset 和 flash size。
- 记录 reTerminal 旧设备 active slot 为 `ota_0` 的证据。
- 导出 NVS 样本，包含 Wi-Fi、设备模式、图片版本、album 版本。
- 备份旧 LittleFS 样本，至少包含 `/manifest.json` 和一张图片。
- 准备 SD 卡稳定性复现 case。

通过标准：

- 当前 `reterminal_e1001/e1002/e1003/e1004/xiao_diy_kit` 都能在旧工程构建。
- 至少一台旧设备能通过只写旧 `firmware.bin` 到 `0x90000` 正常启动，作为 app-only 对照组。
- 明确记录旧 full-flash 配方中 `boot_app0.bin @ 0xe000` 不可用于旧设备迁移。

### Phase 1：建立纯 ESP-IDF 构建骨架

目标：不用 PlatformIO、不链接 Arduino，能生成 ESP-IDF 5.4 app bin。

任务：

- 新增 CMake 工程。
- 新增 sdkconfig defaults。
- 新增 `idf_component.yml` 并锁定外部 component 版本。
- 把 `src`、`lib`、外部依赖纳入 IDF components。
- 保持 `BOARD_*`、`BOARD_SCREEN_COMBO` 编译宏输入方式。
- 先只支持 `reterminal_e1001`，但 CMake 结构要能扩展到其他板型。
- 先完成基础类型替换：去掉 `Arduino.h`、`String`、`delay`、`millis`、`File` 的核心编译依赖。

通过标准：

- `idf.py set-target esp32s3`
- `idf.py build`
- 生成 app bin。
- app size 小于当前 app partition。
- 链接产物中没有 Arduino framework/component。

### Phase 2：基础设施和旧数据兼容

目标：在真实旧设备数据上证明新 app 不破坏 NVS 和 LittleFS。

任务：

- 实现 `app_main()` 和 `system_init()`。
- 初始化 `nvs_flash`、`esp_netif`、`esp_event`。
- NVS 使用原生 API 读取旧 namespace/key/type。
- LittleFS component 挂载旧 `spiffs` label。
- 禁止 mount fail 自动格式化。
- coredump 指向旧 `coredump` partition。

通过标准：

- 新固件能读取旧 NVS 样本。
- 新固件能读取旧 `/manifest.json` 和图片。
- NVS/LittleFS 挂载失败不会 erase。
- `esp_app_desc` 版本可读。
- coredump partition 未被误挂载或擦除。

### Phase 3：旧 bootloader + 新 app 启动验证

目标：证明只烧 app 的迁移路径成立。

任务：

- 取一台完整烧录旧固件的设备。
- 不擦 flash，不烧 bootloader，不烧 partition table。
- 只写新 app 到 `0x90000`。
- 观察 boot log，确认旧 bootloader 选中 `ota_0` 并进入新固件。
- 验证 NVS 可读、LittleFS 可读、MAC/版本/板型上报正常。

通过标准：

- 重启 20 次无 boot failure。
- 深睡唤醒后仍能启动。
- `otadata` 不被错误清空或写坏。
- 断电重启后仍启动新 app。

### Phase 4：存储层迁移

目标：解决 SD 和本地文件系统的可排查性。

任务：

- LittleFS 路径使用 POSIX/stdio 文件接口。
- SD 使用 `sdspi` + `esp_vfs_fat_sdspi_mount()`。
- 保留插卡检测、电源控制、频率重试。
- SPI bus ownership 放到 board/HAL。
- 写文件后显式 flush/fsync。
- 所有失败返回明确错误码。

通过标准：

- manifest 下载到 LittleFS。
- 图片下载到 LittleFS。
- 插 SD 时图片下载到 SD。
- SD 热插拔或掉电重试不会破坏 SPI 总线。
- 旧 LittleFS 里的 `/manifest.json` 和图片可被新固件读取。

### Phase 5：网络层迁移

目标：把 Wi-Fi/HTTP/Web/DNS/MQTT 全部跑在 IDF API 上。

任务：

- `esp_wifi` 管理 STA/AP。
- `esp_event` 统一网络事件。
- `esp_http_client` 替换 HTTP 下载和绑定请求。
- `esp_http_server` 替换配网 WebServer。
- DNS captive portal 独立成小模块。
- TLS CA 继续使用当前 root certificate。
- MQTT config 改为 ESP-IDF 5.x 结构。

通过标准：

- 首次配网成功。
- 已保存 Wi-Fi 自动连接。
- bind API 成功。
- MQTT 连接成功。
- manifest/image 下载成功。
- 网络断开重连行为不弱于旧固件。

### Phase 6：外设、BLE 和显示收口

目标：去掉剩余 Arduino 硬件库依赖。

任务：

- BLE 改 IDF NimBLE。
- 显示链路改为 IDF 可构建实现。
- I2C 设备改 IDF `i2c_master`。
- Button/LED/Buzzer 改 IDF GPIO/LEDC。
- Battery ADC 改 `esp_adc/adc_oneshot` 和 calibration。
- 图片解析继续使用现有 `epd_decoder/pngle`，只替换文件接口。

通过标准：

- BLE 配网/通信行为和旧 app 一致。
- 所有屏幕组合能显示启动页、激活页、图片页。
- 按键唤醒、触摸唤醒、timer wakeup 正常。
- 电量读数和旧固件误差在可接受范围。
- PMIC/RTC/SHT40 可读。
- E1001/E1002/E1003/E1004/XIAO DIY 全部构建通过。

### Phase 7：发布工具和回归矩阵

目标：让迁移后的发布流程不会误烧整包或误改分区。

任务：

- 新增 `scripts/build_idf_release.py`。
- 默认只生成或只暴露 app-only 发布产物。
- full bin 仅用于研发/产线完整烧录，必须和 app-only 分目录输出。
- 生成 manifest，记录 app offset、sha256、idf version、partition profile。
- 新增脚本校验 app size 不超过分区。
- 新增脚本反解 partition table 并和基线比对。
- 新增脚本扫描 flash args，拒绝旧设备迁移命令写 `0x0000/0x8000/0xe000`。
- 新增产测命令模板。

通过标准：

- CI 或本地脚本能批量构建所有板型。
- 任一产物都能追溯到分区表、IDF 版本、git commit。
- 发布目录中 app-only 和 full 明确区分。
- 旧设备迁移脚本只写 `0x90000`。

## app-only 迁移验证矩阵

### reTerminal 32MB 双 OTA

| Case | 初始状态 | 操作 | 期望 |
| --- | --- | --- | --- |
| R1 | 旧完整固件，空 `otadata` | 写新 app 到 `0x90000` | 启动新 app |
| R2 | 旧完整固件，`otadata -> ota_0` | 写新 app 到 `0x90000` | 启动新 app |
| R3 | 旧完整固件，保留 NVS | 写新 app 到 `0x90000` | Wi-Fi/NVS 配置保留 |
| R4 | 旧完整固件，LittleFS 有 manifest 和图片 | 写新 app 到 `0x90000` | 文件可读，不格式化 |
| R5 | 旧完整固件，插 SD | 写新 app 到 `0x90000` | SD mount/read/write 稳定 |
| R6 | 新 app 启动后深睡 | timer/button/touch 唤醒 | 唤醒原因正确 |
| R7 | 新 app 断电重启 | 重新上电 | 仍启动新 app |

`ota_1` 不是当前线上路径。若未来新增 firmware OTA，必须新增 `ota_1` case，不能复用本迁移假设。

### XIAO DIY

| Case | 分区表 | 操作 | 期望 |
| --- | --- | --- | --- |
| X1 | `partitions_diykit.csv` factory | 写新 app 到 `0x90000` | factory app 启动 |
| X2 | `partitions_diykit_new.csv` ota_0 | 写新 app 到 `0x90000` | ota_0 app 启动 |

## 风险和处理

### 风险：误用 full-flash 破坏旧设备数据

处理：

- 发布默认只输出 app-only 迁移包。
- 旧设备迁移脚本拒绝写 `0x0000/0x8000/0xe000`。
- full bin 只允许用于空板、研发或产线完整烧录。

### 风险：旧 bootloader 能启动，但硬件配置不同导致运行不稳定

处理：

- sdkconfig 固定 flash/PSRAM 参数。
- 用旧 bootloader 反复验证，而不是只测 `idf.py flash`。
- 保存新 app image header 信息。

### 风险：LittleFS 无法挂载旧 `spiffs` subtype

处理：

- 把 LittleFS 旧分区挂载验证提前到 Phase 1/2。
- 固定 LittleFS component 版本。
- 如组件强依赖 subtype，则维护最小 mount adapter。
- 禁止默认 format on fail。

### 风险：SD 与显示共享 SPI 总线冲突

处理：

- SPI bus 只初始化一次。
- SD 和显示各自只注册 device。
- 总线锁放在 board/HAL 层。
- 所有 SD 操作输出 `esp_err_t` 和 FatFs 错误。

### 风险：NVS 类型或 key 变化导致配置丢失

处理：

- 迁移前导出 NVS 样本。
- 新旧固件读取结果逐项比对。
- 不改 namespace/key/type。
- 初始化失败不自动 erase NVS。

### 风险：直接纯 IDF 迁移改动面大

处理：

- 按系统边界推进：build/runtime、NVS/LittleFS、SD、Network、Peripheral、BLE/display。
- 每个边界迁完必须在真实设备上验证。
- 不写厚兼容层，不把 Arduino API 换皮成新 API。

## 不建议做的事

- 不要修改分区表来“顺手整理”。
- 不要把 `spiffs` partition 改名。
- 不要在第一版迁移里启用 secure boot、flash encryption 或 anti-rollback。
- 不要默认擦除 NVS 或 LittleFS。
- 不要继续依赖 PlatformIO LDF 自动扫描库。
- 不要链接 Arduino framework 或 Arduino as component。
- 不要把 Arduino API 再包装成一层很厚的新框架。
- 不要把 `managed_components/` 当成需要手写维护的源码目录。

## 参考资料

- ESP-IDF Bootloader Compatibility: https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-guides/bootloader.html#bootloader-compatibility
- ESP-IDF Partition Tables: https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-guides/partition-tables.html
- ESP-IDF OTA: https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/system/ota.html
- ESP-IDF FatFs and SD cards: https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/storage/fatfs.html
- ESP-IDF v4.4 EOL advisory: https://documentation.espressif.com/AR2024-008%20End-of-Life%20Advisory%20for%20ESP-IDF%20v4.4%20Release%20Branch%20EN.html
