# APP Indicator 功能模型

对应代码：`src/APP/app_indicator.h`、`src/APP/app_indicator.cpp`、`src/hal/hal_indicator.h`、`src/hal/hal_indicator.cpp`

## 第一性原理定位

Indicator 模块负责把 APP 层事件转换成用户可感知反馈。

APP 层的 `app_indicator` 只保留反馈语义接口，实际蜂鸣器和 LED 序列由 HAL 层的 `hal_indicator` 执行。它不拥有硬件事实。蜂鸣器和 LED 能力由 HAL 提供：

- 优先使用 `HAL::GetHAL().buzzerBeep()`。
- 如果蜂鸣器不可用或播放失败，退化为 LED pulse。

因此 Indicator 是“反馈适配层”，不是 buzzer 或 LED 驱动。业务模块只表达“点击反馈、刷新完成、成功、错误”等反馈语义，不直接驱动蜂鸣器或 LED 时序。

## 对外接口

- `app_indicator_btn_click()`：用户点击反馈。
- `app_indicator_refresh_finished()`：刷新完成反馈。
- `app_indicator_img_download()`：单张图片下载/保存成功反馈。
- `app_indicator_succeed()`：成功提示音。
- `app_indicator_error()`：错误提示音。

## 当前反馈规则

- `app_indicator_btn_click()`：1319 Hz，35 ms；如果蜂鸣失败则 LED 亮 35 ms。
- `app_indicator_refresh_finished()`：两次短 beep，频率和间隔来自 `app_config.h`。
- `app_indicator_img_download()`：LED pulse 100 ms。
- `app_indicator_succeed()`：523 Hz、659 Hz、784 Hz 三段上行音。
- `app_indicator_error()`：392 Hz、262 Hz 两段下行音。

这些规则由 `hal_indicator` 的独立 FreeRTOS task 串行播放。`app_indicator_xxx()` 调用只把 pattern 放入队列，不在调用方上下文里 `delay()`。

## 当前依赖

- APP 层：`app_indicator.cpp` 依赖 `hal/hal_indicator.h`。
- HAL 层：`hal_indicator.cpp` 依赖 `app_config.h` 和 `hal/hal.h`，负责实际播放。

## 当前边界问题

- Indicator 不应知道业务状态，只接受“反馈语义”。当前这一点成立。
- `hal_indicator` 当前按队列顺序播放反馈，如果连续下载大量图片，过多反馈可能被队列容量限制丢弃；这是可接受的降级，比阻塞下载流程更合理。
- 如果未来不同板子反馈能力差异更大，HAL 应提供 capability，Indicator 再决定降级策略。
