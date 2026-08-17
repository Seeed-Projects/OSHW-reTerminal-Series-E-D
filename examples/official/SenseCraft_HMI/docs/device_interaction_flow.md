# 设备交互流程图

本文基于 `docs/` 下各 APP 功能模型和 `src/main.cpp` 的初始化顺序整理。目标是先看清当前设备交互闭环，再为后续按第一性原理拆模块做收敛。

## 1. 模块边界总览

```mermaid
flowchart LR
    Board["Board\n硬件事实"] --> HAL["HAL\n硬件能力"]
    HAL --> Input["Input\n输入识别"]
    HAL --> View["View\n显示执行"]
    HAL --> DeviceInfo["DeviceInfo\n运行态/配置/电源策略"]
    HAL --> Indicator["Indicator\n用户反馈"]
    HAL --> WiFi["WiFi\n网络可用性"]
    HAL --> SenseCraft["SenseCraft\n云端协议"]
    HAL --> Download["Download\n内容同步执行"]

    Input -->|"APP_INPUT_EVENT_*"| View
    Input -->|"ENTER_PROVISIONING"| WiFi
    Input -->|"PRIMARY_ACTION"| AppAction["AppAction\n主动作裁决"]
    AppAction -->|"WIFI 未连接"| WiFi
    AppAction -->|"WIFI 已连接"| SenseCraft

    WiFi -->|"SENSECRAFT_START/STOP/RESUME"| SenseCraft
    WiFi -->|"SHOW_STARTUP/SHOW_IMAGE"| View
    SenseCraft -->|"manifest_url"| Download
    SenseCraft -->|"activation/waiting"| View
    Download -->|"SHOW_IMAGE"| View
    Download -->|"imgRefreshRes"| SenseCraft
    View -->|"刷新完成/启动深睡计时"| DeviceInfo
    DeviceInfo -->|"唤醒上下文/content mode/local gallery session/状态位"| View
    DeviceInfo -->|"设备事实/状态位"| WiFi
    DeviceInfo -->|"内容事实/状态位"| Download
    DeviceInfo -->|"IoT report/deepsleep event"| SenseCraft
    Indicator -->|"beep/LED pattern"| HAL
```

第一性原理边界：

- Board 只描述硬件事实。
- HAL 只暴露硬件能力和状态。
- APP 模块只处理产品策略和业务状态。
- Input 只发布稳定输入事件，不直接做 Wi-Fi、云端、显示决策。
- View 只执行显示，但当前仍混有 manifest 查图和刷新后休眠策略。
- DeviceInfo 当前实际是运行态、配置、电源策略、唤醒策略聚合点。

## 2. 设备主流程

