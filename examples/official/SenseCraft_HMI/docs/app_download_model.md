# APP Download 功能模型

对应代码：`src/APP/app_download.h`、`src/APP/app_download.cpp`

## 第一性原理定位

Download 是图片资源同步执行器。

云端 manifest 描述目标资源集合：

- 每张图片的唯一资源 ID。
- 每张图片的显示索引。
- 每张图片的下载 URL。
- 每张图片的业务资源类型。

设备本地保存当前资源集合：

- 已存在图片文件。
- 图片文件对应的资源 ID。
- 图片存储位置。
- 图片文件编码格式。
- 可用于切图的图片索引缓存。

Download 的职责是把本地资源集合同步成云端 manifest 描述的目标集合：

```text
manifest 里存在，本地不存在 -> 下载。
manifest 里存在，本地已存在 -> 复用并上报完成。
manifest 里不存在，本地存在 -> 删除。
```

图片是否变化只由 `id` 判断。用户在服务端修改图片后，服务端下发的新 manifest 中对应图片的 `id` 会变化。旧 `id` 不再属于目标集合，会被删除；新 `id` 不在本地，会被下载。这就是图片替换的完整语义。

Download 不决定云端协议，不决定何时请求 manifest，不决定深睡唤醒后是否联网，也不决定轮播策略。它只执行一次 manifest 资源同步。

## Manifest 模型

manifest 基础结构：

```json
{
  "album_id": "20221440",
  "images": [
    {
      "id": "6fe2afae_2026_20257899_20221440_4_800x480",
      "index": 0,
      "url": "https://sensecraft-hmi-api.seeed.cc/render/img/B8:F8:62:F8:D8:00/20257899"
    },
    {
      "id": "1774408390412_20255030_20221440_4_800x480",
      "index": 5,
      "url": "https://sensecraft-hmi-api.seeed.cc/render/layout/B8:F8:62:F8:D8:00/20255030"
    }
  ],
  "version": 2
}
```

字段语义：

- `id`：图片资源唯一身份，用于判断资源是否变化。
- `index`：显示顺序，用于切图翻页。
- `url`：图片下载入口。
- `version`：manifest 协议版本，不作为单张图片变化判断依据。

本地图片文件名以 `id` 为核心：

```text
/<id>.bmp
/<id>.png
/<id>.epd
```

这样扫描本地图片文件时，可以从文件名直接还原资源 ID。

## 三层类型模型

图片资源有三层类型，不能混用：

```text
ResourceKind  -> 服务端资源业务类型。
ImageFormat   -> 下载后的图片文件编码格式。
ContentMode   -> 设备内容模式。
```

### ResourceKind

`ResourceKind` 从 URL 的 `/render/<type>/` 片段解析：

```text
/render/img/...     -> 静态图片
/render/layout/...  -> 动态画布图片
```

建议模型：

```cpp
enum class ResourceKind {
    StaticImage,
    DynamicCanvas,
    UnknownDynamic,
};
```

未知 render 类型视为动态资源，避免把需要联网刷新的内容误判成静态相册。

`ResourceKind` 只表达资源是不是服务端动态生成的，不表达图片文件格式。

### ImageFormat

`ImageFormat` 从下载内容本身判断，优先看文件头，不只依赖 `Content-Type`：

```text
BMP -> 文件头 "BM"
PNG -> 89 50 4E 47 0D 0A 1A 0A
EPD -> 文件头 "EPD0"
```

建议模型：

```cpp
enum class ImageFormat {
    Unknown,
    BMP,
    PNG,
    EPD,
};
```

保存后缀：

```text
BMP -> .bmp
PNG -> .png
EPD -> .epd
```

`EPD` 是图片编码格式，不是业务资源类型。以下组合都允许存在：

```text
/render/img/...     -> 返回 BMP / PNG / EPD
/render/layout/...  -> 返回 BMP / PNG / EPD
```

## 内容模式

`ContentMode` 由 manifest 中 `ResourceKind` 集合决定：

```text
全部图片都是 StaticImage                    -> Gallery
存在任意 DynamicCanvas 或 UnknownDynamic    -> Dashboard
```

语义：

- `Gallery`：纯静态图片相册。图片内容不会随时间变化，后续自动轮播可以本地切图。
- `Dashboard`：存在动态画布图片。图片可能由服务端根据 API 或实时数据渲染，后续自动轮播需要由外部调度决定是否联网请求更新。

Download 只负责把模式写入 DeviceInfo，作为其它模块决策的事实输入：

- `SetContentMode(AppContentMode::Gallery)`
- `SetContentMode(AppContentMode::Dashboard)`
- `SetLocalGallerySession(...)`

