# APP View 功能模型

对应代码：`src/APP/app_view.h`、`src/APP/app_view.cpp`

## 第一性原理定位

View 模块是 APP 层显示执行器。它负责把 APP 事件渲染到墨水屏：

- 启动画面/配网页。
- 激活码页面。
- 等待页面。
- 本地图片页面。
- 清屏页面。
- 错误页面。
- 刷新完成后的用户反馈和普通深睡计时。

它不拥有屏幕硬件事实，而是通过 HAL 获取：

- `display()`。
- 屏幕尺寸、颜色、旋转映射、combo id。
- board profile 中的显示名称。

## 对外接口

- `app_view_init()`：初始化字体和 View 状态，注册事件，绘制初始封面，启动 MiniRT task。
- `imgBufferMutex_lock()` / `imgBufferMutex_unlock()`：下载和显示共享图片资源时使用的互斥锁。
- `app_view_is_showing_activation()`：绑定流程用来判断激活页是否已经显示。

## 事件模型

事件 base：`VIEW_EVENT_BASE`。

View 处理：

- `VIEW_EVENT_SHOW_ACTIVATION_CODE`
- `VIEW_EVENT_SHOW_IMAGE`
- `VIEW_EVENT_SHOW_IMAGE_FAST`
- `VIEW_EVENT_SHOW_STARTUP`
- `VIEW_EVENT_SHOW_WAITING`
- `VIEW_EVENT_SHOW_DOWNLOADING`
- `VIEW_EVENT_SHOW_CLEAR`
- `VIEW_EVENT_SHOW_ERROR`

内部 EventGroup bit：

- `VIEW_EVT_SHOW_ACTIVATION`
- `VIEW_EVT_SHOW_IMAGE`
- `VIEW_EVT_SHOW_FAST`
- `VIEW_EVT_SHOW_STARTUP`
- `VIEW_EVT_SHOW_WAIT`
- `VIEW_EVT_SHOW_DOWNLOAD`
- `VIEW_EVT_SHOW_CLEAR`
- `VIEW_EVT_SHOW_ERROR`

事件 handler 只保存必要数据并设置 bit，实际绘制在 `view_task()` 中执行。

## 页面模型

### 初始封面

`app_view_init()` 会调用 `screen_assets::drawInitialCover()` 绘制初始封面。如果成功，会停留 3 秒。

### 启动/配网页

`VIEW_EVENT_SHOW_STARTUP` 会生成 AP 名称，格式为：

```text
<HAL apPrefix>-<mac后4位>
```

随后根据屏幕信息显示：

- 部分屏幕直接调用 `displayHomeUI()` 显示完整配网页。
- XIAO DIY 小屏通过 `startup_sequence_task()` 轮播 3 个配网页步骤。

### 激活页

`VIEW_EVENT_SHOW_ACTIVATION_CODE` 保存 `ActivationEventData`。

显示时：

- 大屏使用 `displayActivationCodePage()`。
- 小屏使用 `diy_displayActivationCodePage()`。
- 显示完成后清除 `STATE_REFRESHING`。
- `g_showing_activation = true`。

### 等待页

`VIEW_EVENT_SHOW_WAITING` 调用 `screen_assets::drawWaitingScreen()`。

颜色：

- 彩屏使用红底白字。
- 黑白屏使用黑底白字。

### 图片页

`VIEW_EVENT_SHOW_IMAGE` 可携带 `view_show_image_event_data_t`：

- 如果 `has_image_index = true`，先通过 `SetImageIndex()` 写入目标图片索引。
- 否则通过 `GetImageIndex()` 读取当前索引。

图片名解析：

- 从 LittleFS 的 `/manifest.json` 读取 `images` 数组。
- 按 `index` 找目标图片。
- 用 `id` 查找 `<id>.bmp`、`<id>.png` 或 `<id>.epd`。
- 如果 SD 插入，优先按 SD 查找；否则查 LittleFS。

图片文件格式：

- BMP：标准 BMP 文件。
- PNG：标准 PNG 文件。
- EPD：以 `EPD0` 为 magic 的设备专用图片帧。

View 只关心本地文件能否打开和解码，不根据 manifest URL 判断业务资源类型。`render/img`、`render/layout` 等业务资源类型属于 Download/DeviceInfo 的内容模型。

显示规则：

- 如果处于 `STATE_PROVISIONING`，不显示图片。
- 如果找不到图片，转到等待页。
- StaticImage 不依赖网络，直接显示本地文件。
- DynamicCanvas 在 Wi-Fi 未连接时显示本地缓存，并叠加离线图标。
- StaticImage 如果图片名和上一张相同，跳过重复刷新。
- 否则加锁，调用 `drawImageFromFileStream()` 流式解析并绘制。

绘制完成：

- `finalizeImageDraw()` 更新屏幕。
- 当前图片是 DynamicCanvas 且 Wi-Fi 未连接时叠加离线图标，同时按电量状态叠加低电图标。
- 通知 `app_user_action` 完成用户触发刷新反馈。
- 如果 `disableSleep` 为 false，启动普通深睡计时。

### 清屏

`VIEW_EVENT_SHOW_CLEAR`：

- 填白并更新屏幕。
- 如果当前不在配网门户，则启动普通深睡计时。

### 错误页

`VIEW_EVENT_SHOW_ERROR`：

- 显示错误文本。
- 清除 `STATE_REFRESHING`。
- 启动普通深睡计时。

## 共享状态

- `imageBufferMutex`：下载/显示图片互斥。
- `g_savedActivationData`：激活页数据。
- `g_pendingShowImage`：待显示图片索引。
- `g_savedApName`：配网页 AP 名。
- `g_errorMessage`：错误页文本。
- `g_showing_activation`：当前是否显示激活页。
- `g_last_image_name`：gallery 模式下避免重复刷新。
- 用户操作反馈状态已迁移到 `app_user_action`。

## 屏幕适配模型

View 当前已不直接读取板级宏或引脚宏，但仍有基于屏幕信息的分支：

- `screenCombo()` 来自 `HAL::GetHAL().screen().combo_id`。
- `usesDownOrientation()`、`usesLargeLogo()`、`usesInvertedQr()`、`usesFullActivationPage()` 根据屏幕信息决定页面布局。
- XIAO DIY 判断基于 `HAL::GetHAL().boardProfile().device_family`。

这比 APP 直接依赖 `BOARD_SCREEN_COMBO` 更好，但后续还可以继续把“页面布局选择”沉到资源/布局层。

## 当前边界问题

- View 中同时存在事件状态机、页面布局、manifest 查图、图片解析调用和刷新后休眠策略，后续可以先拆页面资源和图片选择逻辑。
- `drawImageFromFileStream()` 的 `partial_update` 参数当前没有参与逻辑，需要确认是否删除或补齐。
- 用户触发刷新后的蜂鸣、LED 和 `STATE_CLICKING` 收尾由 `app_user_action` 管理，View 不再拥有按钮反馈状态。
- `app_get_image_name_by_index()` 每次显示都重新解析 manifest，后续可以由内容模型提供索引到文件名的映射。
- View 当前仍通过 combo id 做布局判断；这不是板级宏依赖，但长期可以改为 HAL/Board 提供更直接的屏幕能力或页面 layout profile。
