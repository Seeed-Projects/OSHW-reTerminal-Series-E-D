# Contributing Guide

This guide is the operating manual for adding or updating firmware examples in
the reTerminal E-Series Firmware Hub. It is written for both human contributors
and AI coding agents.

If you only remember one rule: add or edit the example source and the web
metadata that makes it visible. Do not manually publish firmware. GitHub Actions
builds, versions, deploys, and releases firmware automatically.

## Start here

Choose the path that matches your contribution.

| Goal | Allowed? | Main files to touch |
|---|---:|---|
| Add a new core hardware demo | Yes | `examples/base/<Demo>/`, optionally `web/js/firmwares.js` |
| Add a new official partner/platform demo | Yes | `examples/official/<Project>/`, `web/js/firmwares.js`, optionally `.github/scripts/firmware_release.py`. First choose: **flash mode**, **template mode**, **download mode**, or **official tool mode**. |
| Add a new community project | Yes | `examples/community/<Project>/`, `web/js/firmwares.js`, optionally `.github/scripts/firmware_release.py`. First choose: **flash mode**, **template mode**, or **download mode**. |
| Update an existing example | Yes | Only that example folder, plus matching web metadata if the UI changes |
| Add a new platform group/category | No | Do not edit `PLATFORM_GROUPS` |
| Manually create firmware versions or releases | No | Do not edit generated firmware output |

## Allowed Hub categories

The Firmware Hub currently has three groups.

| Group | `group` value | Use it for |
|---|---|---|
| Official Platforms | `official` | Official platform integrations or partner workflows, such as ESPHome, EEZ Studio, SquareLine Vision, and OpenDisplay |
| Base | `base` | First-party hardware bring-up demos: RTC, sleep, buttons, buzzer, display, sensors, SD card, microphone |
| Community Projects | `community` | Community apps or contributed end-user projects built on the device stack |

Do not create a new group. Contributions that need a new group should open a
discussion or issue first.

## Repository map

| Path | Purpose | Contributor action |
|---|---|---|
| `examples/` | Firmware source code | Add or update example code here |
| `web/js/firmwares.js` | Hub groups, platform cards, firmware dropdowns, config fields | Edit when the web page must show or describe a project |
| `.github/scripts/firmware_release.py` | Build registry and release helper | Edit only for PlatformIO, multi-device, special-library, or per-device builds |
| `.github/workflows/build-and-deploy.yml` | Shared GitHub Actions workflow | Do not edit for normal example contributions |
| `firmware/` on `gh-pages` | Generated firmware versions, manifests, and catalog | Do not edit manually |

## Example folder layout

New examples should use the grouped layout:

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

All examples in this repository should now live under one of these grouped
folders. Do not add new flat `examples/<Name>/` folders.

## Automation model

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

Unchanged examples are not recompiled. They keep their previously published
latest version and are still included in the full release package.

## Firmware version rules

Firmware versions are tracked per firmware ID.

Example:

```
firmware/ePaper_VoiceMemo_E1001/2026.06.07/
firmware/ePaper_VoiceMemo_E1001/2026.06.12/
firmware/RTC_PCF8563/2026.06.07/
```

Rules:

- The first build of a firmware on a date uses `YYYY.MM.DD`.
- If the same firmware is rebuilt again on the same date, the next version is
  `YYYY.MM.DD.1`, then `YYYY.MM.DD.2`, and so on.
- Official external platform firmware may set `fixed_version` in
  `.github/scripts/firmware_release.py` when the user-facing version must match
  the upstream firmware version.
- The web page reads `firmware/versions.json` and selects the newest version by
  default.
- New builds do not create a separate `latest` folder.
- Legacy `latest` firmware may be copied once into a date version so old
  published firmware remains available without rebuilding it.

## Scenario 1: add a standard Base Arduino demo

Use this path for simple first-party hardware demos that belong in Base.

### 1. Create the example folder

The folder name and `.ino` filename must match exactly.

```
examples/
  base/
    WiFi_Scanner/
      WiFi_Scanner.ino
      README.md
```

