# APP WiFi 功能模型

对应代码：`src/APP/app_wifi.h`、`src/APP/app_wifi.cpp`

## 第一性原理定位

WiFi 模块当前是网络连接管理器，同时承担配网门户和网络状态桥接：

- 保存网络自动连接。
- 手动触发自动连接。
- SoftAP + WebServer + DNS captive portal 配网。
- Wi-Fi 扫描和扫描缓存。
- AT/BLE 连接请求执行。
- Wi-Fi/MQTT 断线后的网络检查。
- 向 SenseCraft 报告网络开始、停止、恢复。

它的本质是“网络可用性管理”，不是云端协议层。云端协议属于 SenseCraft。

## 对外接口

- `app_wifi_init()`：初始化 Wi-Fi 模块，注册事件和 Arduino WiFi event，启动 2 秒后自动连接 timer，并注册 MiniRT task。
- `app_wifi_request_autoconnect(bool manual)`：请求自动连接保存的 Wi-Fi。`manual=true` 表示用户主动触发，gallery mode 下也允许执行。
- `app_wifi_get_scan_cache()`：返回当前扫描缓存。
- `get_portal_status()`：返回配网门户是否运行。

## 事件模型

事件 base：`WIFI_APP_EVENT_BASE`。

WiFi 模块消费：

- `APP_WIFI_EVENT_START_AUTOCONNECT`
- `APP_WIFI_EVENT_START_PORTAL`
- `APP_WIFI_EVENT_SCAN`
- `APP_WIFI_EVENT_CONNECT`
- `APP_WIFI_EVENT_PORTAL_DONE`
- `APP_MQTT_DISCONNECTED`
- `APP_INPUT_EVENT_BASE / APP_INPUT_EVENT_ENTER_PROVISIONING`

WiFi 模块也会发出：

- `APP_WIFI_EVENT_SCAN_DONE`
- `CTRL_EVENT_BASE / SENSECRAFT_EVENT_START`
- `CTRL_EVENT_BASE / SENSECRAFT_EVENT_STOP`
- `CTRL_EVENT_BASE / SENSECRAFT_EVENT_RESUME`
- `CTRL_EVENT_BASE / APP_ATCMD_WIFI_SCAN`
- `CTRL_EVENT_BASE / APP_ATCMD_WIFI_CONNECT`
- `VIEW_EVENT_BASE / VIEW_EVENT_SHOW_STARTUP`
- `VIEW_EVENT_BASE / VIEW_EVENT_SHOW_IMAGE`

内部 EventGroup bit：

- `WIFI_EVENT_BIT_START_AUTOCONNECT`
- `WIFI_EVENT_BIT_STA_CONNECTED`
- `WIFI_EVENT_BIT_STA_DISCONNECTED`
- `WIFI_EVENT_BIT_START_PORTAL`
- `WIFI_EVENT_BIT_PORTAL_DONE`
- `WIFI_EVENT_BIT_DO_CONNECT`
- `WIFI_EVENT_BIT_NETWORK_CHECK`
- `WIFI_EVENT_BIT_SCAN_REQUEST`

## 自动连接模型

启动后：

- `app_wifi_init()` 创建 Wi-Fi 自动连接 timer。
- 2 秒后 timer 调用 `app_wifi_request_autoconnect(false)`。

自动连接流程：

1. 从 DeviceInfo/NVS 读取保存的 Wi-Fi 凭据。
2. 如果没有凭据，有本地图片时保持离线显示；没有本地图片或用户主动请求时进入配网门户。
3. 按保存顺序逐个尝试连接，每个 SSID 最多等待 10 秒。
4. 连接成功后把该凭据重新保存到最前面。
5. 全部失败时，如果已有本地图片，则显示本地图片；否则进入配网门户。

启动自动连接不区分 gallery mode。设备每次启动或定时唤醒后都会尝试连接已保存的 Wi-Fi；gallery mode 不阻止本次启动连接，连接后的重连和后台扫描仍遵循 gallery mode 策略。

断线恢复规则：

- gallery mode 不进行后台自动重连。
- 非 gallery mode 在省电关闭或存在外部输入时保持在线：稳定连接断开后 5 秒重连，一轮失败后每 60 秒继续尝试。
- 电池供电且省电开启时不安排后台重连，也不持有 Wi-Fi 电源 owner；设备按原有电源管理流程进入深睡，并在下次唤醒时重新执行启动自动连接。
- 没有保存凭据时不进行周期重试。
- 后台重连只恢复网络，不主动刷新图片；DynamicCanvas 仍由原有刷新定时器处理。

## 配网门户模型

手动配网入口：

- `app_input` 识别 Key1 + Key2 同时长按后发布 `APP_INPUT_EVENT_ENTER_PROVISIONING`。
- `app_wifi` 消费该事件，取消 pending wakeup action，停止 Wi-Fi 自动连接 timer。
- `app_wifi` 设置 `is_portal_manual = true`，立即进入 `STATE_PROVISIONING`，请求 View 显示启动页，并设置 `WIFI_EVENT_BIT_START_PORTAL`。
- `APP_WIFI_EVENT_PORTAL_MANUAL` 已删除，手动配网语义不再暴露为 Wi-Fi 公开事件。

