# APP Input 功能模型与重构计划

对应代码：`src/APP/app_input.h`、`src/APP/app_input.cpp`、`src/APP/app_action.h`、`src/APP/app_action.cpp`、`src/APP/app_gallery.h`、`src/APP/app_gallery.cpp`、`src/APP/app_user_action.h`、`src/APP/app_user_action.cpp`

## 第一性原理定位

Input 模块当前不是底层按键驱动。底层按键对象由 Board/HAL 提供，触摸手势也由 HAL 提供。

从第一性原理看，这个模块真正面对的不是“按键”，而是“用户输入”：

- 三个物理按键。
- E1003 等机型的触摸手势。
- 后续可能扩展的其它本机输入源。

因此模块名已经从 `app_button` 收敛为 `app_input`。它最终应该只负责读取输入源、识别输入事件，并向 APP 层发布稳定的用户动作事件。

重构前 Input 模块会把“用户输入”直接转换成 APP 层业务意图：

- 刷新当前内容。
- 切换上一张/下一张图片。
- 进入 Wi-Fi 配网门户。
- 清屏。
- 停止普通深睡计时，标记本次操作来自用户点击。

这个方向比底层驱动更合理，但会越过输入模块的边界。当前重构后，业务消费已移到 View/WiFi/AppAction 等模块，`app_input` 只发布输入事件。

## 对外接口

- `app_input_init()`：注册按键和触摸手势回调，并把轮询任务加入 `MiniRT`。
- `app_gallery_select_next()`：只修改图片索引到下一张。
- `app_gallery_select_prev()`：只修改图片索引到上一张。
- `app_action_init()`：注册主动作输入事件消费。
- `app_user_action_begin()` / `app_user_action_begin_refresh()` / `app_user_action_finish_refresh()`：管理用户主动操作的反馈状态。

当前目标接口：

- `app_input_init()`：注册按键和触摸手势回调，并把轮询任务加入 `MiniRT`。
- 图片索引选择保留在 `app_gallery`，供 View、DeviceInfo、SenseCraft 复用。

## 当前输入模型

输入来源全部通过 HAL：

- `HAL::GetHAL().button(0)`：button1 / Key0。
- `HAL::GetHAL().button(1)`：button2 / Key1。
- `HAL::GetHAL().button(2)`：button3 / Key2。
- `HAL::GetHAL().touchOnGesture()`：触摸手势。
- `HAL::GetHAL().touchLoop()`：由 Input 的 MiniRT task 周期调用。

当前输入映射：

- Key0 短按：发布 `APP_INPUT_EVENT_PRIMARY_ACTION`。
- Key1 短按：发布 `APP_INPUT_EVENT_NEXT_IMAGE`。
- Key2 短按：发布 `APP_INPUT_EVENT_PREV_IMAGE`。
- Key0 长按：发布 `APP_INPUT_EVENT_CLEAR_SCREEN`。
- Key1 + Key2 同时长按：发布 `APP_INPUT_EVENT_ENTER_PROVISIONING`。
- 触摸左滑：发布 `APP_INPUT_EVENT_NEXT_IMAGE`。
- 触摸右滑：发布 `APP_INPUT_EVENT_PREV_IMAGE`。
- 触摸双击：发布 `APP_INPUT_EVENT_PRIMARY_ACTION`。

启动时如果某个按键已经被按住，模块会等待它释放一次后再开始处理该按键，避免唤醒残留按压被误判成点击。

## 目标事件模型

`app_input` 定义输入事件 base 和输入事件类型：

```cpp
ESP_EVENT_DECLARE_BASE(APP_INPUT_EVENT_BASE);

enum app_input_event_id_t {
    APP_INPUT_EVENT_PRIMARY_ACTION,
    APP_INPUT_EVENT_NEXT_IMAGE,
    APP_INPUT_EVENT_PREV_IMAGE,
    APP_INPUT_EVENT_CLEAR_SCREEN,
    APP_INPUT_EVENT_ENTER_PROVISIONING,
    APP_INPUT_EVENT_REQUEST_CLOUD_REFRESH,
};
```

如果后续需要知道事件来源，可带一个轻量数据结构：

```cpp
enum app_input_source_t {
    APP_INPUT_SOURCE_KEY0,
    APP_INPUT_SOURCE_KEY1,
    APP_INPUT_SOURCE_KEY2,
    APP_INPUT_SOURCE_TOUCH,
    APP_INPUT_SOURCE_KEY1_KEY2,
};

typedef struct {
    app_input_source_t source;
} app_input_event_data_t;
```

