# BSP 分层迁移计划：从理想重写改为当前工程可执行重构

## 1. 本轮目标

这份文档不再作为一次性重写方案，而是作为当前工程可以逐步落地的分层迁移计划。核心目标不是追求完美架构，而是先把维护成本最高的问题拆掉：

1. APP 层不再直接依赖板卡宏、屏幕组合宏和引脚宏。
2. 板级事实集中到 `src/boards/<board>/config.h` 和对应板级实现中。
3. HAL 层向 APP 暴露稳定能力接口，例如按键、蜂鸣器、墨水屏信息、唤醒配置、电池、RTC、触摸、SD 卡。
4. `BOARD_SCREEN_COMBO` 只保留给 Seeed_GFX 这类需要编译期选择驱动的底层库，不再作为 APP 业务分支依据。
5. 优先保持现有工程可编译、可验证、可逐步迁移；ESP-IDF 原生 API 是长期方向，但本轮不强行一次性替换所有 Arduino API。

## 2. 第一性原理下的分层边界

### 2.1 Board 层：硬件事实

Board 层回答“这块板子是什么、有哪些硬件、硬件怎么连”。

Board 层应该拥有：

- 引脚定义。
- 屏幕型号、尺寸、颜色、旋转映射、上报类型。
- 板卡型号、BLE 短名、AP 前缀、产品上报名称。
- 外设对象的组装和生命周期，例如 LED、Button、Buzzer、EpaperDisplay、SD、Battery、PMIC、RTC、SHT40、Touch。
- 某块板子是否有某个硬件，以及硬件初始化失败后的安全降级。

Board 层不应该拥有：

- 低电量后是否睡眠这种产品策略。
- UI 页面该怎么排版。
- MQTT / BLE / Wi-Fi 业务流程。

### 2.2 HAL 层：硬件能力

HAL 层回答“上层可以做什么动作、读到什么状态”。

HAL 层应该提供：

- `display()` / `epaperDisplay()` / `screenInfo()`。
- `button(index)`、`buttonIsPressed(index)`、`enableButtonWakeup()`。
- `buzzerBeep()`、`buzzerStop()`。
- `ledSet()`。
- `batteryReadPercent()`、`batteryReadVoltage()`、`pmicIsCharging()`。
- `rtcReadTime()`、`envRead()`、`touchLoop()`、`sdInit()`。
- wakeup reason 解析。

HAL 层可以做轻量判空和降级，但不要堆业务策略。比如按键唤醒配置属于 HAL；低电休眠时机属于 APP。

### 2.3 APP 层：产品策略

APP 层回答“产品在某个状态下要做什么”。

APP 层可以依赖：

- HAL 暴露的硬件能力。
- Board/HAL 暴露的屏幕信息，例如宽高、颜色、分辨率、上报类型。
- APP 层内部的产品状态和配置，例如低电保护、普通休眠、下载/刷新/配网状态。

APP 层不应该直接依赖：

- `KEY0_PIN`、`KEY1_PIN`、`KEY2_PIN`。
- `BUZZER_PIN`、`GREENLED_PIN`。
- `BOARD_SEEED_RETERMINAL_*`、`BOARD_XIAO_*`。
- `BOARD_SCREEN_COMBO`。
- `include/variants/board_*.h`。

## 3. 已确认设计决策

1. `docs/bsp_refactoring_plan.md` 只提供方向，最终以当前工程可落地为准。
2. 本轮优先目标是规范 Board / HAL / APP 分层，消除 APP 中板卡宏和引脚宏判断。
3. APP 不使用 `BOARD_SCREEN_COMBO`。板级通过接口提供屏幕信息，APP 根据屏幕宽高、颜色、上报类型、旋转映射等信息做逻辑。
4. `include/variants/board_*.h` 废弃，迁移到 `src/boards/<board>/config.h`。
5. ESP-IDF API 是长期方向；本轮可以保留必要的 Arduino API 以降低迁移风险。
6. `Button`、`Buzzer`、`EpaperDisplay` 纳入 Board/HAL 设备模型。
7. 麦克风当前产品需求未使用，本轮不处理。
8. XIAO DIY 统一认为有 SY6974 PMIC，初始化失败时安全跳过。
9. E1001 / E1002 长期共用一个板级类；差异只在屏幕接口和 `BOARD_SCREEN_COMBO`，分别为 520 和 521。
10. 低电休眠策略放在 APP 层；按键深睡唤醒配置放在 HAL 层。当前低电保护阈值为 5%，不受 `disableSleep` 普通休眠开关影响。