```mermaid
flowchart TD
    Boot["上电/深睡唤醒"] --> Setup["setup 初始化\nHAL -> Indicator -> DeviceInfoEarly -> Gallery -> View -> DeviceInfo -> Input -> WiFi -> AppAction -> Download -> SenseCraft -> BLE -> ATCMD"]
    Setup --> Wake{"DeviceInfo 解析唤醒原因"}

    Wake -->|"None"| NormalBoot["普通启动"]
    Wake -->|"Key0/Touch"| UserWake["标记用户点击唤醒"]
    Wake -->|"Key1/Key2 且 gallery"| GalleryWake["延迟 2s\n选择下一张/上一张"]
    Wake -->|"Timer"| TimerWake["标记 timer wakeup\n选择下一张"]
    Wake -->|"DoubleLong"| ProvisionWake["通常用于配网\n等待 WiFi/Input owner 处理"]
    Wake -->|"TripleLong"| ShowGallery["进入 gallery\n显示本地图片"]

    NormalBoot --> WifiAuto["WiFi 2s 后自动连接"]
    UserWake --> WifiAuto
    TimerWake --> TimerMode{"当前模式"}
    GalleryWake --> LocalImage["VIEW_SHOW_IMAGE"]
    ShowGallery --> LocalImage

    TimerMode -->|"gallery"| LocalImage
    TimerMode -->|"dashboard"| WifiAuto

    WifiAuto --> WifiDecision{"有保存 Wi-Fi 且允许自动连接"}
    WifiDecision -->|"否"| StartPortal["显示启动页\n进入配网门户"]
    WifiDecision -->|"是"| TrySaved["按保存顺序连接 SSID"]
    TrySaved -->|"成功"| CloudStart["SENSECRAFT_EVENT_START"]
    TrySaved -->|"全部失败且有本地图片"| LocalImage
    TrySaved -->|"全部失败且无图片"| StartPortal

    StartPortal --> Portal["SoftAP + DNS + WebServer\n扫描/提交/忽略已保存网络"]
    Portal -->|"提交凭据成功"| PortalDone["保存 Wi-Fi\n5s 后停止门户"]
    PortalDone --> CloudStart

    CloudStart --> Bind{"SenseCraft 绑定/MQTT 状态"}
    Bind -->|"未绑定或配置无效"| BindApi["HTTP bind"]
    BindApi -->|"需要激活"| Activation["VIEW_SHOW_ACTIVATION_CODE\n10s 轮询绑定"]
    BindApi -->|"绑定成功"| MQTT["MQTT 连接并订阅"]
    Bind -->|"已可用"| MQTT
    MQTT --> FirstRefresh["2s 后首次屏幕刷新\n启动 60s IoT 上报"]

    FirstRefresh --> RefreshReq["MQTT_EVENT_SCREEN_REFRESH"]
    RefreshReq --> CloudResp["云端返回 img_flash/album\nmanifest_url"]
    CloudResp --> DownloadManifest["Download 下载 manifest"]
    DownloadManifest --> PlanContent["解析 manifest\n判断 gallery/dashboard\n生成图片任务"]
    PlanContent --> DownloadImages["下载/复用图片\n上报进度"]
    DownloadImages --> FinishBatch["批次完成\n清状态/写 album version"]
    FinishBatch --> LocalImage

    LocalImage --> Render{"View 显示图片"}
    Render -->|"配网中"| IgnoreImage["忽略显示"]
    Render -->|"找不到图片"| Waiting["VIEW_SHOW_WAITING"]
    Render -->|"DynamicCanvas 离线"| DrawCached["显示本地缓存\n叠加 Wi-Fi 离线图标"]
    Render -->|"正常"| Draw["加锁读取文件\n绘制图片/叠加状态图标"]
    DrawCached --> UserActionDone
    Draw --> UserActionDone["结束用户反馈\n清 STATE_CLICKING"]
    UserActionDone --> SleepTimer
    Waiting --> SleepTimer

    SleepTimer --> SleepCheck{"DeviceInfo 深睡判断"}
    SleepCheck -->|"下载/刷新/配网中"| NoSleep["不睡眠"]
    SleepCheck -->|"充电且编译期允许阻止"| NoSleep
    SleepCheck -->|"disableSleep=true"| NoSleep
    SleepCheck -->|"允许"| DeepSleep["设置 timer/touch/button wakeup\n进入深睡"]

    NoSleep --> MiniRT["MiniRT/FreeRTOS tasks 持续调度"]
    DeepSleep --> Boot
```

## 3. 用户输入链路

```mermaid
flowchart TD
    InputSrc["HAL Button/Touch"] --> Input["app_input\n识别输入"]
    Input -->|"Key1 / 左滑"| Next["APP_INPUT_EVENT_NEXT_IMAGE"]
    Input -->|"Key2 / 右滑"| Prev["APP_INPUT_EVENT_PREV_IMAGE"]
    Input -->|"Key0 短按 / 双击"| Primary["APP_INPUT_EVENT_PRIMARY_ACTION"]
    Input -->|"Key0 长按"| Clear["APP_INPUT_EVENT_CLEAR_SCREEN"]
    Input -->|"Key1+Key2 长按"| Provision["APP_INPUT_EVENT_ENTER_PROVISIONING"]

    Next --> ViewSelect["View 消费\n忙状态/250ms 点击保护"]
    Prev --> ViewSelect
    ViewSelect --> Gallery["app_gallery\n选择索引并写 NVS"]
    Gallery --> ShowImage["VIEW_SHOW_IMAGE"]

    Clear --> ViewClear["View 清屏"]
    ViewClear --> SleepAfterClear["非配网则启动深睡计时"]

    Provision --> WifiPortal["WiFi 消费\n停 auto timer/取消 pending wakeup action"]
    WifiPortal --> PortalState["STATE_PROVISIONING\nVIEW_SHOW_STARTUP\nSTART_PORTAL"]

    Primary --> AppAction["AppAction 唯一裁决"]
    AppAction -->|"Wi-Fi 未连接"| ManualConnect["手动 autoconnect"]
    AppAction -->|"Wi-Fi 已连接"| CloudRefresh["MQTT_EVENT_SCREEN_REFRESH"]
```

优化收敛点：

