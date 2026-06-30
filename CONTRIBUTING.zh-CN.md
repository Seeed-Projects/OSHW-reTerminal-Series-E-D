# 贡献指南

> **Note**: This is a Chinese translation of [CONTRIBUTING.md](CONTRIBUTING.md).
> When in doubt, the English version is the source of truth.
>
> **说明**：这是 [CONTRIBUTING.md](CONTRIBUTING.md) 的中文翻译。
> 如有歧义，以英文版为准。

本指南是在 reTerminal E-Series Firmware Hub 中添加或更新固件示例的操作手册。它同时面向人工贡献者和 AI 编码代理。

如果你只记住一条规则：请添加或编辑示例源码，以及让它在网页上可见的网页元数据。不要手动发布固件。GitHub Actions 会自动构建、版本化、部署并发布固件。

## 从这里开始

选择与你的贡献内容匹配的路径。

| 目标 | 是否允许？ | 主要需要修改的文件 |
|---|---:|---|
| 添加新的核心硬件示例 | 是 | `examples/base/<Demo>/`，可选 `web/js/firmwares.js` |
| 添加新的官方合作伙伴/平台示例 | 是 | `examples/official/<Project>/`、`web/js/firmwares.js`，可选 `.github/scripts/firmware_release.py`。先选择：**烧录模式**、**模板模式**或 **官方工具模式**。 |
| 添加新的社区项目 | 是 | `examples/community/<Project>/`、`web/js/firmwares.js`，可选 `.github/scripts/firmware_release.py`。先选择：**烧录模式**或 **模板模式**。 |
| 更新已有示例 | 是 | 仅修改对应的示例文件夹；如果 UI 有变化，再修改匹配的网页元数据 |
| 添加新的平台组/分类 | 否 | 不要编辑 `PLATFORM_GROUPS` |
| 手动创建固件版本或发布版本 | 否 | 不要编辑生成出来的固件输出 |

## 允许的 Hub 分类

Firmware Hub 目前有三个组。

| 组 | `group` 值 | 用途 |
|---|---|---|
| 官方平台 | `official` | 官方平台集成或合作伙伴工作流，例如 ESPHome、SquareLine Vision 和 OpenDisplay |
| 基础 | `base` | 第一方硬件 bring-up（通俗解释：让硬件基本跑起来并验证功能）示例：RTC、睡眠、按钮、蜂鸣器、显示、传感器、SD 卡、麦克风 |
| 社区项目 | `community` | 基于设备技术栈构建的社区应用或贡献的终端用户项目 |

不要创建新的组。需要新组的贡献应该先发起讨论或 issue。

## 仓库地图

| 路径 | 用途 | 贡献者操作 |
|---|---|---|
| `examples/` | 固件源码 | 在这里添加或更新示例代码 |
| `web/js/firmwares.js` | Hub 分组、平台卡片、固件下拉选项、配置字段 | 当网页必须展示或描述某个项目时编辑 |
| `.github/scripts/firmware_release.py` | 构建注册表和发布辅助脚本 | 仅在 PlatformIO、多设备、特殊库或按设备构建时编辑 |
| `.github/workflows/build-and-deploy.yml` | 共享的 GitHub Actions 工作流 | 普通示例贡献不要编辑 |
| `firmware/` on `gh-pages` | 生成出来的固件版本、manifest 和 catalog | 不要手动编辑 |

## 示例文件夹布局

新示例应使用分组布局：

```
examples/
  base/
    WiFi_Scanner/
      WiFi_Scanner.ino
  official/
    MyOfficialProject/
      ...
  community/
    MyCommunityProject/
      ...
```

本仓库中的所有示例现在都应该放在这些分组文件夹之一下面。不要再添加新的扁平 `examples/<Name>/` 文件夹。

## 自动化模型

```
Contributor changes examples/<group>/<Example>/
        |
        v
GitHub Actions detects changed example folders
        |
        v
Only changed firmware targets are built
        |
        v
Changed firmware gets a new date version
        |
        v
gh-pages receives updated web files, versions.json, catalog.json, and manifests
        |
        v
GitHub Release includes the latest firmware package for every example
```

未变更的示例不会重新编译。它们会保留之前已发布的最新版本，并且仍会包含在完整发布包中。

## 固件版本规则

固件版本按固件 ID 分别跟踪。

示例：