`startPortal()` 会：

- 断开当前 Wi-Fi。
- 切到 `WIFI_AP_STA`。
- 根据周边扫描选择干扰最少的 AP channel。
- 创建无密码 SoftAP。
- AP SSID 为 `<HAL apPrefix>-<mac后4位>`。
- 启动 DNS server 和 WebServer。
- 注册 captive portal 常用探测路径。
- 初始触发一次后台扫描。
- 设置 `portal_running = true`。

门户页面内置在 `INDEX_HTML` 字符串中，提供：

- 可用网络列表。
- 保存网络列表。
- 密码提交。
- 忽略已保存网络。
- 刷新扫描。

主要 HTTP 路由：

- `/`：门户首页。
- `/scan`：返回扫描缓存。
- `/refresh_scan`：触发扫描。
- `/saved`：返回已保存 SSID。
- `/submit`：提交 SSID/password 并验证。
- `/ignore`：删除保存的 SSID。
- `/done.html`：配网完成页。

提交凭据：

- `/submit` 会调用 `tryToConnect()`。
- 验证成功后保存凭据，并投递 `APP_WIFI_EVENT_PORTAL_DONE`。
- Portal done 后等待 5 秒，停止门户并投递 `SENSECRAFT_EVENT_START`。

## 扫描模型

扫描缓存类型：`std::vector<CachedAPInfo, util::psram_allocator<CachedAPInfo>>`。

扫描流程：

- `APP_WIFI_EVENT_SCAN` 设置 `WIFI_EVENT_BIT_SCAN_REQUEST`。
- `start_scan()` 清空缓存并调用 `WiFi.scanNetworks(true)` 异步扫描。
- `handle_scan_progress()` 轮询扫描结果，15 秒超时。
- `finalize_scan()` 将 SSID、BSSID、channel、RSSI、authmode 写入缓存。
- 按 RSSI 降序排序。
- 投递 `APP_WIFI_EVENT_SCAN_DONE` 和 `APP_ATCMD_WIFI_SCAN`。

扫描请求在 gallery mode 且门户未运行时会被忽略。

## 指定连接模型

`APP_WIFI_EVENT_CONNECT` 通常来自 AT/BLE 命令：

1. 保存 `ssid/password/origin` 到 `g_wifi_connect_args`。
2. 设置 `WIFI_EVENT_BIT_DO_CONNECT`。
3. WiFi task 调用 `tryToConnect()`。
4. 将结果通过 `APP_ATCMD_WIFI_CONNECT` 返回。

`ProvisioningOrigin` 记录本轮未绑定会话的配网来源：

- Portal 设置为 `WebPortal`。
- `AT+wifi=` 设置为 `BleAt`。
- 没有既有来源的自动连接设置为 `AutoConnect`。
- 绑定完成后清除。

来源只保存在运行时。未绑定设备重启后通过保存的凭据联网，会按 `AutoConnect` 处理并显示激活码。

`tryToConnect()` 依赖扫描缓存：

- 先从扫描缓存中找同名 SSID。
- 选择 RSSI 最强的 BSSID。
- 最多尝试 3 次。
- 成功后可保存凭据。
- 失败时记录 `lastDisconnectReason`。

## 断线恢复模型

Arduino WiFi event：

- `ARDUINO_EVENT_WIFI_STA_GOT_IP` 设置 `WIFI_EVENT_BIT_STA_CONNECTED`。
- `ARDUINO_EVENT_WIFI_STA_DISCONNECTED` 记录 reason，并设置 `WIFI_EVENT_BIT_STA_DISCONNECTED`。

稳定连接断开后：

- 先投递 `SENSECRAFT_EVENT_STOP`。
- gallery mode 下不自动重连。
- 非 gallery mode 下最多重连 `MAX_RETRIES` 次。
- 达到上限后，如果 `disableSleep` 为真则重启，否则进入深睡。

MQTT 断开但 Wi-Fi 仍连接：

- SenseCraft 投递 `APP_MQTT_DISCONNECTED`。
- WiFi 启动网络检查。
- 每 5 秒 ping `223.5.5.5`。
- 成功后投递 `SENSECRAFT_EVENT_RESUME`。
- 连续失败 10 次后当前策略是重启。

## 当前边界问题

- WiFi 模块同时包含连接状态机、门户 HTML/JS、Web API、扫描缓存和网络恢复，后续可以拆出 portal resource / portal controller / connection manager。
- `tryToConnect()` 依赖扫描缓存，缓存为空时无法验证用户输入；这对门户是合理的，但对 AT/BLE 指定连接可能需要更明确的行为。
- 网络检查固定 ping `223.5.5.5`，需要确认目标市场是否合适。
- gallery mode 对自动连接、扫描、重连都有特殊规则；当前通过 `IsLocalGallerySession()` 读取运行上下文，不再直接读取裸全局变量。