Download 不在下载流程里判断深睡唤醒后是否联网，也不根据唤醒来源裁剪下载任务。

## 对外接口

- `app_download_init()`：初始化 Download 事件、状态和任务。
- `app_download_set_manifest_url(const String &url, const String &version)`：取消当前同步，保存新的 manifest URL 和版本，开始新一轮资源同步。
- `app_download_is_same_manifest_in_progress(const char *version, const char *url)`：判断同一 manifest 是否正在同步，用于避免重复触发。
- `app_download_get_manifest_index_min()`：读取当前 manifest 图片最小显示索引。
- `app_download_get_manifest_index_max()`：读取当前 manifest 图片最大显示索引。

接口保持少量、直接。不要把 manifest 解析、下载任务、存储策略等内部细节暴露出去。

## 数据模型

建议保留单文件实现，先用简单结构表达真实概念。

### ManifestImage

```cpp
struct ManifestImage {
    String id;
    String url;
    int index;
    int order;
    ResourceKind kind;
};
```

- `id`：资源身份。
- `url`：下载入口。
- `index`：显示索引。
- `order`：manifest 中第几项，用于上报进度。
- `kind`：服务端资源业务类型。

### LocalImage

```cpp
struct LocalImage {
    String id;
    String path;
    bool onSd;
    ImageFormat format;
};
```

- `id`：由本地文件名解析得到。
- `path`：完整文件路径。
- `onSd`：是否存储在 SD 卡。
- `format`：本地文件编码格式。

### ImageTask

```cpp
enum class ImageTaskType {
    Reuse,
    Download,
};

struct ImageTask {
    ImageTaskType type;
    String id;
    String url;
    String path;
    int index;
    int order;
    bool storeOnSd;
    ImageFormat format;
};
```

- `Reuse`：本地已有同 `id` 图片，标记可用并上报完成。
- `Download`：本地缺少该 `id` 图片，需要下载保存。

## 存储策略

存储策略只决定图片文件落在哪里，不参与资源是否变化判断。

建议策略：

```text
Gallery   -> 优先 SD，SD 不可用时使用 LittleFS。
Dashboard -> 优先 LittleFS，保证深睡唤醒和动态图展示路径稳定。
```

如果保存到 SD 失败，可以回退到 LittleFS。

清理策略要扫描当前同步使用的存储位置，也要清理另一存储中同类历史图片，避免切换模式或插拔 SD 后留下旧资源。

## 主流程

```text
app_download_set_manifest_url()
  -> cancelCurrentSync()
  -> downloadManifest()
  -> parseManifest()
  -> decideContentMode()
  -> applyManifestState()
  -> scanLocalImages()
  -> pruneObsoleteImages()
  -> buildImageTasks()
  -> runImageTasks()
  -> finishSync()
```

### 1. 设置 manifest URL

`app_download_set_manifest_url(url, version)`：

- 取消当前同步批次。
- 保存新的 manifest URL 和 manifest version。
- 获取 Download 功耗保持。
- 触发 manifest 下载。

### 2. 下载 manifest

`downloadManifest()`：

- 等待 Wi-Fi 可用。
- 通过 HTTP GET 下载 manifest。
- 使用 `GetCloudToken()` 作为 authorization header。
- 写入 LittleFS `/manifest.json`。
- 下载失败则进入失败恢复。

manifest 是本轮同步的输入快照，后续解析都从 `/manifest.json` 读取。

### 3. 解析 manifest

`parseManifest()`：

- 读取 `/manifest.json`。
- 限制最大大小为 `MANIFEST_MAX_SIZE_BYTES`。
- 解析 `images` 数组。
- 读取每项的 `id`、`url`、`index`。
- 根据 URL 解析 `ResourceKind`。
- 计算 `index` 的最小值和最大值。
- 生成 `ManifestImage` 列表。

无效图片项跳过；如果数组非空但没有任何有效图片，则本轮同步失败。

`images=[]` 表示内容暂未生成或相册为空。设备显示等待页面，并按 2 秒、5 秒、10 秒重新下载同一个 manifest。重试后仍为空则结束本轮同步，保留等待页面，不回退显示旧缓存内容。

### 4. 应用 manifest 状态

`applyManifestState()`：

- 根据 `ResourceKind` 集合写入内容模式。
- 写入图片数量。
- 修正当前显示索引：
  - 当前索引不在 manifest index 范围内时，回到最小 index。
  - 从 Gallery 切到 Dashboard 时，也回到最小 index。
- 标记 manifest index range 有效。

`index` 是显示顺序，不参与资源变化判断。

