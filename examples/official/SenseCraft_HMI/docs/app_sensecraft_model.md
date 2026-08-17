# APP SenseCraft 功能模型

对应代码：`src/APP/app_sensecraft.h`、`src/APP/app_sensecraft.cpp`

## 第一性原理定位

SenseCraft 是云端会话与协议适配层。

设备本地拥有事实：

- Wi-Fi 是否可用。
- 电量、按键、传感器、存储、图片版本、刷新间隔、深睡配置。
- 图片下载和显示进度。

云端拥有意图：

- 查询设备状态。
- 修改设备配置。
- 下发图片资源。
- 确认设备是否下载和应用成功。

SenseCraft 的职责是建立两者之间的可信会话，并完成协议转换：

- 通过 bind 接口申请当前云端会话配置。
- 使用会话配置建立 MQTT 双向通道。
- 把本地事实编码成云端协议并上报。
- 把云端协议解析成本地动作并分发。
- 在会话失效时重新申请云端会话。

SenseCraft 不是 Wi-Fi 管理器、图片下载器、图库调度器、View 控制器，也不是硬件事实的拥有者。

## 模块边界

SenseCraft 负责：

- bind 请求和 bind 响应解析。
- server time 同步。
- MQTT 配置保存和 MQTT client 生命周期。
- MQTT publish/subscribe。
- 云端下发消息分发。
- IoT 上报协议构造。
- 图片刷新请求协议构造。
- 图片刷新结果协议构造。
- 云端会话失败恢复。

SenseCraft 只调用但不拥有：

- Wi-Fi 可用性：由 Wi-Fi 模块判断并通过事件通知。
- 设备持久配置：由 DeviceInfo 保存，如 bind state、cloud token、content version、refresh interval、deep sleep enabled。
- 图片下载：由 Download 处理 manifest、图片下载、校验和存储。
- 图片选择和显示：由 Gallery/View 处理。
- 深睡和活跃状态：由 PowerManager/DeviceInfo 处理。
- 硬件读数：由 HAL 或对应模块提供。

## 对外接口

- `app_task_init()`：初始化 SenseCraft 事件、定时器、状态并启动任务。
- `request_update_timer_start()`：启动云端内容周期刷新定时器。
- `imgRefreshRes(const char *version, bool apply, int download_per, const char *image_id, int image_index, int image_total)`：使用本轮目标版本上报图片下载/应用结果。

对外接口保持少量、直接、稳定。内部 helper 不为了“看起来分层”而过度包装。

## 会话定义

bind 不定义为“首次绑定接口”。

bind 的语义是：

> 向 SenseCraft 云端申请当前设备可用的会话配置。

一次 bind 响应可能包含：

- `mqtt.endpoint`
- `mqtt.client_id`
- `mqtt.username`
- `mqtt.password`
- `mqtt.publish_topic`
- `mqtt.subscribe_topic`
- `activation.code`
- `activation.message`
- `server_time`

其中 `mqtt.password` 是设备云端鉴权 token，需要同步到 DeviceInfo 的 cloud token。

以下情况都应回到 bind：

- 首次 Wi-Fi 可用。
- 等待用户激活时轮询绑定状态。
- 本地没有可用 MQTT 配置。
- MQTT 配置丢失。
- MQTT 鉴权失败。
- 云端 token 失效。

## 状态模型

SenseCraft 状态只表达云端会话生命周期：

```cpp
enum class SenseCraftState {
    Offline,            // Wi-Fi 不可用
    NeedSession,        // 需要 bind 获取或刷新云端会话
    WaitingActivation,  // 已拿到激活码，等待用户绑定
    SessionReady,       // 已拿到 MQTT 配置
    MqttConnecting,     // MQTT 正在连接
    Online,             // MQTT 已连接，可收发云端协议
};
```

状态转移主线：

```text
Wi-Fi ready
  -> NeedSession
  -> bind
      -> activation code != 0: WaitingActivation
      -> activation code == 0: SessionReady
  -> MqttConnecting
  -> Online
```

失败恢复：

```text
Wi-Fi lost
  -> Offline
  -> stop/disconnect MQTT

MQTT disconnected
  -> 如果 Wi-Fi 不可用: Offline
  -> 如果网络可用: MqttConnecting

MQTT auth/config error
  -> NeedSession
  -> bind

bind failed
  -> retry
  -> retry exhausted
      -> deep sleep enabled: enter deep sleep
      -> deep sleep disabled: delayed retry
```

`SessionReady` 和 `MqttConnecting` 只有在实现中需要判断或恢复时才显式保存；否则可以作为流程阶段，不强行落成全局状态。

## 事件模型

外部事件：

- `SENSECRAFT_EVENT_START`：Wi-Fi 首次可用，开始申请云端会话。
- `SENSECRAFT_EVENT_RESUME`：网络恢复，继续或重建云端会话。
- `SENSECRAFT_EVENT_STOP`：网络不可用，停止云端通道。
- `MQTT_EVENT_SCREEN_REFRESH`：请求云端图片资源。
- `MQTT_EVENT_IOT_REPORT`：上报设备状态。
- `DEVICE_EVENT_DEEPSLEEP`：进入深睡。

内部事件：