当前保持直接业务动作事件，不引入复杂 router：

- Key1 短按、触摸左滑：发布 `APP_INPUT_EVENT_NEXT_IMAGE`。
- Key2 短按、触摸右滑：发布 `APP_INPUT_EVENT_PREV_IMAGE`。
- Key0 长按：发布 `APP_INPUT_EVENT_CLEAR_SCREEN`。
- Key1 + Key2 同时长按：发布 `APP_INPUT_EVENT_ENTER_PROVISIONING`。
- Key0 短按、触摸双击：优先发布 `APP_INPUT_EVENT_PRIMARY_ACTION`，由唯一 owner 保持当前“Wi-Fi 未连接则连接，已连接则云端刷新”的语义。

如果产品语义确认改成“Key0/双击只请求云端刷新”，则可以删除 `APP_INPUT_EVENT_PRIMARY_ACTION`，直接发布 `APP_INPUT_EVENT_REQUEST_CLOUD_REFRESH`。不要让多个模块同时消费同一个模糊事件再各自判断。

## 目标消费关系

业务模块各自注册并消费自己关心的输入事件：

- `app_view.cpp`
  - `APP_INPUT_EVENT_NEXT_IMAGE`：选择下一张并刷新显示。
  - `APP_INPUT_EVENT_PREV_IMAGE`：选择上一张并刷新显示。
  - `APP_INPUT_EVENT_CLEAR_SCREEN`：清屏。
- `app_wifi.cpp`
  - `APP_INPUT_EVENT_ENTER_PROVISIONING`：进入手动配网门户。
- `app_action.cpp`
  - `APP_INPUT_EVENT_PRIMARY_ACTION`：根据 Wi-Fi 状态和图片状态转换成 Wi-Fi 连接或云端刷新。
  - `APP_INPUT_EVENT_REQUEST_CLOUD_REFRESH`：转成 `CTRL_EVENT_BASE / MQTT_EVENT_SCREEN_REFRESH`，由 SenseCraft 执行云端刷新请求。

`app_input.cpp` 不再直接投递 View/WiFi/CTRL 事件，也不直接调用这些模块的函数。

## 状态与约束

模块内部维护：

- `button1/button2/button3`：HAL 按键对象指针。
- `button2LongPressActive/button3LongPressActive`：双键长按判断。
- `wait_release_btn1/2/3`：启动时按键释放保护。

重构后 `app_input` 应只保留：

- 3 个 HAL Button 指针。
- 双键长按识别状态。
- 启动时按键释放保护状态。
- 输入轮询任务注册状态。

这些状态应移出 `app_input`：

- 图片索引。
- 用户操作反馈状态。
- timer wakeup / gallery mode 运行上下文。
- Wi-Fi、View、SenseCraft、DeviceInfo、Indicator 的业务状态。

已迁出：

- gallery 点击保护窗口：已移到 View，跟随 `NEXT_IMAGE` / `PREV_IMAGE` 消费。
- 主动作判断：已移到 `app_action`。
- 手动配网动作：已移到 `app_wifi`。
- 用户操作反馈：已移到 `app_user_action`。

图片切换规则：

- 图片总数为 0 时不切换。
- 可选索引范围来自 `app_download_get_manifest_index_min/max()`。
- 当前索引超出范围时重置到最小索引。
- Next 到最大值后回到最小值；Previous 到最小值后回到最大值。
- 成功选择后通过 `SetImageIndex()` 写入当前图片索引。

这些规则不属于输入模块，已迁移到 `app_gallery`。不放进 View 的原因是 DeviceInfo 唤醒逻辑和 SenseCraft 定时刷新也需要复用同一套图片索引选择规则。

点击刷新保护：

- 如果设备处于 `STATE_DOWNLOADING | STATE_REFRESHING | STATE_PROVISIONING | STATE_CLICKING`，忽略图库点击。
- 发出刷新前设置 `STATE_CLICKING`。
- View 刷新结束后通过 `app_user_action_finish_refresh()` 清除用户操作反馈和 `STATE_CLICKING`。

这些保护属于“业务动作是否可执行”，不属于“输入是否发生”。重构后由事件消费者判断。

## 当前事件交互

`app_input` 当前只投递：

- `APP_INPUT_EVENT_BASE / APP_INPUT_EVENT_NEXT_IMAGE`。
- `APP_INPUT_EVENT_BASE / APP_INPUT_EVENT_PREV_IMAGE`。
- `APP_INPUT_EVENT_BASE / APP_INPUT_EVENT_CLEAR_SCREEN`。
- `APP_INPUT_EVENT_BASE / APP_INPUT_EVENT_ENTER_PROVISIONING`。
- `APP_INPUT_EVENT_BASE / APP_INPUT_EVENT_PRIMARY_ACTION`。