- `PRIMARY_ACTION` 仍是复合语义。若产品确定 Key0/双击只做云端刷新，应删除 `PRIMARY_ACTION`，直接发布 `REQUEST_CLOUD_REFRESH`。
- 唤醒后的 Key1/Key2/Touch 和运行时输入应继续复用 `app_gallery` 的索引选择规则，避免两套切图语义。

## 4. 内容刷新链路

```mermaid
flowchart TD
    Trigger["触发刷新\n首次 MQTT/用户主动作/周期 timer/云端下发"] --> SenseReq["SenseCraft\nimgRefreshRequest"]
    SenseReq --> Cloud["SenseCraft HMI 云端"]
    Cloud -->|"manifest_url + version + session_id"| AlbumHandler["mqttAlbumHandler\n去重/写 image version"]
    AlbumHandler --> DownloadSet["app_download_set_manifest_url\n取消旧批次/设置新 URL"]
    DownloadSet --> JsonDownload["DOWNLOAD_EVT_JSON_DOWNLOAD\nHTTP GET manifest"]
    JsonDownload -->|"成功"| JsonParse["DOWNLOAD_EVT_JSON_PARSE\n解析 images"]
    JsonDownload -->|"失败"| Fallback["TASK_FAILED\n有缓存则显示本地图片"]
    JsonParse --> Planner["内容规划\ngallery/dashboard\nSD/LittleFS\n任务队列/清理过期图片"]
    Planner --> ImageTasks["DOWNLOAD_EVT_IMAGE_DOWNLOAD"]
    ImageTasks -->|"EXISTING_REPORT"| ReportExisting["上报已存在图片"]
    ImageTasks -->|"IMAGE"| Fetch["HTTP 下载图片\nMD5 校验\n保存文件"]
    Fetch --> Indicator["Indicator\n图片保存反馈"]
    Fetch --> Progress["imgRefreshRes\n20% 进度/100% 完成"]
    Progress --> SenseReport["SenseCraft 发布 img_flash_res"]
    ReportExisting --> TaskDone["任务推进"]
    Indicator --> TaskDone
    TaskDone --> BatchDone{"队列完成"}
    BatchDone -->|"否"| ImageTasks
    BatchDone -->|"是"| Finish["清 STATE_DOWNLOADING/PROVISIONING\n写 album version\nVIEW_SHOW_IMAGE"]
    Fallback --> ViewImage["VIEW_SHOW_IMAGE"]
    Finish --> ViewImage
```

优化收敛点：

- Download 可拆为 `manifest parser`、`content planner`、`download executor`、`storage cleanup`。
- gallery/dashboard 判断依赖 URL 包含 `/img/`，需要确认这是协议契约还是临时规则。
- manifest index range 现在由 Download 暴露给 Gallery。后续可以收敛为独立内容索引模型，给 View、DeviceInfo、SenseCraft 共用。

## 5. 网络与配网链路

```mermaid
flowchart TD
    WifiStart["app_wifi_init\n2s auto timer"] --> Auto{"自动连接触发"}
    Auto --> Credentials["读取最多 5 组 Wi-Fi 凭据"]
    Credentials -->|"无凭据且无本地图片"| Portal["进入配网门户"]
    Credentials -->|"无凭据但有本地图片"| ShowLocal
    Credentials -->|"有凭据"| TryLoop["逐个 SSID 尝试\n每个最多 10s"]
    TryLoop -->|"成功"| SaveFront["成功凭据移到最前"]
    SaveFront --> SenseStart["SENSECRAFT_EVENT_START"]
    TryLoop -->|"失败"| HasImage{"已有本地图片"}
    HasImage -->|"是"| ShowLocal["VIEW_SHOW_IMAGE"]
    HasImage -->|"否"| Portal

    Portal --> SoftAP["WIFI_AP_STA\n选择 AP channel\nSoftAP 无密码"]
    SoftAP --> Web["DNS captive portal\nWebServer routes\n/scan /submit /saved /ignore"]
    Web --> Scan["后台扫描缓存"]
    Web --> Submit["/submit\ntryToConnect"]
    Submit -->|"成功"| SaveWifi["保存凭据\nAPP_WIFI_EVENT_PORTAL_DONE"]
    SaveWifi --> PortalDone["5s 后停止门户"]
    PortalDone --> SenseStart
    Submit -->|"失败"| Web

    MqttDown["MQTT 断开"] --> NetworkCheck["WiFi 网络检查\n每 5s ping 223.5.5.5"]
    NetworkCheck -->|"成功"| SenseResume["SENSECRAFT_EVENT_RESUME"]
    NetworkCheck -->|"连续失败 10 次"| Retry

    StaDown["STA 稳定连接断开"] --> SenseStop["SENSECRAFT_EVENT_STOP"]
    SenseStop --> Retry{"恢复策略"}
    Retry -->|"gallery"| NoReconnect["不自动重连"]
    Retry -->|"非 gallery\n省电关闭或外部供电"| Retry5s["5s 后重连"]
    Retry5s --> TryLoop
    HasImage -->|"连接失败且保持唤醒"| Retry60s["60s 后再次重连"]
    Retry60s --> TryLoop
    Retry -->|"电池供电且省电开启"| DeepSleep["不安排后台重连\n按原流程进入深睡"]
```