## 4. 当前工程状态

当前工程已经完成本轮主要分层迁移：

- `src/boards/common/board.h` 已有 Board 基类和 `DECLARE_BOARD`。
- `src/boards/<board>/<board>.cpp` 已有板级实现。
- `Button`、`Buzzer`、`EpaperDisplay` 已接入 Board 基类和各板级实现。
- `src/hal/hal.cpp` 已经通过 `Board::GetInstance()` 访问 LED、Button、Buzzer、SD、电池、PMIC、RTC、SHT40、触摸和显示。
- `src/boards/board_registry.h` 已经有板卡、屏幕、分辨率、颜色、旋转映射等数据。
- `src/boards/<board>/config.h` 已承接原 `include/variants/board_*.h` 的板级定义。
- `src/APP` 和 `src/utils` 已不再直接使用板卡宏、引脚宏、`BOARD_SCREEN_COMBO`、`EPD_COLOR` / `EPD_MONO` 或 `board_registry`。
- 等待页这类仍需编译期页面资源选择的逻辑，已通过 `screen_assets` 从 APP 下沉到 common/Board 侧。

仍需注意：

- `BOARD_SCREEN_COMBO` 仍是 Seeed_GFX 和页面资源编译期选择所需输入，不能从底层构建链路删除。
- HAL 内部仍可使用 `board_registry` 查询当前板级事实；APP 只能通过 HAL 获取这些信息。
- 当前设备类仍以 Arduino API 为主，ESP-IDF 原生 API 迁移属于后续阶段。

## 5. 目标结构

```text
src/
  app_config.h
  driver.h
  APP/
    app_events.h
    iot.h
    app_*.h
    app_*.cpp
  boards/
    board_registry.h
    common/
      board.h
      board.cpp
      led.h
      led.cpp
      button.h
      button.cpp
      buzzer.h
      buzzer.cpp
      epaper_display.h
      epaper_display.cpp
      screen_assets.h
      screen_assets.cpp
      ...
    reterminal_e1001/
      config.h
      reterminal_e1001.cpp
    reterminal_e1003/
      config.h
      reterminal_e1003.cpp
    reterminal_e1004/
      config.h
      reterminal_e1004.cpp
    xiao_diy/
      config.h
      xiao_diy.cpp
  hal/
    hal.h
    hal.cpp
  utils/
    *.h
    *.cpp / *.c
  resources/
    fonts/
    pages/
    resources.h
    ...
```

## 6. Board 对外接口目标

Board 基类保留简单直接的 getter，不引入额外抽象层。没有多种底层实现竞争的设备，不做纯虚驱动接口。

建议 Board 最终提供：

```cpp
class Board {
public:
    static Board& GetInstance();

    virtual const char* GetBoardType() const = 0;
    virtual const board_registry::BoardProfile& GetProfile() const = 0;
    virtual const board_registry::BoardScreenEntry& GetScreen() const = 0;

    virtual Led* GetLed() { return nullptr; }
    virtual Button* GetButton(size_t index) { return nullptr; }
    virtual Buzzer* GetBuzzer() { return nullptr; }
    virtual EpaperDisplay* GetEpaperDisplay() { return nullptr; }

    virtual SdCard* GetSdCard() { return nullptr; }
    virtual BatteryGauge* GetBattery() { return nullptr; }
    virtual PmicSy6974* GetPmic() { return nullptr; }
    virtual RtcPcf8563* GetRtc() { return nullptr; }
    virtual Sht40Sensor* GetEnv() { return nullptr; }
    virtual Gt911Touch* GetTouch() { return nullptr; }
    virtual EPaper& GetDisplay() = 0;
};
```