This standard shape can be auto-discovered by GitHub Actions. It builds as one
default Arduino firmware target named `WiFi_Scanner` for E1001, E1002, E1003,
and E1004.

### 2. Add local documentation

Add `examples/base/WiFi_Scanner/README.md` when setup steps are not obvious. Include:

- supported devices,
- required hardware,
- required external services,
- expected serial output,
- any local compile notes.

### 3. Decide whether web metadata is needed

If this is a very simple standard Arduino demo, GitHub Actions can generate
`firmware/catalog.json`, and the web app can append it to the Base firmware list
after the firmware has been published.

Auto-discovery is intended for simple Base Arduino demos only. Official platform
projects and Community Projects must be registered manually in
`web/js/firmwares.js`; otherwise they will not appear under the correct group.

Edit `web/js/firmwares.js` instead when you need any of these:

- a polished display name,
- a clear description,
- device-specific compatibility,
- warnings or notes,
- configuration fields,
- multiple firmware options for the same source folder.

### 4. Update the root README example list

When adding, renaming, or removing any Base, Official, or Community example,
update the root `README.md` `Arduino Examples` table with the matching row
change, including the example path, compatible devices, and a one-line
description.

## Scenario 2: add a demo to an official platform

Use this path for official partner integrations or supported platform workflows.
Do not create a new group. Add a platform card under the existing `official`
group, or add firmware options to an existing official platform card.

### 0. Choose a display mode

Before writing any code, decide how users will consume your contribution:

| Question | Answer: Flash mode | Answer: Template mode | Answer: Download mode | Answer: Official tool mode |
|---|---|---|---|---|
| Does your project produce a ready-to-run `.bin` firmware? | Yes | A generated config file | A source project template | Maintained by the upstream platform |
| How does the user finish setup? | Flash from the browser | Export a config file and use the target toolchain | Download, customize, compile, and flash locally | Open the upstream firmware or toolbox page |
| What does the user get? | A working firmware on the device | A starter config file | A packaged PlatformIO or source project | A direct handoff to the official platform tool |

**Flash mode** — set `installReady: true`, provide `firmwareOptions` with build
IDs, and register build targets in `firmware_release.py`. GitHub Actions builds
the binary; the Hub flashes it to the device via Web Serial.