## 当前边界问题

- Input 模块已经不再知道 Wi-Fi、SenseCraft、View、DeviceInfo、Indicator 的业务细节。
- 用户操作反馈已从 View 移到 `app_user_action`，并用小状态枚举管理“待完成刷新反馈”。
- timer wakeup / gallery mode 已收口到 `app_device_info` 运行上下文接口，外部模块不再直接读写裸全局变量。
- 无业务闭环的 `__120_secs_timer`、`__5_secs_timer`、`__timer120_time`、`__timer5_time` 已清理。
- Input 不应下沉到 Board/HAL；它依赖的是 HAL 能力，属于 APP 层策略入口。

## 重构计划

### 阶段 0：抽出图片索引模型

- 新增 `app_gallery.h/cpp`。
- `app_gallery_select_next()` / `app_gallery_select_prev()` 承接原 Button 内的图片索引选择逻辑。
- DeviceInfo 唤醒、SenseCraft 定时刷新、Input 切图刷新统一调用 `app_gallery`。
- Input 不再 include `app_download.h`，也不再直接读取 manifest index range。

### 阶段 1：建立输入事件模型

- 已新增 `APP_INPUT_EVENT_BASE` 和 `app_input_event_id_t`。
- 已将 `app_button.h/cpp` 重命名为 `app_input.h/cpp`。
- 已用 `app_input_init()` 替代 `app_button_init()`。
- 已在 `main.cpp` 初始化 `app_input`。
- 保留按键和触摸轮询逻辑，保留启动 wait-release 逻辑。
- 已删除没有业务闭环的 `__120_secs_timer`、`__5_secs_timer`、`__timer120_time`、`__timer5_time`。
- 已完成：Key0 短按和触摸双击发布 `PRIMARY_ACTION`。

### 阶段 2：迁移 View 相关业务

- 已完成：`app_view.cpp` 注册 `APP_INPUT_EVENT_BASE` 并消费 `CLEAR_SCREEN`。
- 已完成：`app_view.cpp` 消费 `NEXT_IMAGE`、`PREV_IMAGE`。
- 已完成：`NEXT_IMAGE` / `PREV_IMAGE` 通过 `app_gallery` 选择图片索引，再请求 View 显示。
- 已完成：当前 busy 状态保护和 250 ms 点击保护的 owner 从 Input 迁到 View。

### 阶段 3：迁移 Wi-Fi 和主动作业务

- 已完成：`app_wifi.cpp` 消费 `ENTER_PROVISIONING`。
- 已完成：`app_action.cpp` 消费 `PRIMARY_ACTION`，保留当前 Key0 “Wi-Fi 未连接则连接，已连接则云端刷新”的语义。
- 已完成：`app_action.cpp` 消费 `REQUEST_CLOUD_REFRESH` 并转成 SenseCraft 的 `MQTT_EVENT_SCREEN_REFRESH`。
- 已完成：Input 不再 include `app_wifi.h`、`app_sensecraft.h`、`app_download.h`、`app_indicator.h`。
- 已完成：双键长按中取消唤醒 action timer、取消 Wi-Fi auto timer 的逻辑已回到对应 owner，不放在 `app_input`。

### 阶段 4：整理用户操作反馈状态

- 已完成：从 View 中移出 `is_button_clicked`。
- 已完成：建立轻量的用户操作反馈接口：
  - `app_user_action_mark_pending()`。
  - `app_user_action_begin()`。
  - `app_user_action_begin_refresh()`。
  - `app_user_action_finish_refresh()`。
- 已完成：内部从 bool 改成小状态枚举，避免继续扩散模糊的 active flag。
- View 刷新结束时只通知“刷新完成”，不拥有“是否来自按钮/触摸”的状态。

### 阶段 5：统一唤醒输入语义

- DeviceInfo 仍负责识别 `WakeupClick`。
- 唤醒后的 Key1/Key2/Touch 行为尽量复用输入事件或同一套图片选择函数。
- 避免运行时按键和深睡唤醒各自维护一套切图语义。

## 不做的事

- 不把 `app_input` 下沉到 HAL 或 Board。
- 不引入复杂的 Input Manager/Command Bus。
- 不让多个模块同时消费同一个含义模糊的主动作事件再各自判断。
- 不为暂时不存在的输入源预留复杂抽象。