原则：

- `GetProfile()` 和 `GetScreen()` 是 APP 获取板卡/屏幕事实的唯一来源。
- `GetButton(index)` 替代 `KEY*_PIN`。
- `GetBuzzer()` 替代 `BUZZER_PIN`。
- `GetEpaperDisplay()` 提供屏幕元信息，并保留对现有 `EPaper` 的兼容访问。
- E1001/E1002 的板级类可以共用实现，但 `GetProfile()` / `GetScreen()` 返回不同数据。

## 7. HAL 对外接口目标

HAL 是 APP 的主要入口。接口要少而稳定：

```cpp
namespace HAL {

class Hal {
public:
    void init();

    const board_registry::BoardProfile& boardProfile() const;
    const board_registry::BoardScreenEntry& screen() const;

    const char* boardType() const;
    const char* bleShortName() const;
    const char* apPrefix() const;
    const char* screenResolution() const;
    const char* boardInfoType() const;
    const char* boardInfoScreenType() const;
    uint16_t screenWidth() const;
    uint16_t screenHeight() const;
    bool screenIsColor() const;
    uint8_t screenRotation(size_t orientation_index = 0) const;
    uint8_t screenOrientationIndex() const;

    Button* button(size_t index);
    bool buttonIsPressed(size_t index);
    void enableButtonWakeup();

    bool buzzerBeep(uint32_t freq_hz, uint32_t duration_ms);
    void buzzerStop();

    void ledSet(bool on);
    EPaper& display();
    EpaperDisplay* epaperDisplay();
};

}
```

HAL 可以暂时继续在 `hal.cpp` 中集中实现，不需要过早拆成很多文件。等接口稳定后再按主题拆分。

## 8. 迁移步骤

### 阶段 A：文档和接口校准

- 将本文件改成当前工程可执行迁移计划。
- 明确 Board / HAL / APP 的职责边界。
- 保留 ESP-IDF API 长期方向，但本轮允许 Arduino 兼容层。

完成条件：

- 文档不再要求一次性理想重写。
- 文档明确 APP 不直接使用板级宏、引脚宏和 `BOARD_SCREEN_COMBO`。

### 阶段 B：Board/HAL 基础接口

- 增加 `Button`、`Buzzer`、`EpaperDisplay` 具体类。
- Board 基类增加 `GetProfile()`、`GetScreen()`、`GetButton()`、`GetBuzzer()`、`GetEpaperDisplay()`。
- 各板级实现创建并返回对应设备。
- XIAO DIY 创建 PMIC 对象，初始化失败时保留安全降级。

完成条件：

- HAL 能通过 Board 获取按钮、蜂鸣器和屏幕信息。
- APP 不需要知道具体按键引脚和蜂鸣器引脚。

### 阶段 C：迁移板级配置

- 在每个 `src/boards/<board>/config.h` 放置本板引脚、屏幕组合、电池曲线等定义。
- 板级 `.cpp` include 自己目录下的 `config.h`。
- `src/app_config.h` 不再 include `include/variants/board_*.h`。
- 保留 `BOARD_SCREEN_COMBO` 作为 Seeed_GFX 编译期输入，但它只在板级配置或底层库编译中使用。

完成条件：

- `include/variants/board_*.h` 不再被主工程 include。
- 板级定义和板级实现位于同一目录。

### 阶段 D：迁移 APP 中的宏判断

优先迁移这些文件：

- `src/APP/app_indicator.cpp` / `src/hal/hal_indicator.cpp`
  - APP 层只保留反馈语义入口。
  - HAL 层通过 `HAL::GetHAL().buzzerBeep()` / `buzzerStop()` / `ledSet()` 非阻塞播放反馈。

- `src/APP/app_input.cpp`
  - `OneButton button(KEYx_PIN)` 改为使用 Board/HAL 提供的 `Button`。
  - `digitalRead(KEYx_PIN)` 改为 `buttonIsPressed(index)`。