- `ENSURE_SESSION`：确保当前有可用云端会话。
- `BIND_RETRY`：bind 失败后的重试。
- `BIND_POLL`：等待激活时轮询绑定状态。
- `MQTT_CONNECT`：使用当前会话配置连接 MQTT。
- `SCREEN_REFRESH`：发送图片资源请求。
- `IOT_REPORT`：发送 IoT 状态上报。

事件命名应表达业务目的，不直接泄露旧实现细节。

## bind 流程

```text
ensureSession()
  -> build board info
  -> POST /api/v1/device/bind
  -> parse server_time
  -> parse mqtt config
  -> parse activation
```

绑定成功：

- `activation.code == 0`
- 写入 MQTT 配置。
- `SetCloudToken(mqtt.password)`。
- `SetActivationCode(0)`。
- `SetBindState(true)`。
- 首次绑定成功时切换到等待内容页面。
- BLE AT 配网时通过 `APP_ATCMD_BIND` 主动返回 `code=0`。
- 进入 MQTT 连接流程。

等待激活：

- `activation.code != 0`
- `SetActivationCode(code)`。
- `SetBindState(false)`。
- `BleAt` 来源通过 `APP_ATCMD_BIND` 返回激活码，不刷新设备屏幕。
- 其他来源显示激活码页面。
- 启动 bind poll。

bind 失败：

- 进入 bind retry。
- 超过重试次数后执行深睡策略或延迟重试。

## MQTT 流程

MQTT 配置只能来自当前 bind 响应。

连接流程：

```text
connectMqtt()
  -> create MQTT client
  -> register mqtt event handler
  -> start client
```

连接成功：

- 状态进入 `Online`。
- 订阅 `subscribeTopic`。
- 触发首次图片刷新请求。
- 触发首次 IoT 状态上报。
- 启动周期 IoT 上报 timer。

断开：

- 状态退出 `Online`。
- 让 Wi-Fi 模块检查网络可用性。
- 网络恢复后根据错误类型选择 MQTT reconnect 或重新 bind。

鉴权或配置失败：

- 销毁或停止旧 MQTT client。
- 清理当前 MQTT 配置。
- 状态进入 `NeedSession`。
- 重新 bind。

## 协议模型

设备主动 publish：

- `iot`：设备状态、能力和可调用方法。
- `img_flash`：请求最新图片资源。
- `img_flash_res`：图片下载/应用进度和结果。

云端下发：

- `iot`：配置命令。
- `img_flash`：图片资源下发。
- `album`：图片资源下发。

消息分发：

```text
handleMqttData()
  -> type == "iot": handleCloudCommand()
  -> type == "img_flash": handleImageResource()
  -> type == "album": handleImageResource()
```

未知 type 只记录日志，不触发业务动作。

## 云端配置命令

`handleCloudCommand()` 只处理云端协议解析和本地配置写入：

- `DataAccess.SetInterval`
  - 写入刷新间隔。
  - 重启周期刷新 timer。

- `Power.SetDeepSleep`
  - 写入普通深睡开关。
  - 通知电源管理模块调整普通深睡计时。

未识别命令只记录日志。

## 图片资源流程

主动请求：

```text
requestImageRefresh()
  -> publish type=img_flash
  -> data.version = GetContentVersion()
```

云端响应：

```text
handleImageResource()
  -> parse data.session_id
  -> parse data.version
  -> parse data.manifest_url
  -> duplicate session guard
  -> app_download_set_manifest_url(manifest_url, version)
```

内容版本由 Download 在本轮图片成功应用后提交。SenseCraft 不决定本地下一张图片是谁，也不直接控制 View 显示。

下载结果：

```text
imgRefreshRes()
  -> publish type=img_flash_res
  -> data.version
  -> data.img_id
  -> data.img_apply
  -> data.img_progress
  -> data.img_total
  -> data.img_index
```

## IoT 上报流程

`publishIotReport()` 构造并发布 `type=iot`：

- states：设备事实快照。
- descriptors：云端可调用方法描述。

设备事实来自 HAL、DeviceInfo、Download、Gallery 或对应模块的读取接口。SenseCraft 不缓存这些事实，也不修改不属于云端命令的本地状态。

## 周期刷新

周期刷新 timer 到期后：

- 多张图片时选择下一张可播放图片。
- StaticImage 直接显示本地文件。
- DynamicCanvas 在 Wi-Fi 已连接时请求更新；未连接时显示本地缓存。
- 单张 DynamicCanvas 周期请求自身更新。
- 如果设备正在配网、刷新或用户操作中，可以跳过本轮请求。

## 文件结构

`app_sensecraft.cpp` 先保持单文件收敛，不先拆类和多文件。

建议区域顺序：

```text
1. State / Config
2. Time helpers
3. Session / bind API
4. MQTT lifecycle
5. MQTT publish helpers
6. Cloud message handlers
7. Image refresh flow
8. IoT report flow
9. Timers
10. Event dispatch
11. Task init
```

## 重构原则

- 不做旧模型兼容。
- 不为了分层而引入空壳类。
- 无多方依赖的小函数直接内聚。
- 不保留只被调用一次且不降低复杂度的 helper。
- 不把 Wi-Fi、Download、Gallery、View、PowerManager 的事实搬进 SenseCraft。
- 失败恢复围绕会话状态机，而不是散落的补丁分支。