### 5. 扫描本地图片

`scanLocalImages()`：

- 扫描 LittleFS 和 SD 中的 `.bmp` / `.png` / `.epd` 图片。
- 从文件名解析资源 `id`。
- 建立 `id -> LocalImage` 索引。

如果同一 `id` 在多个存储位置同时存在，优先选择当前模式对应的存储位置。

### 6. 清理过期图片

`pruneObsoleteImages()`：

- 计算 manifest 目标 `id` 集合。
- 删除本地存在但 manifest 不存在的图片。
- 删除失败只记录日志，不中断整个同步。

清理只基于 `id` 集合，不基于 `index` 或 manifest version。

### 7. 生成图片任务

`buildImageTasks()`：

```text
for image in manifest.images:
    if local contains image.id:
        tasks.push(Reuse)
    else:
        tasks.push(Download)
```

任务按 manifest 顺序执行。

不要根据 timer wakeup、button wakeup、Gallery/Dashboard 分支裁剪任务。Download 的目标是同步 manifest 资源集合；是否在某次唤醒联网请求 manifest，由外部模块决定。

### 8. 执行图片任务

`runImageTasks()`：

- `Reuse`：
  - 标记图片可用。
  - 使用本轮目标版本调用 `imgRefreshRes()` 上报完成。

- `Download`：
  - HTTP GET 下载图片到 PSRAM。
  - 根据文件头判断 BMP/PNG/EPD。
  - `Content-Type` 只作为辅助日志，不作为唯一依据。
  - 未识别格式视为无效图片，不保存、不标记可用。
  - 如果存在 `x-content-md5` header，则校验 MD5。
  - 每 20% 使用本轮目标版本调用 `imgRefreshRes()` 上报进度。
  - 按 `ImageFormat` 后缀保存到目标存储。
  - 保存成功后标记图片可用。
  - 使用本轮目标版本调用 `imgRefreshRes()` 上报完成。
  - 单张图片失败最多重试 3 次。

保存图片时使用 View 的图片缓冲互斥锁，避免 View 同时读取图片文件。

### 9. 结束同步

`finishSync()`：

- 释放 Download 功耗保持。
- 清空任务队列和临时缓冲。
- 如果本轮同步成功：
  - 写入内容版本和 album version。
  - 重建 Gallery 图片缓存。
  - 通知 View 展示图片。
- 如果本轮失败：
  - 保留已存在缓存图片。
  - 如果本地有可显示图片，通知 View 展示本地缓存。

## 事件模型

事件 base：`DOWNLOAD_EVENT_BASE`。

外部和内部事件：

- `DOWNLOAD_EVT_JSON_DOWNLOAD`：下载 manifest。
- `DOWNLOAD_EVT_JSON_PARSE`：解析 manifest。
- `DOWNLOAD_EVT_IMAGE_DOWNLOAD`：执行图片同步任务。
- `DOWNLOAD_EVT_TASK_FINISHED`：同步结束。
- `DOWNLOAD_EVT_TASK_FAILED`：manifest 下载、解析或图片同步失败。
- `DOWNLOAD_EVT_TASK_CANCEL`：取消当前同步。

内部 EventGroup bit：

- `DL_EVT_JSON_DOWNLOAD`
- `DL_EVT_JSON_PARSE`
- `DL_EVT_IMAGE_DOWNLOAD`
- `DL_EVT_TASK_FINISHED`
- `DL_EVT_TASK_CANCEL`

事件命名要表达业务阶段，不要泄露旧实现的临时状态。

## 与其他模块的关系

- SenseCraft：传入 manifest URL 和版本，接收下载进度/完成上报。
- DeviceInfo：保存内容模式、图片数量、当前索引、内容版本、album version、图片存在状态。
- Gallery：维护可切换图片索引缓存。
- View：展示同步完成后的图片，并提供图片缓冲互斥锁。
- PowerManager：下载期间保持活跃，结束后释放。
- HAL：判断 SD 状态，执行 SD 文件操作。

## 重构原则

- 不兼容旧模型。
- 不按唤醒来源裁剪下载任务。
- 不用 manifest version 判断单张图片变化。
- 不用 `index` 判断图片是否变化。
- 不用 `ResourceKind` 判断文件后缀。
- 不用 `ImageFormat` 判断 Gallery/Dashboard。
- 不把云端请求调度逻辑塞进 Download。
- 不为了分层而引入空壳类。
- 单文件先收敛，只有概念清晰后再考虑拆文件。
- 对无多方依赖的小逻辑直接内聚。
- 真正不需要的中间状态和临时结构直接删除。