**Template mode** — set `installReady: false` and `templateMode: true`, provide
`templateOptions` with `snippet` fields. No build targets needed. The Hub
generates a configuration file in the browser for the user to preview, copy, or
download. See [Template mode platforms](#template-mode-platforms) for the full
data structure.

**Download mode** — set `installReady: false` and `downloadMode: true`, provide
`downloadUrl` and `downloadSteps`. The Hub presents a download button and a
step-by-step local build guide. See
[Download mode platforms](#download-mode-platforms) for the field reference.

**Official tool mode** — set `installReady: false`, leave local firmware fields
empty, and provide `externalTool`. The Hub shows a Step 2 button that opens the
official firmware or toolbox page and skips the local flashing step. See
[Official tool platforms](#official-tool-platforms) for the field reference.

Most official platforms that require per-user customization (such as ESPHome,
where every user's display layout and sensor setup is different) should use
template mode.

Use download mode when the contribution is a complete source project template
that users customize in an external tool before compiling locally.

Use official tool mode when the upstream platform already maintains the firmware
installer, browser toolbox, or configuration flow.

### 1. Add the example source

Place source code under `examples/official/<Project>/`.

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

### 2. Register the web card or firmware option

Open `web/js/firmwares.js`.

For a new official platform card, add an object to `PLATFORM_CARDS`.

**Flash mode** (complete firmware, browser-flashable):

```js
{
  id: "my-official-platform",
  group: "official",
  name: "My Official Platform",
  tagline: "Short workflow summary.",
  source: { label: "Official website", url: "https://example.com" },
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

**Template mode** (example config, user customizes offline):

```js
{
  id: "my-official-platform",
  group: "official",
  name: "My Official Platform",
  tagline: "Short workflow summary.",
  source: { label: "Official website", url: "https://example.com" },
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

See [Template mode platforms](#template-mode-platforms) for the full field
reference and a working example.

**Download mode** (source project, user builds locally):

```js
{
  id: "my-official-platform",
  group: "official",
  name: "My Official Platform",
  tagline: "Short workflow summary.",
  source: { label: "Official website", url: "https://example.com" },
  description: "What this platform does and when to use it.",
  logo: "assets/platforms/my-official-platform-logo.svg",
  preview: "assets/platforms/my-official-platform-preview.svg",
  previewAlt: "My Official Platform preview",
  accent: "#004966",
  highlight: "#8FC31F",
  supportedDevices: ["E1001"],
  installReady: false,
  downloadMode: true,
  downloadUrl: "downloads/MyOfficialPlatform.zip",
  downloadSteps: [
    {
      title: "Download project template",
      description: "Download the packaged PlatformIO project."
    }
  ],
  bullets: ["Source project template", "Local build workflow"],
  versions: [],
  configFields: [],
  firmwareOptions: []
}
```

**Official tool mode** (upstream firmware or toolbox, user continues there):

```js
{
  id: "my-official-platform",
  group: "official",
  name: "My Official Platform",
  tagline: "Short workflow summary.",
  source: { label: "Official website", url: "https://example.com" },
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

The `source` field is required for official platform cards. Use it to link to
the official product or project website.

For a flash-mode firmware option, add it to the platform card's `firmwareOptions` array:

```js
{
  id: "MyOfficialDemo_E1001",
  name: "My Official Demo",
  description: "What the firmware does.",
  category: "Application",
  compatible: ["E1001"],
  configFields: [],
  flashNotes: [
    { type: "warning", text: "Use the erase flash mode for a full-chip image." }
  ],
  notes: [
    { type: "info", text: "What users should know before setup." }
  ]
}
```

The `id` must match the firmware ID built by GitHub Actions.

For full-chip images that start at offset `0x0`, set
`recommendedInstallMode: "erase"` on the firmware option so the Hub opens with
`Erase flash + flash` selected, and add the matching user guidance to
`flashNotes`.

对于从 `0x0` 起写入的整片镜像，在固件选项中设置
`recommendedInstallMode: "erase"`，让 Hub 默认选中 `Erase flash + flash`，
并把对应的用户提示放进 `flashNotes`。

### 3. Register non-standard build targets

If the project is PlatformIO, needs per-device builds, needs compile flags, or
needs special libraries, add entries in `.github/scripts/firmware_release.py`
under `FIRMWARE_TARGETS`.

Do not edit `.github/workflows/build-and-deploy.yml` for this.

For download-mode source templates, package the source folder during deployment
so the Hub can serve the `downloadUrl` file from `gh-pages`.

## Scenario 3: add a community project

Use this path for contributed applications or demos that are not official
platform integrations.

### 0. Choose a display mode

Before writing any code, decide how users will consume your contribution:

| Question | Answer: Flash mode | Answer: Template mode |
|---|---|---|
| Does your project produce a ready-to-run `.bin` firmware? | Yes | No |
| Can the user flash it directly from the browser without modification? | Yes | No — each user needs to customize the config |
| What does the user get? | A working firmware on the device | A starter config file to edit and compile elsewhere |

**Flash mode** — set `installReady: true`, provide `firmwareOptions`, and
register build targets. The Hub builds and flashes the binary.

**Template mode** — set `installReady: false` and `templateMode: true`, provide
`templateOptions` with `snippet` fields. The Hub generates a configuration file
for the user to preview, copy, or download. See
[Template mode platforms](#template-mode-platforms) for the full reference.

### 1. Add source code

Place source code under `examples/community/<Project>/`.

For a standard Arduino community project, use this exact shape:

```
examples/community/Example/
  Example.ino
  README.md
```

The default firmware build ID is `Example`. Use that exact value in
`firmwareOptions[].id`.

For PlatformIO projects, per-device builds, special libraries, compile flags, or
firmware IDs that differ from the folder name, add explicit `FirmwareTarget`
entries in `.github/scripts/firmware_release.py`.

### 2. Add or update a Community Projects card

Open `web/js/firmwares.js`.

For a full project, add a platform card. Choose the template that matches your
display mode from Step 0.

**Flash mode** (complete firmware):

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

Then add one or more `firmwareOptions` with IDs that match the build targets.
For full-chip images that start at offset `0x0`, set
`recommendedInstallMode: "erase"` on the matching firmware option and place
the erase-mode guidance in `flashNotes`.

继续添加一个或多个 `firmwareOptions`，ID 需要匹配构建目标。对于从 `0x0`
起写入的整片镜像，在对应固件选项中设置 `recommendedInstallMode: "erase"`，
并把擦除模式提示放进 `flashNotes`。

**Template mode** (example config):

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

See [Template mode platforms](#template-mode-platforms) for the full field
reference.

The `author` and `source` fields are required for Community Projects. They are
shown on the card to credit the original creator and link back to the upstream
project.

### 3. Keep secrets out of code

If the project needs Wi-Fi, API keys, or tokens, use `configFields` and NVS.
Do not commit real secrets.

### 4. Dry-run the build plan

Before opening a pull request, dry-run the release planner:

```bash
python3 .github/scripts/firmware_release.py plan \
  --changed-file examples/community/Example/Example.ino \
  --output-file /tmp/example-plan.json
```

For PlatformIO, this dry-run only reports firmware changes after the project has
a matching `FirmwareTarget` entry.

## Template mode platforms

Use template mode when a platform generates a configuration file, such as YAML
or JSON, instead of flashing firmware directly. Use flash mode when the tool
installs a compiled firmware binary through Web Serial.

Template mode keeps all template data in `web/js/firmwares.js`. The browser
assembles the final file from platform-level text plus the snippets selected by
the user.

### Template mode platform fields

| Field | Required | Default | Description |
|---|---:|---|---|
| `templateMode` | Yes | — | Must be `true` |
| `templateHeader` | No | `""` | Text always prepended to output |
| `templateFooter` | No | `""` | Text always appended to output |
| `templateFileExtension` | No | `"txt"` | File extension for download |
| `templateFileMimeType` | No | `"text/plain"` | MIME type for download |
| `templateFilePattern` | No | `"{platformId}-{deviceId}"` | Filename pattern |
| `templateJoiner` | No | `"\n\n"` | Separator between parts |
| `templateOptions` | Yes | — | Array of feature options |

### Template option fields

| Field | Required | Description |
|---|---:|---|
| `id` | Yes | Unique identifier |
| `label` | Yes | Checkbox label |
| `description` | Yes | Helper text below label |
| `defaultChecked` | No | `true` to check by default |
| `snippet` | Yes | Text content included when checked |

Filename patterns support these tokens:

- `{platformId}` becomes the platform `id` field.
- `{deviceId}` becomes the selected device ID, lowercased.

Minimal template-mode platform example:

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

Template mode platforms do not need firmware build targets in
`.github/scripts/firmware_release.py`, `versions.json` entries, or
`manifest.json` files. They are purely client-side.

## Download mode platforms

Use download mode when the Hub should provide a source project archive and a
local build guide instead of generating a browser-side config file or flashing a
compiled binary.

### Download mode platform fields

| Field | Required | Default | Description |
|---|---:|---|---|
| `downloadMode` | Yes | — | Must be `true` |
| `downloadUrl` | Yes | — | Relative URL for the packaged project archive |
| `downloadSteps` | Yes | — | Ordered setup guide shown in the flash panel |

### Download step fields

| Field | Required | Description |
|---|---:|---|
| `title` | Yes | Short step title |
| `description` | Yes | One paragraph describing the action |

Download-mode platforms use `installReady: false`, empty `versions`, empty
`configFields`, and empty `firmwareOptions`. The deployment workflow packages
the source project into the file referenced by `downloadUrl`.

## Official tool platforms

Use official tool mode when an upstream platform maintains the firmware
installer, browser toolbox, or configuration flow. The Hub shows the selected
platform and device, then uses Step 2 to link users to that official workflow.

### Official tool platform fields

| Field | Required | Default | Description |
|---|---:|---|---|
| `externalTool` | Yes | — | Object describing the official destination |
| `externalTool.label` | Yes | — | Button text |
| `externalTool.url` | Yes | — | Absolute URL opened by the button |
| `externalTool.title` | Yes | — | Heading inside the Step 2 card |
| `externalTool.description` | Yes | — | One paragraph explaining the handoff |

Official tool platforms use `installReady: false`, empty `versions`, empty
`configFields`, and empty `firmwareOptions`. The web page hides the local flash
panel for these platforms.

## Scenario 4: update an existing example

Use this path when changing code that already exists.

1. Edit only the matching example folder:

   ```
   examples/<group>/<ExistingExample>/
   ```

2. If the change affects user-facing setup, also update that example's README.

3. If the change affects the web page text, compatible devices, notes, or
   configuration fields, update `web/js/firmwares.js`.

4. Do not touch unrelated example folders. GitHub Actions uses the changed
   folder to decide which firmware targets should be rebuilt.

After merge, only the changed example's firmware targets get new versions.

## Scenario 5: request a new category

New platform groups are not accepted in normal contributions.

Do not add to `PLATFORM_GROUPS`. Open an issue or discussion explaining:

- why the existing `official`, `base`, and `community` groups do not fit,
- what user workflow the new group would represent,
- which projects would live there.

## Web metadata reference

The web page is driven by `web/js/firmwares.js`.

### Platform card fields

| Field | Required | Meaning |
|---|---:|---|
| `id` | Yes | Stable platform ID used by the UI |
| `group` | Yes | Must be `official`, `base`, or `community` |
| `name` | Yes | Visible platform name |
| `tagline` | Yes | Short one-line summary |
| `description` | Yes | Longer explanation shown in the card |
| `author` | Community only | Creator name shown on Community Projects cards |
| `source` | Official and Community | Official website or original project link shown on cards |
| `logo` | Yes | Logo asset path |
| `preview` | Yes | Preview image asset path |
| `previewAlt` | Yes | Accessible image description |
| `accent` | Yes | Existing theme color, usually `#004966` |
| `highlight` | Yes | Existing highlight color, usually `#8FC31F` |
| `supportedDevices` | Yes | Device IDs available in this platform |
| `installReady` | Yes | `true` only when web flashing is available |
| `templateMode` | Template only | `true` for configuration file output platforms |
| `templateHeader` | No | Text always prepended to template output |
| `templateFooter` | No | Text always appended to template output |
| `templateFileExtension` | No | Download extension, defaults to `txt` |
| `templateFileMimeType` | No | Download MIME type, defaults to `text/plain` |
| `templateFilePattern` | No | Filename pattern, defaults to `{platformId}-{deviceId}` |
| `templateJoiner` | No | Separator between template parts, defaults to `\n\n` |
| `templateOptions` | Template only | Feature options used to assemble template output |
| `externalTool` | Official tool only | Official firmware or toolbox destination shown in Step 2 |
| `bullets` | Yes | Three short workflow highlights |
| `configFields` | Yes | Platform-level fields shared by all firmware options |
| `firmwareOptions` | Yes | Flashable firmware choices |

### Firmware option fields

| Field | Required | Meaning |
|---|---:|---|
| `id` | Yes | Must match the firmware build ID |
| `name` | Yes | Visible firmware name |
| `description` | Yes | What the firmware does |
| `category` | Yes | Short category label such as `Display`, `Power`, `Application` |
| `compatible` | Yes | Device IDs that can flash this firmware |
| `recommendedInstallMode` | No | Use `"erase"` for full-chip images; omitted firmware uses standard flash / 整片镜像使用 `"erase"`，未填写时使用标准烧录 |
| `configFields` | No | Firmware-specific setup fields |
| `notes` | No | Step 2 setup and preparation notes / Step 2 的配置与准备提示 |
| `flashNotes` | No | Step 3 flashing notes, such as erase-mode or connection guidance / Step 3 的烧录提示，例如擦除模式或连接提示 |

The firmware option `id` is the link between:

- `web/js/firmwares.js`,
- `.github/scripts/firmware_release.py`,
- `firmware/<id>/<version>/manifest.json`,
- the version dropdown on the web page.

## Configuration fields and NVS

Use `configFields` when the user must enter values before flashing, such as
Wi-Fi credentials, API keys, image paths, display settings, or feature toggles.

Example:

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

Supported field types:

| UI `type` | NVS `nvsType` | Typical use |
|---|---|---|
| `text` | `string` | Wi-Fi SSID, API key, file path |
| `password` | `string` | Passwords |
| `number` | `i32` | Integer settings |
| `number` | `float` | Scale, gamma, thresholds |
| `select` | `i32` | Enumerated modes |
| `checkbox` | `u8` | On/off settings |

The firmware must read the same keys from NVS namespace `config`.

When `configFields` are present, the web flasher writes the generated NVS
partition at `0x9000`. Keep the firmware partition layout compatible with the
default ESP32 NVS location unless the web flashing logic is updated at the same
time.

Arduino or PlatformIO firmware should use this pattern:

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

The `nvsKey` in `web/js/firmwares.js` must exactly match the key string used by
the firmware.

## Build target registry

Standard Arduino examples can be auto-discovered when they follow this shape:

```
examples/base/<Name>/<Name>.ino
examples/official/<Name>/<Name>.ino
examples/community/<Name>/<Name>.ino
```

The automation can still read legacy flat paths for older history, but new and
updated examples must use the grouped layout above.

Use `.github/scripts/firmware_release.py` and add entries to `FIRMWARE_TARGETS`
when any of these are true:

- the example is PlatformIO,
- one source folder builds multiple firmware IDs,
- the firmware supports only specific devices,
- the firmware needs compile flags,
- the firmware needs Seeed_GFX, Seeed_GxEPD2, Sensirion SHT4x, or another
  special build dependency,
- the firmware needs a generated data image such as SPIFFS,
- the web `firmwareOptions` ID is not the same as the folder name.

Arduino target example:

```python
FirmwareTarget(
    "LED_Control_E1003",
    "examples/base/LED_Control",
    devices=("E1003",),
    build_flags="-DDEVICE_MODEL=1003",
    title="LED Control for E1003",
),
```

PlatformIO target example:

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

Do not put build metadata in `.github/workflows/build-and-deploy.yml`.

Use `fixed_version`, `boot_app0_offset`, or `app_offset` only for targets whose
upstream firmware requires a non-date version or a non-default flash layout.

## Firmware packaging and flash partitions

The release automation expects every firmware artifact to contain:

| File | Flash offset |
|---|---:|
| `<firmware-id>.ino.bootloader.bin` | `0x0000` |
| `<firmware-id>.ino.partitions.bin` | `0x8000` |
| `boot_app0.bin` | `0xE000` |
| `<firmware-id>.ino.bin` | `0x10000` |
| `<firmware-id>.spiffs.bin` | target-specific data offset, when registered |

The script generates `manifest.json` automatically with these offsets. Targets
with non-default layouts can override `boot_app0_offset`, `app_offset`, and
`spiffs_offset` in `.github/scripts/firmware_release.py`.

Arduino builds produce the expected files through `arduino-cli`.

PlatformIO builds are renamed by the workflow:

| PlatformIO output | Release artifact name |
|---|---|
| `firmware.bin` | `<firmware-id>.ino.bin` |
| `bootloader.bin` | `<firmware-id>.ino.bootloader.bin` |
| `partitions.bin` | `<firmware-id>.ino.partitions.bin` |
| `littlefs.bin` or `spiffs.bin` | `<firmware-id>.spiffs.bin` |
| `boot_app0.bin` | `boot_app0.bin` |

Do not edit generated manifests by hand.

## Local compile checks

Before opening a pull request, compile locally when possible.

Arduino setup:

```bash
arduino-cli config init
arduino-cli config add board_manager.additional_urls \
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
```

Arduino compile:

```bash
arduino-cli compile \
  --fqbn "esp32:esp32:XIAO_ESP32S3:FlashSize=8M,PartitionScheme=default_8MB,PSRAM=opi" \
  ./examples/base/RTC_PCF8563
```

PlatformIO compile:

```bash
cd examples/MyPlatformIOProject
pio run -e <env-name>
```

If local compile is not possible, explain why in the pull request.

## What not to edit

Do not manually edit generated firmware output:

- `firmware/versions.json`
- `firmware/catalog.json`
- `firmware/<id>/*/manifest.json`
- `firmware/<id>/*.bin`
- `boot_app0.bin`
- the `gh-pages` branch
- GitHub Releases

Do not manually add release notes. The release notes are generated by GitHub
Actions.

Do not commit:

- API keys,
- tokens,
- Wi-Fi passwords,
- private keys,
- real user credentials.

Use placeholders and `configFields` instead.

## Pull request checklist

Use this checklist before opening a pull request.

### Source code

- New examples live under `examples/base/`, `examples/official/`, or
  `examples/community/`.
- Do not add new flat `examples/<Project>/` folders.
- The folder name and `.ino` filename match for standard Arduino examples.
- PlatformIO projects include `platformio.ini`.
- Root `README.md` `Arduino Examples` table matches added, renamed, or removed examples.
- No secrets or real credentials are committed.
- The example README explains setup when setup is not obvious.

### Category and web page

- The project uses only `official`, `base`, or `community`.
- No new `PLATFORM_GROUPS` entry was added.
- Official platform cards include `source`.
- Community Projects include `author` and `source`.
- `web/js/firmwares.js` was updated when the project needs a card, polished
  copy, device-specific compatibility, notes, or config fields.
- Every `firmwareOptions[].id` matches a firmware build ID.
- Every compatible device is listed correctly.
- Full-chip images set `recommendedInstallMode: "erase"` and put erase-mode
  guidance in `flashNotes`.
- 整片镜像设置 `recommendedInstallMode: "erase"`，并将擦除模式提示放在
  `flashNotes`。
- Template-mode platforms set `installReady: false` and `templateMode: true`.
- Every template-mode `templateOptions` entry includes a `snippet` field.
- Template data, including headers, footers, and snippets, lives in
  `web/js/firmwares.js`, not in `web/js/app.js`.
- Download-mode platforms set `installReady: false` and `downloadMode: true`.
- Download-mode platforms include `downloadUrl` and `downloadSteps`.
- Official tool platforms set `installReady: false` and include `externalTool`.
- Official tool platforms keep `versions`, `configFields`, and `firmwareOptions`
  empty.

### Configuration

- `configFields` exist for user-provided values.
- `nvsKey` values match firmware `Preferences` keys exactly.
- `nvsType` matches the value type read by firmware.
- Real API keys or passwords are not used as defaults.

### Build and release

- Standard Arduino examples follow `examples/<group>/<Name>/<Name>.ino`, or
  `FIRMWARE_TARGETS` was updated for non-standard builds.
- PlatformIO projects have the correct `pio_env`.
- Per-device variants have separate firmware IDs.
- Local compile was run, or the pull request explains why it was skipped.
- Generated firmware files, `gh-pages`, and GitHub Releases were not edited.

## After merge

After the pull request is merged into `main`, GitHub Actions will:

1. detect the changed example folder,
2. build only the changed firmware target or targets,
3. write a new date-based version for those firmware targets,
4. update `firmware/versions.json` for the version dropdown,
5. update `firmware/catalog.json` for auto-discovered firmware,
6. package download-mode project templates,
7. deploy the Firmware Hub to `gh-pages`,
8. create a GitHub Release that includes a full latest firmware package for all
   examples.

No contributor-side release step is required.

## AI agent instructions

If you are an AI coding agent:

- Read this file before editing.
- Choose one contribution scenario before changing files.
- Keep changes inside the smallest relevant scope.
- Do not create a new platform group.
- Do not edit generated firmware output.
- Do not add real secrets.
- If testing a fake example, use a temporary directory or a disposable branch and
  do not publish firmware.
- Run the release planning script in dry-run form when possible:

```bash
python3 .github/scripts/firmware_release.py plan \
  --changed-file examples/community/MyDemo/MyDemo.ino \
  --output-file /tmp/firmware-plan.json
```