```
firmware/ePaper_VoiceMemo_E1001/2026.06.07/
firmware/ePaper_VoiceMemo_E1001/2026.06.12/
firmware/RTC_PCF8563/2026.06.07/
```

规则：

- 某个固件在某一天的第一次构建使用 `YYYY.MM.DD`。
- 如果同一个固件在同一天再次重建，下一个版本是 `YYYY.MM.DD.1`，然后是 `YYYY.MM.DD.2`，以此类推。
- 官方外部平台固件如果必须让网页版本号匹配上游固件版本，可以在 `.github/scripts/firmware_release.py` 中设置 `fixed_version`。
- 网页会读取 `firmware/versions.json`，并默认选择最新版本。
- 新构建不会创建单独的 `latest` 文件夹。
- 旧的 `latest` 固件可能会被复制一次到日期版本中，这样旧的已发布固件无需重建也能继续可用。

## 场景 1：添加标准 Base Arduino 示例

简单的第一方硬件示例如果属于 Base，请使用这条路径。

### 1. 创建示例文件夹

文件夹名称和 `.ino` 文件名必须完全一致。

```
examples/
  base/
    WiFi_Scanner/
      WiFi_Scanner.ino
      README.md
```

这种标准形状可以被 GitHub Actions 自动发现。它会作为一个默认 Arduino 固件目标构建，目标名为 `WiFi_Scanner`，适用于 E1001、E1002、E1003 和 E1004。

### 2. 添加本地文档

当设置步骤不是显而易见时，添加 `examples/base/WiFi_Scanner/README.md`。包含：

- 支持的设备，
- 所需硬件，
- 所需外部服务，
- 预期串口输出，
- 任何本地编译说明。

### 3. 判断是否需要网页元数据

如果这是一个非常简单的标准 Arduino 示例，GitHub Actions 可以生成 `firmware/catalog.json`，并且在固件发布后，网页应用可以把它追加到 Base 固件列表中。

自动发现只用于简单的 Base Arduino 示例。官方平台项目和社区项目必须在 `web/js/firmwares.js` 中手动注册；否则它们不会出现在正确的分组下面。

在需要以下任意内容时，改为编辑 `web/js/firmwares.js`：

- 更精致的显示名称，
- 清晰的描述，
- 按设备区分的兼容性，
- 警告或说明，
- 配置字段，
- 同一个源码文件夹对应多个固件选项。

## 场景 2：为官方平台添加示例

官方合作伙伴集成或受支持的平台工作流，请使用这条路径。不要创建新的组。请在现有 `official` 组下面添加平台卡片，或给已有官方平台卡片添加固件选项。

### 0. 选择展示模式

写任何代码之前，先决定用户将如何使用你的贡献：

| 问题 | 答案：烧录模式 | 答案：模板模式 | 答案：官方工具模式 |
|---|---|---|---|
| 你的项目会产出可直接运行的 `.bin` 固件吗？ | 是 | 生成配置文件 | 由上游平台维护 |
| 用户如何完成设置？ | 从浏览器烧录 | 导出配置文件，再使用目标工具链 | 打开上游固件或工具箱页面 |
| 用户得到什么？ | 设备上可运行的固件 | 一个可在别处编辑和编译的入门配置文件 | 直接进入官方平台工具 |

**烧录模式**：设置 `installReady: true`，提供带有构建 ID 的 `firmwareOptions`，并在 `firmware_release.py` 中注册构建目标。GitHub Actions 会构建二进制文件；Hub 会通过 Web Serial 把它烧录到设备。