- `src/hal/hal.cpp`
  - wakeup reason 解析不再使用 `KEY*_PIN`。
  - `enableButtonWakeup()` 统一配置 3 个按键深睡唤醒。
  - 移除低电量自动深睡策略，改由 APP 调用。

- `src/APP/app_device_info.cpp`
  - 深睡唤醒配置改为 HAL 接口。

- `src/APP/app_ble.cpp`
  - BLE 名称改为 HAL/Board profile。

- `src/APP/app_wifi.cpp`
  - AP 前缀改为 HAL/Board profile。

- `src/APP/app_sensecraft.cpp`
  - 上报的板卡类型、屏幕类型、分辨率改为 HAL/Board screen info。
  - 按键状态改为 HAL button。

- `src/APP/app_view.cpp` 和 `src/utils/image_parser.cpp`
  - 旋转、屏幕尺寸、是否彩色、分辨率等改为 HAL screen info。
  - 不再使用 `BOARD_SCREEN_COMBO` 做 APP 业务分支。

完成条件：

- `src/APP` 中不再出现 `BOARD_SCREEN_COMBO`、`BOARD_SEEED_*`、`BOARD_XIAO_*`、`KEY*_PIN`、`BUZZER_PIN`、`GREENLED_PIN`。

### 阶段 E：验证和清理

- 编译主要环境：
  - `reterminal_e1001`
  - `reterminal_e1002`
  - `reterminal_e1003`
  - `reterminal_e1004`
  - `xiao_diy_kit`
- 搜索确认 APP 层没有残留板级宏。
- `TRMNL_7inch5_OG_DIY_Kit` 如因本机缺 Python `fatfs` 失败，记录为环境依赖问题，不作为本轮代码阻塞。
- 等上述迁移稳定后，再逐步将内部实现从 Arduino API 收敛到 ESP-IDF API。

本轮验证状态：

- `pio run -e reterminal_e1001 -e reterminal_e1002 -e reterminal_e1003 -e reterminal_e1004 -e xiao_diy_kit` 已通过。
- `rg -n "BOARD_SCREEN_COMBO|BOARD_SEEED|BOARD_XIAO|KEY[0-9]_PIN|BUZZER_PIN|GREENLED_PIN|EPD_COLOR|EPD_MONO|board_registry::" src/APP src/utils src/app_config.h` 无匹配。
- `include/variants/board_*.h` 已迁移到 `src/boards/<board>/config.h`。

## 9. 不在本轮处理

- 麦克风。
- 全量 ESP-IDF 驱动重写。
- UI 页面大规模重构。
- 删除 Seeed_GFX 需要的 `BOARD_SCREEN_COMBO` 编译期输入。
- 将 HAL 过早拆成很多小文件。

## 10. 验收搜索命令

```bash
rg -n "BOARD_SCREEN_COMBO|BOARD_SEEED|BOARD_XIAO|KEY[0-9]_PIN|BUZZER_PIN|GREENLED_PIN|board_registry::" src/APP src/utils src/hal src/app_config.h
```

预期：

- `src/APP` 和 `src/utils` 中不再出现板级宏和引脚宏。
- `src/hal` 中不再直接依赖引脚宏，但可以通过 Board/HAL 对象拿到按键信息。
- `src/app_config.h` 不再 include `include/variants/board_*.h`。

编译命令：

```bash
pio run -e reterminal_e1001 -e reterminal_e1002 -e reterminal_e1003 -e reterminal_e1004 -e xiao_diy_kit
```

## 11. 判断一个改动是否正确

一个改动只有同时满足下面条件，才算符合本轮重构方向：

1. 它减少 APP 对具体板卡、引脚、屏幕 combo 的了解。
2. 它把硬件事实放回 Board，把硬件能力放到 HAL，把产品策略留在 APP。
3. 它没有引入多余抽象，也没有为了“看起来架构化”增加无意义方法。
4. 它能在当前工程里编译验证，而不是只存在于理想设计里。
