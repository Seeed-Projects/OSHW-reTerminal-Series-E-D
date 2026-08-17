# APP Device Info 功能模型

对应代码：`src/APP/app_device_info.h`、`src/APP/app_device_info.cpp`

## 第一性原理定位

DeviceInfo 当前承担的职责远大于“设备信息”。它是 APP 层的设备运行状态与持久化配置聚合点：

- 设备状态位管理。
- NVS 配置缓存和写入。
- Wi-Fi 凭据存储。
- 普通深睡计时和进入深睡。
- 低电量自动深睡保护。
- 唤醒原因处理。
- SD 卡插拔和充电状态监测。
- 解绑时的本地图片清理。

从第一性原理看，硬件事实由 HAL 提供，DeviceInfo 负责把硬件状态转成产品策略。

## 对外接口分组

初始化：

- `DeviceInfoEarlyInit()`：创建配置缓存、事件组、定时器，并从 NVS 加载初始值。
- `DeviceInfoInit()`：处理唤醒原因，启用按键唤醒，启动 DeviceInfo 的 FreeRTOS task 和 1 秒定时器。

运行状态：

- `AddDeviceState()`、`RemoveDeviceState()`、`IsDeviceInState()`。
- `PrintDeviceState()`。
- `IsLocalGallerySession()`、`SetLocalGallerySession()`：访问本轮运行期本地图集上下文。
- `IsTimerWakeup()`、`SetTimerWakeup()`、`MarkUserWakeup()`：访问本次唤醒是否来自 timer 的运行期上下文。

睡眠：

- `EnterDeepSleep()`。
- `StartDeepSleepTimer()`。
- `StopDeepSleepTimer()`。

持久化配置和跨模块事实：

- Wi-Fi：`SaveWifiCredential()`、`GetWifiCredentials()`、`GetWifiPassword()`、`RemoveWifiCredential()`。
- 内容模式：`GetContentMode()`、`SetContentMode()`、`IsGalleryContent()`。
- 内容和图片：`GetContentVersion()`、`SetContentVersion()`、`GetImageCount()`、`SetImageCount()`、`HasImage()`、`SetHasImage()`、`GetImageIndex()`、`SetImageIndex()`、`GetAlbumVersion()`、`SetAlbumVersion()`。
- 刷新和睡眠：`GetDeepSleepInterval()`、`SetDeepSleepInterval()`、`GetDeepSleepEnabled()`、`SetDeepSleepEnabled()`。
- 云端/绑定上报：`GetCloudToken()`、`SetCloudToken()`、`GetBindState()`、`SetBindState()`、`GetReportStatus()`、`SetReportStatus()`、`GetActivationCode()`、`SetActivationCode()`。

解绑：

- `ResetAfterUnbind()`：当绑定状态为 `DEVICE_BIND` 时清理图片、清空 NVS、恢复部分默认值并重启。

## 设备状态模型

状态位定义在 `src/app_config.h`：

- `STATE_STARTING`：启动中。
- `STATE_BINDING`：绑定中。
- `STATE_REFRESHING`：屏幕刷新中。
- `STATE_DOWNLOADING`：图片下载中。
- `STATE_PROVISIONING`：配网中。
- `STATE_CLICKING`：一次用户点击刷新处理中。

这些状态用于保护关键流程：

- Button 忽略忙状态下的图片切换。
- View 绘制期间设置/清除 `STATE_REFRESHING`。
- Download 下载期间设置/清除 `STATE_DOWNLOADING`。
- 深睡前检查 `STATE_DOWNLOADING | STATE_REFRESHING | STATE_PROVISIONING`。

## NVS 缓存和写入模型

当前 NVS 命名空间为 `hmi_config`。

`DeviceInfoEarlyInit()` 会把 NVS 值加载到 `g_deviceCfgs` 内存缓存。除 Wi-Fi 凭据外，配置写入采用 500 ms debounce：

1. `Set*()` 语义接口更新内存缓存。
2. 对应配置的 debounce timer 到期。
3. timer 设置 `g_eg_devicecfg_change` 和 `g_eg_task_wakeup`。
4. DeviceInfo FreeRTOS task 打开 Preferences，把发生变化的字段写入 NVS。

Wi-Fi 凭据是例外：

- 保存、删除 Wi-Fi 凭据后立即调用 `persist_wifi_credentials_now()` 写入 NVS。
- 最多保存 5 组。
- 新保存的 SSID 会移到最前面。

## 深睡策略

普通深睡入口：