优化收敛点：

- WiFi 当前同时包含 STA、SoftAP、WebServer、DNS、扫描缓存、重连策略。迁移到 IDF 时可按 `wifi_station`、`wifi_softap`、`provision_http`、`dns_captive` 拆边界。
- `tryToConnect()` 依赖扫描缓存。门户提交时合理，AT/BLE 指定连接时需要定义缓存为空的行为。
- 固定 ping `223.5.5.5` 需要按目标市场确认。

## 6. 深睡与唤醒链路

```mermaid
flowchart TD
    RenderDone["View 图片/清屏/错误页完成"] --> SleepTimer["StartDeepSleepTimer"]
    ChargingChange["DeviceInfo 检测充电变化"] -->|"开始充电"| StopSleepTimer["停止普通深睡计时"]
    ChargingChange -->|"停止充电且允许睡眠"| SleepTimer
    LowBattery["60s 低电检查\n<= 5% 且未充电"] --> ForceSleep["直接 EnterDeepSleep"]
    SleepTimer --> Enter["EnterDeepSleep"]
    ForceSleep --> Enter

    Enter --> Guard{"进入深睡前判断"}
    Guard -->|"STATE_DOWNLOADING/REFRESHING/PROVISIONING"| StayAwake["保持唤醒"]
    Guard -->|"充电保护命中"| StayAwake
    Guard -->|"普通睡眠被 disableSleep 禁用"| StayAwake
    Guard -->|"允许"| ReportSleep["非 gallery 上报 DEVICE_SLEEP"]
    ReportSleep --> ConfigureWake["配置 timer/touch/button wakeup"]
    ConfigureWake --> Sleep["esp deep sleep"]

    Sleep --> Wake["唤醒"]
    Wake --> Reason{"HAL wakeupReason"}
    Reason -->|"Timer"| TimerCtx["DeviceInfo 标记 timer wakeup"]
    Reason -->|"Key0/Touch"| UserCtx["标记用户唤醒"]
    Reason -->|"Key1/Key2 gallery"| GalleryAction["选择上一张/下一张并显示"]
    Reason -->|"DoubleLong"| ProvisionCtx["进入配网语义"]
```

优化收敛点：

- `disableSleep` 只控制普通自动深睡，不控制低电保护。
- DeviceInfo 当前聚合了运行态、NVS、深睡、低电、唤醒、SD/充电监测。后续优先拆成 `device_state`、`device_config`、`power_policy`、`wakeup_policy`，比继续扩展 `device_info` 更清晰。

## 7. 状态位保护关系

```mermaid
flowchart LR
    State["Device State Bits"] --> Starting["STATE_STARTING"]
    State --> Binding["STATE_BINDING"]
    State --> Refreshing["STATE_REFRESHING"]
    State --> Downloading["STATE_DOWNLOADING"]
    State --> Provisioning["STATE_PROVISIONING"]
    State --> Clicking["STATE_CLICKING"]

    Refreshing --> ViewGuard["View 绘制/清屏/错误页"]
    Downloading --> DownloadGuard["Download 下载图片"]
    Provisioning --> WifiGuard["WiFi 配网门户"]
    Clicking --> UserActionGuard["用户触发刷新反馈"]

    ViewGuard --> SleepGuard["深睡前保护"]
    DownloadGuard --> SleepGuard
    Provisioning --> SleepGuard
    Downloading --> InputGuard["忽略图库点击"]
    Refreshing --> InputGuard
    Provisioning --> InputGuard
    Clicking --> InputGuard
```

后续重构建议先收敛状态 owner：

- 状态位的写入 owner 要明确，避免多个模块写同一个状态但不知道生命周期。
- `STATE_CLICKING` 应继续归 `app_user_action` 收尾，不放回 View 或 Input。
- `STATE_PROVISIONING` 应归 WiFi/portal 生命周期，Download 结束时清它属于历史耦合点，需要后续确认是否仍必要。