**模板模式**：设置 `installReady: false` 和 `templateMode: true`，提供带有 `snippet` 字段的 `templateOptions`。不需要构建目标。Hub 会在浏览器中生成一个配置文件，供用户预览、复制或下载。完整数据结构请见 [模板模式平台](#模板模式平台)。

**官方工具模式**：设置 `installReady: false`，本地固件字段留空，并提供 `externalTool`。Hub 会在 Step 2 显示官方固件或工具箱入口，并使用官方流程完成后续设置。完整字段参考请见 [官方工具平台](#官方工具平台)。

大多数需要每个用户单独自定义的官方平台（例如 ESPHome，每个用户的显示布局和传感器设置都不同）应该使用模板模式。

当上游平台已经维护固件安装器、浏览器工具箱或配置流程时，请使用官方工具模式。

### 1. 添加示例源码

将源码放在 `examples/official/<Project>/` 下面。

Arduino:

```
examples/official/MyOfficialDemo/
  MyOfficialDemo.ino
  README.md
```

PlatformIO:

```
examples/official/MyOfficialProject/
  platformio.ini
  src/
    main.cpp
  README.md
```

### 2. 注册网页卡片或固件选项

打开 `web/js/firmwares.js`。

如果是新的官方平台卡片，向 `PLATFORM_CARDS` 添加一个对象。

**烧录模式**（完整固件，可通过浏览器烧录）：

```js
{
  id: "my-official-platform",
  group: "official",
  name: "My Official Platform",
  tagline: "Short workflow summary.",
  source: { label: "Official website", url: "https://example.com" },
  wiki: { label: "Wiki", url: "https://wiki.seeedstudio.com/example/" },
  description: "What this platform does and when to use it.",
  logo: "assets/platforms/my-official-platform-logo.png",
  preview: "assets/platforms/my-official-platform-preview.png",
  previewAlt: "My Official Platform preview",
  accent: "#004966",
  highlight: "#8FC31F",
  supportedDevices: ["E1001", "E1002"],
  installReady: true,           // <-- flash mode
  bullets: ["Key workflow point", "Required setup", "Best-fit use case"],
  versions: [],
  configFields: [],
  firmwareOptions: []           // <-- add firmware options below
}
```

**模板模式**（示例配置，用户离线自定义）：

```js
{
  id: "my-official-platform",
  group: "official",
  name: "My Official Platform",
  tagline: "Short workflow summary.",
  source: { label: "Official website", url: "https://example.com" },
  wiki: { label: "Wiki", url: "https://wiki.seeedstudio.com/example/" },
  description: "What this platform does and when to use it.",
  logo: "assets/platforms/my-official-platform-logo.png",
  preview: "assets/platforms/my-official-platform-preview.png",
  previewAlt: "My Official Platform preview",
  accent: "#004966",
  highlight: "#8FC31F",
  supportedDevices: ["E1001", "E1002"],
  installReady: false,          // <-- no browser flashing
  templateMode: true,           // <-- template mode
  templateHeader: "...",
  templateFooter: "...",
  templateFileExtension: "yaml",
  templateFileMimeType: "text/yaml",
  templateFilePattern: "{platformId}-{deviceId}",
  templateOptions: [],          // <-- add template options with snippets
  bullets: ["Config file output", "User customization required", "Best-fit use case"],
  versions: [],
  configFields: [],
  firmwareOptions: []
}
```

完整字段参考和可运行示例请见 [模板模式平台](#模板模式平台)。

**官方工具模式**（上游固件或工具箱，用户到官方工具继续）：

```js
{
  id: "my-official-platform",
  group: "official",
  name: "My Official Platform",
  tagline: "Short workflow summary.",
  source: { label: "Official website", url: "https://example.com" },
  wiki: { label: "Wiki", url: "https://wiki.seeedstudio.com/example/" },
  description: "What this platform does and when to use it.",
  externalTool: {
    label: "Open official toolbox",
    url: "https://example.com/toolbox",
    title: "Use the official toolbox",
    description: "Continue with the official platform tool to install firmware, configure the workflow, and manage device content."
  },
  logo: "assets/platforms/my-official-platform-logo.png",
  preview: "assets/platforms/my-official-platform-preview.png",
  previewAlt: "My Official Platform preview",
  accent: "#004966",
  highlight: "#8FC31F",
  supportedDevices: ["E1001", "E1002"],
  installReady: false,
  bullets: ["Official firmware tool", "Browser-based workflow", "Upstream-maintained setup"],
  versions: [],
  configFields: [],
  firmwareOptions: []
}
```

官方平台卡片必须提供 `source` 字段。用它链接到官方产品或项目网站。如果平台有 Seeed Studio Wiki 教程，请添加 `wiki` 字段。

对于烧录模式固件选项，请把它添加到平台卡片的 `firmwareOptions` 数组：

```js
{
  id: "MyOfficialDemo_E1001",
  name: "My Official Demo",
  description: "What the firmware does.",
  category: "Application",
  compatible: ["E1001"],
  configFields: [],
  notes: [
    { type: "info", text: "What users should know before flashing." }
  ]
}
```

`id` 必须匹配 GitHub Actions 构建出来的固件 ID。

### 3. 注册非标准构建目标

如果项目是 PlatformIO、需要按设备构建、需要编译标志，或需要特殊库，请在 `.github/scripts/firmware_release.py` 的 `FIRMWARE_TARGETS` 下面添加条目。

不要为此编辑 `.github/workflows/build-and-deploy.yml`。

## 场景 3：添加社区项目

贡献的应用或示例如果不是官方平台集成，请使用这条路径。

### 0. 选择展示模式

写任何代码之前，先决定用户将如何使用你的贡献：

| 问题 | 答案：烧录模式 | 答案：模板模式 |
|---|---|---|
| 你的项目会产出可直接运行的 `.bin` 固件吗？ | 是 | 否 |
| 用户能否无需修改、直接从浏览器烧录它？ | 是 | 否，每个用户都需要自定义配置 |
| 用户得到什么？ | 设备上可运行的固件 | 一个可在别处编辑和编译的入门配置文件 |

**烧录模式**：设置 `installReady: true`，提供 `firmwareOptions`，并注册构建目标。Hub 会构建并烧录二进制文件。

**模板模式**：设置 `installReady: false` 和 `templateMode: true`，提供带有 `snippet` 字段的 `templateOptions`。Hub 会生成一个配置文件，供用户预览、复制或下载。完整参考请见 [模板模式平台](#模板模式平台)。

### 1. 添加源码

将源码放在 `examples/community/<Project>/` 下面。

对于标准 Arduino 社区项目，请使用这个精确形状：

```
examples/community/Example/
  Example.ino
  README.md
```

默认固件构建 ID 是 `Example`。请在 `firmwareOptions[].id` 中使用这个精确值。

对于 PlatformIO 项目、按设备构建、特殊库、编译标志，或与文件夹名称不同的固件 ID，请在 `.github/scripts/firmware_release.py` 中添加明确的 `FirmwareTarget` 条目。

### 2. 添加或更新 Community Projects 卡片

打开 `web/js/firmwares.js`。

对于完整项目，添加一个平台卡片。从第 0 步中选择与你的展示模式匹配的模板。

**烧录模式**（完整固件）：

```js
{
  id: "my-community-project",
  group: "community",
  name: "My Community Project",
  tagline: "Short project summary.",
  author: "github-user-or-team",
  source: {
    label: "github-user-or-team/my-community-project",
    url: "https://github.com/github-user-or-team/my-community-project"
  },
  description: "What this project does.",
  logo: "assets/brand/reterminal-epaper-icon.svg",
  preview: "assets/devices/reterminal-e1003.jpg",
  previewAlt: "My Community Project preview",
  accent: "#004966",
  highlight: "#8FC31F",
  supportedDevices: ["E1001", "E1002", "E1003"],
  installReady: true,           // <-- flash mode
  bullets: ["Community project", "Main user value", "Required setup"],
  versions: [],
  configFields: [],
  firmwareOptions: []           // <-- add firmware options below
}
```

然后添加一个或多个 `firmwareOptions`，其 ID 必须匹配构建目标。

**模板模式**（示例配置）：

```js
{
  id: "my-community-project",
  group: "community",
  name: "My Community Project",
  tagline: "Short project summary.",
  author: "github-user-or-team",
  source: {
    label: "github-user-or-team/my-community-project",
    url: "https://github.com/github-user-or-team/my-community-project"
  },
  description: "What this project does.",
  logo: "assets/brand/reterminal-epaper-icon.svg",
  preview: "assets/devices/reterminal-e1003.jpg",
  previewAlt: "My Community Project preview",
  accent: "#004966",
  highlight: "#8FC31F",
  supportedDevices: ["E1001", "E1002", "E1003"],
  installReady: false,          // <-- no browser flashing
  templateMode: true,           // <-- template mode
  templateHeader: "...",
  templateFooter: "...",
  templateFileExtension: "yaml",
  templateFileMimeType: "text/yaml",
  templateFilePattern: "{platformId}-{deviceId}",
  templateOptions: [],          // <-- add template options with snippets
  bullets: ["Community project", "Config file output", "User customization required"],
  versions: [],
  configFields: [],
  firmwareOptions: []
}
```

完整字段参考请见 [模板模式平台](#模板模式平台)。

Community Projects 必须提供 `author` 和 `source` 字段。它们会显示在卡片上，用来标注原始创建者，并链接回上游项目。

### 3. 不要把密钥写进代码

如果项目需要 Wi-Fi、API key 或 token，请使用 `configFields` 和 NVS。不要提交真实密钥。

### 4. 试运行构建计划

打开 pull request 之前，试运行发布规划器：

```bash
python3 .github/scripts/firmware_release.py plan \
  --changed-file examples/community/Example/Example.ino \
  --output-file /tmp/example-plan.json
```

对于 PlatformIO，这个试运行只有在项目已经有匹配的 `FirmwareTarget` 条目后，才会报告固件变更。

## 模板模式平台

当平台生成配置文件（例如 YAML 或 JSON），而不是直接烧录固件时，使用模板模式。当工具通过 Web Serial 安装已编译好的固件二进制文件时，使用烧录模式。

模板模式会把所有模板数据保存在 `web/js/firmwares.js` 中。浏览器会把平台级文本和用户选择的片段组装成最终文件。

### 模板模式平台字段

| Field | Required | Default | Description |
|---|---:|---|---|
| `templateMode` | Yes | — | 必须是 `true` |
| `templateHeader` | No | `""` | 始终添加到输出开头的文本 |
| `templateFooter` | No | `""` | 始终添加到输出末尾的文本 |
| `templateFileExtension` | No | `"txt"` | 下载文件的扩展名 |
| `templateFileMimeType` | No | `"text/plain"` | 下载文件的 MIME 类型 |
| `templateFilePattern` | No | `"{platformId}-{deviceId}"` | 文件名模式 |
| `templateJoiner` | No | `"\n\n"` | 各部分之间的分隔符 |
| `templateOptions` | Yes | — | 功能选项数组 |

### 模板选项字段

| Field | Required | Description |
|---|---:|---|
| `id` | Yes | 唯一标识符 |
| `label` | Yes | 复选框标签 |
| `description` | Yes | 标签下方的辅助说明 |
| `defaultChecked` | No | 设为 `true` 时默认勾选 |
| `snippet` | Yes | 勾选时包含的文本内容 |

文件名模式支持这些 token：

- `{platformId}` 会变成平台的 `id` 字段。
- `{deviceId}` 会变成已选设备 ID，并转换为小写。

最小模板模式平台示例：

```js
{
  id: "example-config",
  group: "official",
  name: "Example Config",
  tagline: "Generate a starter config file.",
  source: {
    label: "Example docs",
    url: "https://example.com/docs"
  },
  description: "Creates a local configuration file for the selected device.",
  logo: "assets/brand/reterminal-epaper-icon.svg",
  preview: "assets/devices/reterminal-e1003.jpg",
  previewAlt: "Example Config preview",
  accent: "#004966",
  highlight: "#8FC31F",
  supportedDevices: ["E1003"],
  installReady: false,
  bullets: [
    "Config file output",
    "Local editing workflow",
    "No browser flashing"
  ],
  versions: [],
  configFields: [],
  templateMode: true,
  templateHeader: "device: {device_name}",
  templateFooter: "# Save this file before importing.",
  templateFileExtension: "yaml",
  templateFileMimeType: "text/yaml",
  templateFilePattern: "example-config-{deviceId}",
  templateOptions: [
    {
      id: "network",
      label: "Network",
      description: "Add network settings",
      defaultChecked: true,
      snippet: "network:\\n  enabled: true"
    }
  ],
  firmwareOptions: []
}
```

模板模式平台不需要 `.github/scripts/firmware_release.py` 中的固件构建目标、`versions.json` 条目或 `manifest.json` 文件。它们完全在客户端侧完成。

## 官方工具平台

当上游平台已经维护固件安装器、浏览器工具箱或配置流程时，使用官方工具模式。Hub 会显示已选平台和设备，然后在 Step 2 链接到官方工作流。

### 官方工具平台字段

| Field | Required | Default | Description |
|---|---:|---|---|
| `externalTool` | Yes | — | 描述官方目标页面的对象 |
| `externalTool.label` | Yes | — | 按钮文字 |
| `externalTool.url` | Yes | — | 按钮打开的绝对 URL |
| `externalTool.title` | Yes | — | Step 2 卡片内的标题 |
| `externalTool.description` | Yes | — | 说明官方工具入口的一段文字 |

官方工具平台使用 `installReady: false`，并保持 `versions`、`configFields` 和 `firmwareOptions` 为空。网页会为这些平台隐藏本地烧录面板。

## 场景 4：更新已有示例

修改已经存在的代码时，使用这条路径。

1. 只编辑匹配的示例文件夹：

   ```
   examples/<group>/<ExistingExample>/
   ```

2. 如果变更影响面向用户的设置，也要更新该示例的 README。

3. 如果变更影响网页文字、兼容设备、说明或配置字段，请更新 `web/js/firmwares.js`。

4. 不要触碰无关的示例文件夹。GitHub Actions 会根据变更的文件夹决定哪些固件目标应该重建。

合并后，只有已变更示例的固件目标会获得新版本。

## 场景 5：请求新分类

普通贡献中不接受新的平台组。

不要添加到 `PLATFORM_GROUPS`。请打开 issue 或 discussion，并说明：

- 为什么现有 `official`、`base` 和 `community` 组不适合，
- 新组会代表什么用户工作流，
- 哪些项目会放在其中。

## 网页元数据参考

网页由 `web/js/firmwares.js` 驱动。

### 平台卡片字段

| Field | Required | Meaning |
|---|---:|---|
| `id` | Yes | UI 使用的稳定平台 ID |
| `group` | Yes | 必须是 `official`、`base` 或 `community` |
| `name` | Yes | 可见的平台名称 |
| `tagline` | Yes | 简短的一行摘要 |
| `description` | Yes | 卡片中显示的更长说明 |
| `author` | Community only | Community Projects 卡片上显示的创建者名称 |
| `source` | Official and Community | 卡片上显示的官方网站或原始项目链接 |
| `wiki` | Official only | 显示在官方网站链接旁边的 Seeed Studio Wiki 教程链接 |
| `logo` | Yes | Logo 资源路径 |
| `preview` | Yes | 预览图片资源路径 |
| `previewAlt` | Yes | 无障碍图片描述 |
| `accent` | Yes | 现有主题颜色，通常是 `#004966` |
| `highlight` | Yes | 现有高亮颜色，通常是 `#8FC31F` |
| `supportedDevices` | Yes | 此平台可用的设备 ID |
| `installReady` | Yes | 只有网页烧录可用时才为 `true` |
| `templateMode` | Template only | 对于配置文件输出平台为 `true` |
| `templateHeader` | No | 始终添加到模板输出开头的文本 |
| `templateFooter` | No | 始终添加到模板输出末尾的文本 |
| `templateFileExtension` | No | 下载扩展名，默认是 `txt` |
| `templateFileMimeType` | No | 下载 MIME 类型，默认是 `text/plain` |
| `templateFilePattern` | No | 文件名模式，默认是 `{platformId}-{deviceId}` |
| `templateJoiner` | No | 模板各部分之间的分隔符，默认是 `\n\n` |
| `templateOptions` | Template only | 用于组装模板输出的功能选项 |
| `externalTool` | Official tool only | Step 2 中显示的官方固件或工具箱目标 |
| `bullets` | Yes | 三条简短工作流亮点 |
| `configFields` | Yes | 所有固件选项共享的平台级字段 |
| `firmwareOptions` | Yes | 可烧录的固件选项 |

### 固件选项字段

| Field | Required | Meaning |
|---|---:|---|
| `id` | Yes | 必须匹配固件构建 ID |
| `name` | Yes | 可见的固件名称 |
| `description` | Yes | 固件做什么 |
| `category` | Yes | 简短分类标签，例如 `Display`、`Power`、`Application` |
| `compatible` | Yes | 可以烧录此固件的设备 ID |
| `configFields` | No | 固件专用设置字段 |
| `notes` | No | 烧录前显示的警告或有帮助的说明 |

固件选项 `id` 是以下内容之间的连接点：

- `web/js/firmwares.js`，
- `.github/scripts/firmware_release.py`，
- `firmware/<id>/<version>/manifest.json`，
- 网页上的版本下拉选项。

## 配置字段和 NVS

当用户必须在烧录前输入值时，请使用 `configFields`，例如 Wi-Fi 凭据、API key、图片路径、显示设置或功能开关。

示例：

```js
configFields: [
  {
    id: "cfgWifiSsid",
    nvsKey: "wifiSsid",
    nvsType: "string",
    label: "Wi-Fi SSID",
    type: "text",
    defaultValue: "",
    placeholder: "Your Wi-Fi network name"
  },
  {
    id: "cfgApiKey",
    nvsKey: "apiKey",
    nvsType: "string",
    label: "API Key",
    type: "text",
    defaultValue: "",
    placeholder: "sk_..."
  }
]
```

支持的字段类型：

| UI `type` | NVS `nvsType` | Typical use |
|---|---|---|
| `text` | `string` | Wi-Fi SSID、API key、文件路径 |
| `password` | `string` | 密码 |
| `number` | `i32` | 整数设置 |
| `number` | `float` | 缩放、gamma、阈值 |
| `select` | `i32` | 枚举模式 |
| `checkbox` | `u8` | 开/关设置 |

固件必须从 NVS 命名空间 `config` 中读取相同的键。

当存在 `configFields` 时，网页烧录器会把生成的 NVS 分区写入 `0x9000`。除非同时更新网页烧录逻辑，否则请保持固件分区布局与默认 ESP32 NVS 位置兼容。

Arduino 或 PlatformIO 固件应使用这种模式：

```cpp
#include <Preferences.h>

static String wifiSsid;
static String apiKey;

void loadConfig()
{
  Preferences prefs;
  if (!prefs.begin("config", true)) {
    wifiSsid = "";
    apiKey = "";
    return;
  }

  wifiSsid = prefs.getString("wifiSsid", "");
  apiKey = prefs.getString("apiKey", "");
  prefs.end();
}
```

`web/js/firmwares.js` 中的 `nvsKey` 必须与固件使用的键字符串完全匹配。

## 构建目标注册表

标准 Arduino 示例在遵循这种形状时可以被自动发现：

```
examples/base/<Name>/<Name>.ino
examples/official/<Name>/<Name>.ino
examples/community/<Name>/<Name>.ino
```

自动化仍然可以读取旧历史中的扁平路径，但新增和更新的示例必须使用上面的分组布局。

在满足以下任意条件时，请使用 `.github/scripts/firmware_release.py` 并向 `FIRMWARE_TARGETS` 添加条目：

- 示例是 PlatformIO，
- 一个源码文件夹构建多个固件 ID，
- 固件只支持特定设备，
- 固件需要编译标志，
- 固件需要 Seeed_GFX、Seeed_GxEPD2、Sensirion SHT4x 或另一个特殊构建依赖，
- 固件需要生成的数据镜像，例如 SPIFFS，
- 网页 `firmwareOptions` ID 与文件夹名称不同。

Arduino 目标示例：

```python
FirmwareTarget(
    "LED_Control_E1003",
    "examples/base/LED_Control",
    devices=("E1003",),
    build_flags="-DDEVICE_MODEL=1003",
    title="LED Control for E1003",
),
```

PlatformIO 目标示例：

```python
FirmwareTarget(
    "ePaper_VoiceMemo_E1001",
    "examples/community/ePaper-Voice-Memo",
    tool="platformio",
    devices=("E1001",),
    pio_env="reterminal_e1001",
    title="Voice Memo Reminder for E1001",
),
```

不要把构建元数据放进 `.github/workflows/build-and-deploy.yml`。

只有当上游固件需要非日期版本号或非默认 flash 布局时，才使用 `fixed_version`、`boot_app0_offset` 或 `app_offset`。

## 固件打包和烧录分区

发布自动化期望每个固件 artifact（通俗解释：构建出来准备发布的文件包）包含：

| File | Flash offset |
|---|---:|
| `<firmware-id>.ino.bootloader.bin` | `0x0000` |
| `<firmware-id>.ino.partitions.bin` | `0x8000` |
| `boot_app0.bin` | `0xE000` |
| `<firmware-id>.ino.bin` | `0x10000` |
| `<firmware-id>.spiffs.bin` | target-specific data offset, when registered |

脚本会使用这些 offset 自动生成 `manifest.json`。非默认布局的目标可以在 `.github/scripts/firmware_release.py` 中覆盖 `boot_app0_offset` 和 `app_offset`。

Arduino 构建会通过 `arduino-cli` 产出预期文件。

PlatformIO 构建会由工作流重命名：

| PlatformIO output | Release artifact name |
|---|---|
| `firmware.bin` | `<firmware-id>.ino.bin` |
| `bootloader.bin` | `<firmware-id>.ino.bootloader.bin` |
| `partitions.bin` | `<firmware-id>.ino.partitions.bin` |
| `boot_app0.bin` | `boot_app0.bin` |

不要手动编辑生成的 manifests。

## 本地编译检查

打开 pull request 之前，尽可能在本地编译。

Arduino 设置：

```bash
arduino-cli config init
arduino-cli config add board_manager.additional_urls \
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
```

Arduino 编译：

```bash
arduino-cli compile \
  --fqbn "esp32:esp32:XIAO_ESP32S3:FlashSize=8M,PartitionScheme=default_8MB,PSRAM=opi" \
  ./examples/base/RTC_PCF8563
```

PlatformIO 编译：

```bash
cd examples/MyPlatformIOProject
pio run -e <env-name>
```

如果无法进行本地编译，请在 pull request 中说明原因。

## 不要编辑什么

不要手动编辑生成出来的固件输出：

- `firmware/versions.json`
- `firmware/catalog.json`
- `firmware/<id>/*/manifest.json`
- `firmware/<id>/*.bin`
- `boot_app0.bin`
- `gh-pages` 分支
- GitHub Releases

不要手动添加 release notes。release notes 由 GitHub Actions 生成。

不要提交：

- API keys，
- tokens，
- Wi-Fi 密码，
- 私钥，
- 真实用户凭据。

请改用占位符和 `configFields`。

## Pull request 检查清单

打开 pull request 之前，请使用这份检查清单。

### 源代码

- 新示例放在 `examples/base/`、`examples/official/` 或 `examples/community/` 下面。
- 不要添加新的扁平 `examples/<Project>/` 文件夹。
- 对于标准 Arduino 示例，文件夹名称和 `.ino` 文件名一致。
- PlatformIO 项目包含 `platformio.ini`。
- 没有提交密钥或真实凭据。
- 当设置步骤不是显而易见时，示例 README 会说明设置方法。

### 分类和网页

- 项目只使用 `official`、`base` 或 `community`。
- 没有添加新的 `PLATFORM_GROUPS` 条目。
- 官方平台卡片包含 `source`。
- 当官方平台有 Seeed Studio Wiki 教程时，平台卡片包含 `wiki`。
- Community Projects 包含 `author` 和 `source`。
- 当项目需要卡片、精致文案、按设备区分的兼容性、说明或配置字段时，已更新 `web/js/firmwares.js`。
- 每个 `firmwareOptions[].id` 都匹配一个固件构建 ID。
- 每个兼容设备都已正确列出。
- 模板模式平台设置 `installReady: false` 和 `templateMode: true`。
- 每个模板模式 `templateOptions` 条目都包含 `snippet` 字段。
- 模板数据，包括 header、footer 和 snippet，放在 `web/js/firmwares.js` 中，而不是 `web/js/app.js` 中。
- 官方工具平台设置 `installReady: false`，并包含 `externalTool`。
- 官方工具平台保持 `versions`、`configFields` 和 `firmwareOptions` 为空。

### 配置

- 对于用户提供的值，存在 `configFields`。
- `nvsKey` 值与固件 `Preferences` 键完全匹配。
- `nvsType` 与固件读取的值类型匹配。
- 不把真实 API key 或密码用作默认值。

### 构建和发布

- 标准 Arduino 示例遵循 `examples/<group>/<Name>/<Name>.ino`，或者已经为非标准构建更新 `FIRMWARE_TARGETS`。
- PlatformIO 项目有正确的 `pio_env`。
- 按设备区分的变体有单独的固件 ID。
- 已运行本地编译，或者 pull request 说明了为什么跳过。
- 没有编辑生成的固件文件、`gh-pages` 和 GitHub Releases。

## 合并后

pull request 合并到 `main` 后，GitHub Actions 将会：

1. 检测变更的示例文件夹，
2. 只构建变更的一个或多个固件目标，
3. 为这些固件目标写入新的基于日期的版本，
4. 更新用于版本下拉选项的 `firmware/versions.json`，
5. 更新用于自动发现固件的 `firmware/catalog.json`，
6. 将 Firmware Hub 部署到 `gh-pages`，
7. 创建一个 GitHub Release，其中包含所有示例的完整最新固件包。

不需要贡献者侧执行发布步骤。

## AI 代理说明

如果你是 AI 编码代理：

- 编辑前阅读此文件。
- 修改文件前先选择一个贡献场景。
- 将变更保持在最小相关范围内。
- 不要创建新的平台组。
- 不要编辑生成出来的固件输出。
- 不要添加真实密钥。
- 如果测试假示例，请使用临时目录或一次性分支，并且不要发布固件。
- 尽可能以 dry-run 形式运行发布规划脚本：

```bash
python3 .github/scripts/firmware_release.py plan \
  --changed-file examples/community/MyDemo/MyDemo.ino \
  --output-file /tmp/firmware-plan.json
```