- `StartDeepSleepTimer()`：启动 60 秒延迟深睡。若本次唤醒来自 timer，则 1 ms 后进入深睡判断。
- `StopDeepSleepTimer()`：停止普通深睡计时。
- `EnterDeepSleep()`：真正执行深睡前的策略判断。

普通深睡约束：

- 编译期开关 `RETERMINAL_DEEPSLEEP_DISABLE == 0` 时，如果 PMIC 正在充电，则不进入深睡。
- 如果处于下载、刷新、配网状态，则不进入深睡。
- 非 gallery 模式下，进入深睡前会把设备状态设置为 `DEVICE_SLEEP` 并触发一次 IoT report。
- 睡眠时间优先基于 RTC 和 `sleepInterval` 计算。
- 如果当前时间或下次唤醒会落入 04:30 附近维护窗口，会调整睡眠时间避开窗口。
- 最终调用 `esp_sleep_enable_timer_wakeup()`，并通过 `HAL::GetHAL().touchEnableWakeup()` 配置触摸唤醒。

`disableSleep` 的含义：

- `disableSleep` 只影响普通自动深睡。
- 低电量保护不受 `disableSleep` 影响。

## 低电量自动深睡

低电量保护放在 APP 层 DeviceInfo 中，当前策略为：

- 阈值：`LOW_BATTERY_AUTO_SLEEP_PERCENT = 3.0f`。
- 检查周期：60 秒。
- 如果正在充电，跳过。
- 如果电量读取失败，即 `batteryReadPercent() < 0`，跳过。
- 如果电量小于等于 3%，发送 `VIEW_EVENT_SHOW_LOW_BATTERY`，低电页面刷新后进入深睡。
- 启动时会先做一次低电量门禁，命中后发送 `VIEW_EVENT_SHOW_LOW_BATTERY`，阻止后续模块继续初始化，并在低电页面刷新后进入深睡。
- 不读取、不尊重 `disableSleep`，因为它属于设备保护策略，不是普通休眠策略。

这个位置是合理的：HAL 只负责提供电量和充电状态，是否为了保护设备而睡眠属于产品策略。

## 唤醒处理模型

`DeviceInfoInit()` 根据 `HAL::GetHAL().wakeupReason()` 决定启动行为：

- `None`：普通启动。
- `Key0Short` / `Touch`：标记为用户点击唤醒。
- `Key1Short`：如果设备模式为 gallery，则进入 gallery，延迟 2 秒后选下一张并刷新。
- `Key2Short`：如果设备模式为 gallery，则进入 gallery，延迟 2 秒后选上一张并刷新。
- `DoubleLong`：通常用于进入配网，不立即执行更多动作。
- `TripleLong`：进入 gallery 并显示图片。
- `Timer`：标记为 timer 唤醒，先选下一张；gallery 模式下直接显示本地图片，否则等待网络刷新。

初始化末尾调用 `HAL::GetHAL().enableButtonWakeup()`，按键唤醒配置属于 HAL。

## 调度模型

DeviceInfo 已从 MiniRT 轮询迁到独立 FreeRTOS task：

- debounce timer 和 1 秒周期 timer 只设置 event bits。
- DeviceInfo task 阻塞等待 `EVENT_DEVICECFG_CHANGE | EVENT_TIMER_1S`。
- 收到配置变化后批量写 NVS。
- 收到 1 秒事件后处理 SD 热插拔、充电状态变化和低电量保护。

这样 DeviceInfo 不再占用 MiniRT 的串行调度时间，也不需要每 10 ms 空轮询一次。

## 周期监测

DeviceInfo task 每秒处理：

- SD 卡热插拔：如果 HAL 支持热插拔检测，状态变化时初始化或反初始化 SD，并触发 IoT report。
- 充电状态变化：触发 IoT report；开始充电时停止普通深睡计时，停止充电时根据 `disableSleep` 决定是否重新启动深睡计时。
- 低电量保护：每 60 秒检查一次。

## 当前边界问题

- 模块名 `device_info` 已经无法表达真实职责，后续可以考虑拆成 `device_state`、`device_config`、`power_policy`、`wakeup_policy`。
- NVS 字段级 API 已收为 `app_device_info.cpp` 内部实现；对外接口现在按业务语义暴露。后续可继续把 DeviceInfo 拆成 `device_state`、`device_config`、`power_policy`、`wakeup_policy`。
- 图片清理逻辑在 DeviceInfo 中，实际更接近 storage/content cleanup。
- `gallery mode` 和 `timer wakeup` 已收口为 DeviceInfo 运行上下文接口，外部模块不再直接读写裸全局变量。
- 当前深睡策略、绑定状态、图片状态、Wi-Fi 状态互相穿透，建议后续先整理“设备运行态”再拆代码。
