let expandedPlatformId = null;
let selectedPlatform = null;
let selectedDevice = null;
let selectedPanel = null;
let selectedFirmwareOption = null;
let selectedVersion = null;
let monitorPort = null;
let monitorReader = null;
let monitorKeepReading = false;
let monitorBaudRate = 115200;
let logPaused = false;
const LOG_RETAINED_MAX_LENGTH = 256 * 1024;
const LOG_VISIBLE_MAX_LENGTH = 48 * 1024;
const LOG_RENDER_INTERVAL_MS = 250;
const SERIAL_PENDING_MAX_LENGTH = 32 * 1024;
const SERIAL_READ_YIELD_BYTES = 8192;
const SERIAL_READ_YIELD_CHUNKS = 32;
const SERIAL_PENDING_TRIM_NOTICE = "[monitor] Incoming log output was throttled.\n";
let logBuffer = globalThis.createLogBuffer({ maxLength: LOG_RETAINED_MAX_LENGTH });
let logRenderTimer = null;
let pendingSerialText = "";
let pendingSerialTrimmed = false;
let firmwareVersions = {};
let firmwareCatalogLoaded = false;
const monitorDecoder = new TextDecoder();
const DEFAULT_FIRMWARE_VERSION = "latest";
const FIRMWARE_CACHE_BUSTER = String(Date.now());

document.addEventListener("DOMContentLoaded", () => {
  checkBrowser();
  void loadFirmwareVersions();
  renderPlatformCards();
  renderFlowState();
  bindFlowEvents();
  bindWorkspaceEvents();
  bindBlankAreaCollapse();
});

function checkBrowser() {
  if (!("serial" in navigator)) {
    const el = document.getElementById("browserWarning");
    if (el) el.classList.add("is-visible");
  }
}

function getDevice(deviceId) {
  if (typeof getHardware === "function") return getHardware(deviceId);
  return DEVICES.find((device) => device.id === deviceId) || null;
}

function hardwareNeedsPanel(hardware = selectedDevice) {
  return typeof isDriverBoard === "function" && isDriverBoard(hardware);
}

function getFlashMethod(hardware = selectedDevice) {
  return hardware?.flashMethod || "serial";
}

function getSelectedCompatiblePanels() {
  if (!hardwareNeedsPanel() || typeof getCompatiblePanels !== "function") return [];
  return getCompatiblePanels(selectedDevice.id, selectedFirmwareOption);
}

function choosePanel(panels, preferredId = "") {
  if (!panels.length) return null;
  return panels.find((panel) => panel.id === preferredId) || (panels.length === 1 ? panels[0] : null);
}

// Returns the firmware id to fetch: combo builds resolve to a board+panel id.
// 返回实际要获取的固件 id：组合构建解析为「板 + 屏」专属 id。
function getResolvedFirmwareId() {
  if (typeof resolveComboFirmwareId === "function" && selectedFirmwareOption?.comboPattern) {
    return resolveComboFirmwareId(
      selectedFirmwareOption,
      selectedDevice?.id,
      selectedPanel?.id
    );
  }
  return selectedFirmwareOption?.id || "";
}

function getUf2DownloadUrl() {
  if (!selectedPlatform?.installReady || !selectedFirmwareOption || !selectedDevice || !selectedPanel) {
    return "";
  }
  if (getFlashMethod() !== "uf2") return "";
  const firmwareId = getResolvedFirmwareId();
  if (!firmwareId) return "";
  const version = selectedVersion?.version
    || (selectedPlatform.requiresVersionManifest ? "" : DEFAULT_FIRMWARE_VERSION);
  if (!version) return "";
  return withFirmwareCacheBuster(
    `firmware/${firmwareId}/${version}/${firmwareId}.uf2`
  );
}

function getDefaultVersion(platform) {
  const versions = getVersionOptions(platform);
  return versions[0] || null;
}

function getAvailableFirmwareOptions(platform, deviceId) {
  if (!platform?.firmwareOptions?.length || !deviceId) return [];
  return platform.firmwareOptions.filter((firmware) =>
    firmware.compatible.includes(deviceId)
  );
}

function getFirmwareVariantGroup(firmware) {
  return firmware?.variantGroup || firmware?.id || "";
}

function getFirmwareLanguage(firmware) {
  return firmware?.language || "";
}

function getFirmwareBaseOptions(platform, deviceId) {
  const options = getAvailableFirmwareOptions(platform, deviceId);
  const seen = new Set();
  const baseOptions = [];
  options.forEach((firmware) => {
    const group = getFirmwareVariantGroup(firmware);
    if (seen.has(group)) return;
    seen.add(group);
    baseOptions.push(firmware);
  });
  return baseOptions;
}

function getLanguageVariantsForFirmware(platform, deviceId, firmware) {
  const group = getFirmwareVariantGroup(firmware);
  return getAvailableFirmwareOptions(platform, deviceId)
    .filter((item) => getFirmwareVariantGroup(item) === group);
}

function chooseFirmwareOption(options, preferredGroup = "", preferredLanguage = "") {
  if (!options.length) return null;
  const groupOptions = preferredGroup
    ? options.filter((item) => getFirmwareVariantGroup(item) === preferredGroup)
    : options;
  const candidates = groupOptions.length ? groupOptions : options;
  return candidates.find((item) => getFirmwareLanguage(item) === preferredLanguage)
      || candidates.find((item) => getFirmwareLanguage(item) === "en")
      || candidates[0];
}

function getInstallManifest() {
  if (!selectedPlatform?.installReady || !selectedFirmwareOption) return "";
  if (getFlashMethod() === "uf2") return "";
  if (hardwareNeedsPanel() && !selectedPanel) return "";
  const firmwareId = getResolvedFirmwareId();
  if (!firmwareId) return "";
  const version = selectedVersion?.version
    || (selectedPlatform.requiresVersionManifest ? "" : DEFAULT_FIRMWARE_VERSION);
  if (!version) return "";
  return withFirmwareCacheBuster(`firmware/${firmwareId}/${version}/manifest.json`);
}

function withFirmwareCacheBuster(path) {
  const separator = path.includes("?") ? "&" : "?";
  return `${path}${separator}cb=${encodeURIComponent(FIRMWARE_CACHE_BUSTER)}`;
}

function normalizeFirmwareVersions(data) {
  if (!data || typeof data !== "object") return {};

  return Object.fromEntries(
    Object.entries(data).map(([firmwareId, versions]) => [
      firmwareId,
      Array.isArray(versions) && versions.length
        ? versions.map((version) => String(version))
        : [DEFAULT_FIRMWARE_VERSION],
    ])
  );
}

function getRegisteredFirmwareIds() {
  return new Set(
    PLATFORM_CARDS.flatMap((platform) =>
      (platform.firmwareOptions || []).map((firmware) => firmware.id)
    )
  );
}

function getBasePlatform() {
  return PLATFORM_CARDS.find((platform) => platform.id === "base") || null;
}

function mergeAutoDiscoveredFirmware(catalog) {
  if (!Array.isArray(catalog?.firmware)) return;
  const basePlatform = getBasePlatform();
  if (!basePlatform) return;

  const registeredIds = getRegisteredFirmwareIds();
  const additions = catalog.firmware
    .filter((firmware) => firmware?.autoDiscovered)
    .filter((firmware) => !firmware.group || firmware.group === "base")
    .filter((firmware) => firmware?.id && !registeredIds.has(firmware.id))
    .filter((firmware) => Array.isArray(firmwareVersions[firmware.id]));

  additions.forEach((firmware) => {
    basePlatform.firmwareOptions.push({
      id: firmware.id,
      name: firmware.name || firmware.id,
      description: `Auto-discovered firmware from ${firmware.path || "examples"}.`,
      category: "Generated",
      compatible: Array.isArray(firmware.compatible) && firmware.compatible.length
        ? firmware.compatible
        : basePlatform.supportedDevices,
    });
  });
}

async function loadFirmwareCatalog() {
  if (firmwareCatalogLoaded) return;
  firmwareCatalogLoaded = true;

  try {
    const response = await fetch(withFirmwareCacheBuster("firmware/catalog.json"));
    if (!response.ok) throw new Error(`Firmware catalog fetch failed: ${response.status}`);
    mergeAutoDiscoveredFirmware(await response.json());
  } catch (_) {
    // The catalog is generated on GitHub Pages; local previews can run without it.
    // catalog 由 GitHub Pages 生成；本地预览没有它也可以运行。
  }
}

async function loadFirmwareVersions() {
  try {
    const response = await fetch(withFirmwareCacheBuster("firmware/versions.json"));
    if (!response.ok) throw new Error(`Version manifest fetch failed: ${response.status}`);
    firmwareVersions = normalizeFirmwareVersions(await response.json());
  } catch (_) {
    firmwareVersions = {};
  }

  await loadFirmwareCatalog();
  renderPlatformCards();

  if (selectedPlatform) {
    selectedVersion = getDefaultVersion(selectedPlatform);
    renderLanguageSelect();
    renderVersionPanel();
    updateFlashState();
  }
}

function getFirmwareVersionValues(firmwareOption, platform = selectedPlatform) {
  if (!firmwareOption?.id) return [DEFAULT_FIRMWARE_VERSION];
  // Combo builds publish versions under the resolved board+panel id.
  // 组合构建的版本发布在解析后的「板 + 屏」id 下。
  const lookupId = firmwareOption.comboPattern && firmwareOption === selectedFirmwareOption
    ? getResolvedFirmwareId() || firmwareOption.id
    : firmwareOption.id;
  const versions = firmwareVersions[lookupId];
  if (Array.isArray(versions) && versions.length) return versions;
  // Platforms whose versions mirror an upstream release have no meaningful
  // hardcoded fallback — without the deployed manifest, flashing is disabled.
  // 版本跟随上游发布的平台没有可用的硬编码回退——缺少清单时禁用烧录。
  if (platform?.requiresVersionManifest) return [];
  return [firmwareOption.defaultVersion || DEFAULT_FIRMWARE_VERSION];
}

function getVersionOptions(platform = selectedPlatform) {
  if (platform?.installReady && selectedFirmwareOption) {
    return getFirmwareVersionValues(selectedFirmwareOption, platform).map((version) => ({
      version,
      label: version,
    }));
  }

  return platform?.versions || [
    {
      version: DEFAULT_FIRMWARE_VERSION,
      label: DEFAULT_FIRMWARE_VERSION,
    },
  ];
}

function renderDeviceSpecs(device, className = "") {
  if (!device?.specs?.length) return "";
  const classAttr = className ? ` class="${className}"` : "";
  return device.specs.map((spec) => `<span${classAttr}>${spec}</span>`).join("");
}

function renderPlatformDetailTags(platform, className = "") {
  if (!platform?.detailTags?.length) return "";
  const classAttr = className ? ` class="${className}"` : "";
  return platform.detailTags.map((tag) => `<span${classAttr}>${tag}</span>`).join("");
}

function renderPlatformCreditMeta(platform, className = "", showWiki = true) {
  const canShowAuthor = platform.group === "community" && platform.author;
  const canShowSource =
    (platform.group === "official" || platform.group === "community") && platform.source?.url;
  const canShowWiki = showWiki && platform.group === "official" && platform.wiki?.url;
  if (!canShowAuthor && !canShowSource && !canShowWiki) {
    return "";
  }

  const metaClassName = ["community-meta", className].filter(Boolean).join(" ");
  const author = canShowAuthor
    ? `<span class="community-author">By <strong>${platform.author}</strong></span>`
    : "";
  const sourceIcon = platform.source?.url?.includes("github.com")
    ? `<svg viewBox="0 0 16 16" aria-hidden="true">
        <path d="M8 1.3a6.7 6.7 0 0 0-2.1 13c.34.06.46-.15.46-.32v-1.24c-1.88.41-2.28-.8-2.28-.8-.3-.78-.75-.99-.75-.99-.62-.42.05-.41.05-.41.68.05 1.04.7 1.04.7.6 1.03 1.58.73 1.97.56.06-.44.24-.73.43-.9-1.5-.17-3.08-.75-3.08-3.34 0-.74.26-1.34.7-1.82-.07-.17-.3-.86.07-1.8 0 0 .57-.19 1.86.7A6.5 6.5 0 0 1 8 4.4c.57 0 1.14.08 1.68.23 1.29-.89 1.86-.7 1.86-.7.37.94.14 1.63.07 1.8.44.48.7 1.08.7 1.82 0 2.6-1.58 3.16-3.09 3.33.25.22.46.64.46 1.3v1.8c0 .18.12.39.47.32A6.7 6.7 0 0 0 8 1.3Z"/>
      </svg>`
    : `<svg viewBox="0 0 16 16" aria-hidden="true">
        <path d="M4 3.25A1.75 1.75 0 0 0 2.25 5v7A1.75 1.75 0 0 0 4 13.75h7A1.75 1.75 0 0 0 12.75 12V9.5a.75.75 0 0 0-1.5 0V12a.25.25 0 0 1-.25.25H4a.25.25 0 0 1-.25-.25V5A.25.25 0 0 1 4 4.75h2.5a.75.75 0 0 0 0-1.5H4Zm5.5 0a.75.75 0 0 0 0 1.5h1.19L7.22 8.22a.75.75 0 1 0 1.06 1.06l3.47-3.47V7a.75.75 0 0 0 1.5 0V4a.75.75 0 0 0-.75-.75h-3Z"/>
      </svg>`;
  const source = canShowSource
    ? `<a class="community-source" href="${platform.source.url}" target="_blank" rel="noopener">
        ${sourceIcon}
        <span>${platform.source.label || "Source"}</span>
      </a>`
    : "";
  const wiki = canShowWiki
    ? `<a class="community-source platform-wiki-link" href="${platform.wiki.url}" target="_blank" rel="noopener">
        <svg viewBox="0 0 16 16" aria-hidden="true">
          <path d="M3.5 2.25h3.35c.68 0 1.28.26 1.65.7.37-.44.97-.7 1.65-.7h2.35c.69 0 1.25.56 1.25 1.25v9.25c0 .69-.56 1.25-1.25 1.25h-2.35c-.47 0-.87.15-1.15.43a.75.75 0 0 1-1 0 1.62 1.62 0 0 0-1.15-.43H3.5c-.69 0-1.25-.56-1.25-1.25V3.5c0-.69.56-1.25 1.25-1.25Zm4.25 10.03V4.1c-.18-.22-.5-.35-.9-.35H3.75v8.75h3.1c.32 0 .62.05.9.16Zm1.5.38c.28-.11.58-.16.9-.16h2.1V3.75h-2.1c-.4 0-.72.13-.9.35Z"/>
        </svg>
        <span>${platform.wiki.label || "Wiki"}</span>
      </a>`
    : "";

  return `<div class="${metaClassName}">${author}${source}${wiki}</div>`;
}

function renderHardwareOption(platform, hardware) {
  const connectorBadge = hardware.connector
    ? `<span class="device-connector">${hardware.connector}</span>`
    : "";
  return `
    <button class="device-option" data-platform="${platform.id}" data-device="${hardware.id}" type="button">
      <span class="device-image">
        <img src="${hardware.image}" alt="${hardware.imageAlt}">
        ${connectorBadge}
      </span>
      <span class="device-copy">
        <strong>${hardware.name}</strong>
        <span>${hardware.description}</span>
        <span class="device-specs">${renderDeviceSpecs(hardware)}</span>
      </span>
    </button>
  `;
}

function renderHardwareGroups(platform) {
  const groups = typeof groupSupportedHardware === "function"
    ? groupSupportedHardware(platform.supportedDevices)
    : [{
        id: "all",
        title: "Supported hardware",
        items: platform.supportedDevices.map(getDevice).filter(Boolean),
      }];

  if (groups.length <= 1) {
    const items = groups[0]?.items || [];
    return `<div class="device-options">${items.map((item) => renderHardwareOption(platform, item)).join("")}</div>`;
  }

  return groups.map((group) => `
    <div class="hardware-series-group">
      <div class="hardware-series-head">
        <p class="eyebrow">${group.title}</p>
        <p class="hardware-series-desc">${group.description || ""}</p>
      </div>
      <div class="device-options">${group.items.map((item) => renderHardwareOption(platform, item)).join("")}</div>
    </div>
  `).join("");
}

function renderPlatformCard(platform) {
  const isExpanded = platform.id === expandedPlatformId;
  const bullets = platform.bullets
    .map((item) => `<li>${item}</li>`)
    .join("");

  return `
    <article class="platform-card ${isExpanded ? "is-expanded" : ""}" style="--platform-accent:${platform.accent};--platform-highlight:${platform.highlight};">
      <button class="platform-card-main" data-expand-platform="${platform.id}" type="button">
        <span class="platform-logo-wrap">
          <img src="${platform.logo}" alt="${platform.name} logo">
        </span>
        <span class="platform-card-copy">
          <strong>${platform.name}</strong>
          <span>${platform.tagline}</span>
        </span>
        <span class="platform-card-action">${isExpanded ? "Expanded" : "View"}</span>
      </button>
      ${renderPlatformCreditMeta(platform)}
      <div class="platform-detail">
        <div class="platform-detail-copy">
          <p>${platform.description}</p>
          <ul>${bullets}</ul>
        </div>
        <figure class="platform-preview">
          <img src="${platform.preview}" alt="${platform.previewAlt}">
        </figure>
        <div class="device-choice">
          <div>
            <p class="eyebrow">Supported hardware</p>
            <h3>Select device or driver board</h3>
          </div>
          <div class="hardware-groups">${renderHardwareGroups(platform)}</div>
        </div>
      </div>
    </article>
  `;
}

function renderPlatformCards() {
  const container = document.getElementById("platformGrid");
  if (!container) return;

  container.innerHTML = PLATFORM_GROUPS.map((group) => {
    const platforms = PLATFORM_CARDS.filter((platform) => platform.group === group.id);
    if (!platforms.length) return "";

    return `
      <section class="platform-group" aria-labelledby="platformGroup-${group.id}">
        <div class="platform-group-head">
          <h3 id="platformGroup-${group.id}">${group.title}</h3>
          <p>${group.description}</p>
        </div>
        <div class="platform-group-grid">
          ${platforms.map(renderPlatformCard).join("")}
        </div>
      </section>
    `;
  }).join("");

  container.querySelectorAll("[data-expand-platform]").forEach((btn) => {
    btn.addEventListener("click", () => {
      expandedPlatformId = btn.dataset.expandPlatform;
      renderPlatformCards();
    });
  });

  container.querySelectorAll("[data-device]").forEach((btn) => {
    btn.addEventListener("click", () => {
      selectPlatformDevice(btn.dataset.platform, btn.dataset.device);
    });
  });
}

function collapsePlatformCards() {
  if (!expandedPlatformId) return;
  expandedPlatformId = null;
  renderPlatformCards();
}

function bindBlankAreaCollapse() {
  document.addEventListener("click", (event) => {
    const target = event.target;
    if (!(target instanceof Element)) return;
    if (target.closest(".platform-card")) return;
    if (target.closest("button, a, input, select, textarea")) return;
    collapsePlatformCards();
  });
}

// Selects the platform-device pair that drives the remaining setup flow.
// 选择平台和设备组合，用来驱动后续配置流程。
function selectPlatformDevice(platformId, deviceId) {
  document.body.classList.remove("is-flash-step");
  selectedPlatform =
    PLATFORM_CARDS.find((platform) => platform.id === platformId) || null;
  selectedDevice = getDevice(deviceId);
  const availableOptions = getAvailableFirmwareOptions(selectedPlatform, deviceId);
  selectedFirmwareOption = chooseFirmwareOption(availableOptions);
  selectedVersion = getDefaultVersion(selectedPlatform);
  selectedPanel = choosePanel(getSelectedCompatiblePanels());
  applyRecommendedInstallMode();

  renderSelectedRelease();
  renderSetupPanel();
  renderConfigArea();
  renderFlashNotes();
  updateFlashState();
  renderFlowState();
  resetProgress();
  const panelLabel = selectedPanel ? ` / ${selectedPanel.name}` : "";
  appendLog(
    `[system] Selected platform: ${selectedPlatform?.name || "None"} / ${selectedDevice?.name || "None"}${panelLabel}`
  );
}

function clearPlatformSelection() {
  document.body.classList.remove("is-flash-step");
  selectedPlatform = null;
  selectedDevice = null;
  selectedPanel = null;
  selectedFirmwareOption = null;
  selectedVersion = null;
  renderPlatformCards();
  renderFlowState();
  renderFlashNotes();
  updateFlashState();
  resetProgress();
}

function renderFlowState() {
  const hasSelection = Boolean(selectedPlatform && selectedDevice);
  const selectionPanel = document.getElementById("selectionPanel");
  const selectedPanel = document.getElementById("selectedPanel");
  const versionPanel = document.getElementById("versionPanel");
  const flashPanel = document.getElementById("flashPanel");

  const isTemplate = Boolean(selectedPlatform?.templateMode);
  const isDownload = Boolean(selectedPlatform?.downloadMode);
  const isExternalTool = Boolean(selectedPlatform?.externalTool);
  document.body.classList.toggle("has-selection", hasSelection);
  document.body.classList.toggle("is-external-tool", hasSelection && isExternalTool);
  if (selectionPanel) selectionPanel.classList.toggle("is-collapsed", hasSelection);
  toggleStepPanel(selectedPanel, hasSelection, 0);
  toggleStepPanel(versionPanel, hasSelection && !isDownload, 90);
  toggleStepPanel(flashPanel, hasSelection && !isExternalTool, 180);
  if (hasSelection && !isExternalTool) renderFlashPanelMode(isTemplate, isDownload);
  if (hasSelection && isDownload) renderDownloadGuide();
}

function toggleStepPanel(panel, visible, delayMs) {
  if (!panel) return;
  panel.classList.toggle("is-hidden", !visible);
  panel.classList.toggle("is-visible", visible);
  panel.style.setProperty("--step-delay", `${delayMs}ms`);
}

function renderSelectedRelease() {
  const container = document.getElementById("selectedRelease");
  if (!container || !selectedPlatform || !selectedDevice) return;

  const firmwareName = selectedFirmwareOption?.name || selectedPlatform.name;
  const firmwareDescription =
    selectedFirmwareOption?.description || selectedPlatform.description;
  const panelTag = selectedPanel
    ? `<span class="tag tag-panel">${selectedPanel.name}</span>`
    : "";
  const panelBadges = selectedPanel
    ? [
        selectedPanel.size,
        selectedPanel.resolution,
        selectedPanel.colorType,
        selectedPanel.interface,
      ]
        .filter(Boolean)
        .map((item) => `<span class="compat-badge active">${item}</span>`)
        .join("")
    : "";
  const photoSrc = selectedPanel?.image || selectedDevice.image;
  const photoAlt = selectedPanel?.imageAlt || selectedDevice.imageAlt;

  container.innerHTML = `
    <div class="selected-copy">
      <div class="selected-tags">
        <span class="tag tag-platform">${selectedPlatform.name}</span>
        <span class="tag tag-device">${selectedDevice.name}</span>
        ${panelTag}
      </div>
      <h3>${firmwareName}</h3>
      ${renderPlatformCreditMeta(selectedPlatform, "community-meta-selected", false)}
      <p>${firmwareDescription}</p>
      <div class="compat-list">
        ${renderDeviceSpecs(selectedDevice, "compat-badge active")}
        ${panelBadges}
        ${renderPlatformDetailTags(selectedPlatform, "compat-badge active")}
      </div>
    </div>
    <div class="selected-device-photo">
      <img src="${photoSrc}" alt="${photoAlt}">
    </div>
  `;
}

function renderSetupPanel() {
  renderPanelSelect();
  renderFirmwareSelect();
  renderLanguageSelect();
  renderVersionPanel();
}

function renderPanelSelect() {
  const field = document.getElementById("panelField");
  if (!field) return;

  if (!hardwareNeedsPanel()) {
    field.classList.add("is-hidden");
    field.innerHTML = "";
    return;
  }

  const panels = getSelectedCompatiblePanels();
  field.classList.remove("is-hidden");
  field.innerHTML = `
    <span>Screen panel</span>
    <select id="panelSelect" required>
      <option value="">Select a panel…</option>
      ${panels.map((panel) => {
        const selectedAttr = panel.id === selectedPanel?.id ? "selected" : "";
        const colorLabel = panel.colorType === "spectra6"
          ? "Spectra 6"
          : panel.colorType === "quad"
            ? "4-color"
            : panel.flexible
              ? "Flexible"
              : "Mono";
        return `<option value="${panel.id}" ${selectedAttr}>${panel.name} · ${panel.resolution} · ${colorLabel}</option>`;
      }).join("")}
    </select>
    <div class="panel-preview ${selectedPanel ? "" : "is-hidden"}" id="panelPreview">
      ${selectedPanel ? `<img src="${selectedPanel.image}" alt="${selectedPanel.imageAlt}">` : ""}
    </div>
  `;
}

function renderFirmwareSelect() {
  const field = document.getElementById("firmwareField");
  if (!field || !selectedPlatform || !selectedDevice) return;

  if (selectedPlatform?.templateMode) {
    const options = selectedPlatform.templateOptions || [];
    field.classList.add("field-block--template-options");
    field.classList.toggle("is-hidden", options.length === 0);
    field.innerHTML = `
      <span>Template options</span>
      <div class="template-options-grid">
        ${options.map((option) => `
          <label class="field-block field-block--checkbox" for="${option.id}">
            <span>
              ${option.label}
              <br>
              <small>${option.description}</small>
            </span>
            <input id="${option.id}" name="${option.id}" type="checkbox" ${option.defaultChecked === true ? "checked" : ""}>
          </label>
        `).join("")}
      </div>
    `;
    if (!field.dataset.templateListener) {
      field.addEventListener("change", (e) => {
        if (e.target?.type === "checkbox") renderTemplatePreview();
      });
      field.dataset.templateListener = "true";
    }
    renderTemplatePreview();
    return;
  }

  field.classList.remove("field-block--template-options");
  const options = getFirmwareBaseOptions(selectedPlatform, selectedDevice.id);
  const labelText =
    selectedPlatform.id === "base"
      ? "Arduino demo"
      : selectedPlatform.group === "community"
        ? "Project firmware"
        : "Firmware option";

  field.innerHTML = `
    <span>${labelText}</span>
    <select id="firmwareSelect"></select>
  `;
  const nextSelect = document.getElementById("firmwareSelect");

  field.classList.toggle("is-hidden", options.length === 0);
  if (!options.length) {
    if (nextSelect) nextSelect.innerHTML = "";
    return;
  }

  if (!nextSelect) return;
  nextSelect.innerHTML = options.map((firmware) => {
    const group = getFirmwareVariantGroup(firmware);
    const selectedAttr =
      group === getFirmwareVariantGroup(selectedFirmwareOption) ? "selected" : "";
    return `<option value="${group}" ${selectedAttr}>${firmware.baseName || firmware.name}</option>`;
  }).join("");
}

function renderLanguageSelect() {
  const field = document.getElementById("languageField");
  if (!field || !selectedPlatform || !selectedDevice || !selectedFirmwareOption) return;

  const variants = getLanguageVariantsForFirmware(
    selectedPlatform,
    selectedDevice.id,
    selectedFirmwareOption
  );
  field.classList.toggle("is-hidden", variants.length <= 1);
  const select = document.getElementById("languageSelect");
  if (!select) return;

  if (variants.length <= 1) {
    select.innerHTML = "";
    return;
  }

  select.innerHTML = variants.map((firmware) => {
    const selectedAttr = firmware.id === selectedFirmwareOption?.id ? "selected" : "";
    return `<option value="${firmware.id}" ${selectedAttr}>${firmware.languageLabel || firmware.name}</option>`;
  }).join("");
}

function renderVersionPanel() {
  const versionSelect = document.getElementById("versionSelect");
  if (!versionSelect || !selectedPlatform) return;

  const isTemplate = Boolean(selectedPlatform?.templateMode);
  const isExternalTool = Boolean(selectedPlatform?.externalTool);
  const versionField = versionSelect.closest(".field-block");
  if (versionField) versionField.classList.toggle("is-hidden", isTemplate || isExternalTool);

  const versionPanel = document.getElementById("versionPanel");
  const panelHeading = versionPanel?.querySelector(".panel-heading h2");
  if (panelHeading) {
    if (isExternalTool) {
      panelHeading.textContent = selectedPlatform.externalTool.stepTitle || "Official toolbox";
    }
    else panelHeading.textContent = isTemplate ? "Template setup" : "Version and setup";
  }

  if (isTemplate || isExternalTool) return;

  const versions = getVersionOptions(selectedPlatform);
  if (!versions.some((item) => item.version === selectedVersion?.version)) {
    selectedVersion = versions[0] || null;
  }

  versionSelect.disabled = !versions.length;
  if (!versions.length) {
    versionSelect.innerHTML = `<option value="" selected disabled>Version list unavailable</option>`;
    return;
  }

  versionSelect.innerHTML = versions.map((item) => {
    const label = item.label && item.label !== item.version
      ? `${item.version} - ${item.label}`
      : item.version;
    const selectedAttr = item.version === selectedVersion?.version ? "selected" : "";
    return `<option value="${item.version}" ${selectedAttr}>${label}</option>`;
  }).join("");
}

// Renders setup and flashing notes with shared alert styling.
// 使用统一提示样式渲染配置提示和烧录提示。
function renderAlertNotes(notes) {
  return notes.map((note) => `
    <div class="alert alert-${note.type === "warning" ? "warning" : "info"} is-visible">
      <span>${note.text}</span>
    </div>
  `).join("");
}

function getExternalToolUrl(tool, device) {
  return tool.urlsByDevice?.[device?.id] || tool.url;
}

function renderExternalToolConfig(tool, device) {
  const toolUrl = getExternalToolUrl(tool, device);
  return `
    <div class="external-tool-card">
      <strong>${tool.title}</strong>
      <span>${tool.description}</span>
      <a class="button" href="${toolUrl}" target="_blank" rel="noopener">${tool.label}</a>
    </div>
  `;
}

function renderConfigArea() {
  const container = document.getElementById("configArea");
  if (!container || !selectedPlatform) return;

  const externalTool = selectedPlatform.externalTool;
  container.classList.toggle("config-area--external", Boolean(externalTool));

  if (externalTool) {
    container.innerHTML = renderExternalToolConfig(externalTool, selectedDevice);
    return;
  }

  const notes = selectedFirmwareOption?.notes || [];
  const platformFields = selectedPlatform?.configFields || [];
  const fwFields = selectedFirmwareOption?.configFields || [];
  const allFields = [...platformFields, ...fwFields];
  const notesHtml = renderAlertNotes(notes);

  if (!allFields.length) {
    container.innerHTML = notesHtml || `
      <div class="config-empty">
        <strong>No setup fields required</strong>
        <span>This platform can continue without pre-configuring network or API settings.</span>
      </div>
    `;
    return;
  }

  container.innerHTML = notesHtml + allFields.map((field) => {
    let control = "";
    if (field.type === "select") {
      const options = field.options || [];
      control = `
        <select id="${field.id}">
          ${options.map((option) => `
            <option value="${option.value}" ${option.value === field.defaultValue ? "selected" : ""}>${option.label}</option>
          `).join("")}
        </select>
      `;
    } else if (field.type === "checkbox") {
      control = `<input id="${field.id}" type="checkbox" ${field.defaultValue ? "checked" : ""}>`;
    } else if (field.type === "number") {
      const minAttr = field.min !== undefined ? ` min="${field.min}"` : "";
      const maxAttr = field.max !== undefined ? ` max="${field.max}"` : "";
      const stepAttr = field.step !== undefined ? ` step="${field.step}"` : "";
      control = `<input id="${field.id}" type="number" value="${field.defaultValue}"${minAttr}${maxAttr}${stepAttr}>`;
    } else if (field.type === "password") {
      control = `<input id="${field.id}" type="password" value="${field.defaultValue || ""}" placeholder="${field.placeholder || ""}">`;
    } else {
      control = `<input id="${field.id}" type="text" value="${field.defaultValue || ""}" placeholder="${field.placeholder || ""}">`;
    }

    const blockClass = field.type === "checkbox" ? "field-block field-block--checkbox" : "field-block";
    return `
      <label class="${blockClass}" for="${field.id}">
        <span>${field.label}</span>
        ${control}
      </label>
    `;
  }).join("");

  if (!container.dataset.templateInputListener) {
    container.addEventListener("input", (e) => {
      if (!selectedPlatform?.templateMode) return;
      const fieldId = e.target?.id;
      const field = (selectedPlatform.configFields || []).find((item) => item.id === fieldId);
      if (field && (field.type === "text" || field.type === "password")) renderTemplatePreview();
    });
    container.dataset.templateInputListener = "true";
  }
}

function renderFlashNotes() {
  const container = document.getElementById("flashNotes");
  if (!container) return;
  container.innerHTML = renderAlertNotes(selectedFirmwareOption?.flashNotes || []);
}

function collectConfigValues() {
  const platformFields = selectedPlatform?.configFields || [];
  const fwFields = selectedFirmwareOption?.configFields || [];
  const allFields = [...platformFields, ...fwFields];
  const values = allFields.map((field) => {
    const el = document.getElementById(field.id);
    if (!el) return null;
    let value;
    if (field.type === "checkbox") value = el.checked ? 1 : 0;
    else if (field.type === "select" || field.type === "number") value = field.nvsType === "float" ? parseFloat(el.value) : parseInt(el.value, 10);
    else value = el.value || field.defaultValue || "";
    if (field.nvsKey === "imagePath" && typeof value === "string" && value.length > 0 && value[0] !== "/") {
      value = "/" + value;
    }
    let nvsType = field.nvsType;
    let nvsValue = value;
    if (nvsType === "float") {
      const buf = new ArrayBuffer(4);
      new DataView(buf).setFloat32(0, value, true);
      nvsType = "blob";
      nvsValue = new Uint8Array(buf);
    }
    return { key: field.nvsKey, type: nvsType, value: nvsValue };
  }).filter(Boolean);

  return values;
}

// Reads template field values from the platform-level setup inputs.
function getTemplateFieldValues() {
  const values = {};
  (selectedPlatform?.configFields || []).forEach((field) => {
    const el = document.getElementById(field.id);
    if (!el) return;
    values[field.id] = field.type === "checkbox" ? el.checked : el.value || "";
  });
  return values;
}

let eraseBeforeFlash = false;
let isFlashing = false;
let lastFlashedPort = null;

// Applies the selected firmware's default install mode to the flash controls.
// 将所选固件的默认安装模式同步到烧录控制按钮。
function setInstallMode(mode, { log = false } = {}) {
  const modeStandard = document.getElementById("modeStandard");
  const modeErase = document.getElementById("modeErase");
  const normalizedMode = mode === "erase" ? "erase" : "standard";

  eraseBeforeFlash = normalizedMode === "erase";
  if (modeStandard) {
    modeStandard.classList.toggle("is-active", normalizedMode === "standard");
  }
  if (modeErase) modeErase.classList.toggle("is-active", normalizedMode === "erase");
  if (log) {
    appendLog(`[system] Flash mode: ${normalizedMode === "erase" ? "erase + flash" : "standard"}`);
  }
}

// Reads the firmware option's preferred install mode, defaulting to standard flash.
// 读取固件选项的推荐安装模式，默认使用标准烧录。
function applyRecommendedInstallMode() {
  setInstallMode(selectedFirmwareOption?.recommendedInstallMode || "standard");
}

function updateFlashState() {
  const flashBtn = document.getElementById("flashButton");
  const disabledBtn = document.getElementById("disabledFlashButton");
  const installNote = document.getElementById("installNote");
  const flashHint = document.getElementById("flashHint");
  const isUf2 = getFlashMethod() === "uf2";
  const panelMissing = hardwareNeedsPanel() && !selectedPanel;
  const uf2Url = getUf2DownloadUrl();
  const manifest = getInstallManifest();
  const ready = Boolean(
    selectedPlatform?.installReady &&
    !panelMissing &&
    (isUf2 ? uf2Url : manifest)
  );
  const versionListMissing = Boolean(
    selectedPlatform?.installReady &&
    selectedPlatform?.requiresVersionManifest &&
    selectedFirmwareOption &&
    !getFirmwareVersionValues(selectedFirmwareOption, selectedPlatform).length
  );

  if (flashBtn) flashBtn.classList.toggle("is-hidden", !ready || isUf2);
  if (disabledBtn) disabledBtn.classList.toggle("is-hidden", ready || isUf2);
  if (flashHint) {
    flashHint.textContent = isUf2
      ? "Double-tap the reset button until the XIAO-BOOT drive appears, then download the .uf2 file."
      : hardwareNeedsPanel()
        ? "Connect your DIY Kit board with a USB data cable first."
        : "Connect your reTerminal with a USB data cable first.";
  }
  if (installNote) {
    const showNote = !ready && !selectedPlatform?.templateMode && !selectedPlatform?.downloadMode;
    installNote.classList.toggle("is-visible", showNote);
    if (showNote) {
      if (panelMissing) {
        installNote.innerHTML = `<strong>Select a screen panel.</strong>
          <span>Choose the ePaper panel attached to your driver board before flashing.</span>`;
      } else if (versionListMissing) {
        installNote.innerHTML = `<strong>Version list unavailable.</strong>
          <span>The firmware version manifest could not be loaded, so flashing is disabled. Reload the page or try again later.</span>`;
      } else {
        installNote.innerHTML = `<strong>Firmware not published yet.</strong>
          <span>This platform card is ready, but its web flashing package has not been added.</span>`;
      }
    }
  }
  renderUf2Panel(isUf2, ready, uf2Url);
}

// Assembles template output from platform-level header, selected snippets, and footer.
function buildTemplateContent(
  platform,
  selectedOptionIds,
  deviceId = selectedDevice?.id,
  userValues = getTemplateFieldValues()
) {
  if (platform?.templateMode && typeof globalThis.buildEsphomeTemplateContent === "function") {
    const hasStructuredOptions = (platform.templateOptions || []).some((option) => option.contributes);
    if (hasStructuredOptions) {
      return globalThis.buildEsphomeTemplateContent(platform, selectedOptionIds, deviceId, userValues);
    }
  }

  const joiner = platform.templateJoiner || "\n\n";
  const parts = [];
  const header = platform.templateHeader || "";
  const footer = platform.templateFooter || "";
  if (header) parts.push(header);
  const snippets = selectedOptionIds
    .map((id) => {
      const opt = (platform.templateOptions || []).find((o) => o.id === id);
      return opt?.snippet;
    })
    .filter(Boolean);
  parts.push(...snippets);
  if (footer) parts.push(footer);
  return parts.join(joiner) + "\n";
}

// Reads checkbox state from the DOM and returns checked option IDs.
function getCheckedTemplateOptionIds() {
  if (!selectedPlatform?.templateMode) return [];
  return (selectedPlatform.templateOptions || [])
    .filter((option) => {
      const input = document.getElementById(option.id);
      return input?.checked;
    })
    .map((option) => option.id);
}

// Updates the Step 3 template code preview with the current checkbox selection.
function renderTemplatePreview() {
  const codeEl = document.getElementById("templateCode");
  if (!codeEl || !selectedPlatform?.templateMode) return;
  const ids = getCheckedTemplateOptionIds();
  const userValues = getTemplateFieldValues();
  codeEl.textContent = buildTemplateContent(selectedPlatform, ids, selectedDevice?.id, userValues);
}

// Downloads the generated template as a file.
function generateTemplateFile() {
  if (!selectedPlatform?.templateMode || !selectedDevice) return;
  const ids = getCheckedTemplateOptionIds();
  const userValues = getTemplateFieldValues();
  const content = buildTemplateContent(selectedPlatform, ids, selectedDevice.id, userValues);
  const ext = selectedPlatform.templateFileExtension || "txt";
  const mime = selectedPlatform.templateFileMimeType || "text/plain";
  const pattern = selectedPlatform.templateFilePattern || "{platformId}-{deviceId}";
  const filename = pattern
    .replace("{platformId}", selectedPlatform.id)
    .replace("{deviceId}", selectedDevice.id.toLowerCase()) + "." + ext;
  const blob = new Blob([content], { type: mime });
  const url = URL.createObjectURL(blob);
  const link = document.createElement("a");
  link.href = url;
  link.download = filename;
  document.body.appendChild(link);
  link.click();
  link.remove();
  URL.revokeObjectURL(url);
  appendLog(`[system] Downloaded template: ${filename}`);
}

// Copies the template preview content to clipboard with button feedback.
function copyTemplateToClipboard() {
  const codeEl = document.getElementById("templateCode");
  const btn = document.getElementById("copyTemplateButton");
  if (!codeEl) return;
  const text = codeEl.textContent;
  navigator.clipboard.writeText(text).then(() => {
    if (btn) {
      const original = btn.innerHTML;
      btn.textContent = "Copied!";
      setTimeout(() => { btn.innerHTML = original; }, 1500);
    }
    appendLog("[system] Template copied to clipboard.");
  }).catch(() => {
    appendLog("[system] Failed to copy - use the download button instead.");
  });
}

// Toggles flash-panel child elements between flash, UF2, template, and download modes.
// 在串口烧录、UF2、模板预览、下载模式之间切换 Step 3 面板内容。
function renderFlashPanelMode(isTemplate, isDownload) {
  const flashPanel = document.getElementById("flashPanel");
  if (!flashPanel) return;
  const heading = flashPanel.querySelector(".panel-heading h2");
  const installMode = flashPanel.querySelector(".install-mode");
  const flashNotes = document.getElementById("flashNotes");
  const installNote = document.getElementById("installNote");
  const errorAlert = document.getElementById("errorAlert");
  const progressRow = flashPanel.querySelector(".progress-row");
  const flashActions = flashPanel.querySelector(".flash-actions");
  const templatePreview = document.getElementById("templatePreview");
  const templateExport = document.getElementById("templateExportActions");
  const downloadGuide = document.getElementById("downloadGuide");
  const downloadActions = document.getElementById("downloadActions");
  const uf2Panel = document.getElementById("uf2Panel");
  const isUf2 = !isTemplate && !isDownload && getFlashMethod() === "uf2";

  if (heading) {
    if (isDownload) heading.textContent = "Download and build";
    else if (isTemplate) heading.textContent = "Preview and export";
    else if (isUf2) heading.textContent = "Install via UF2";
    else heading.textContent = "Flash to device";
  }
  const hideFlash = isTemplate || isDownload || isUf2;
  if (installMode) installMode.classList.toggle("is-hidden", hideFlash);
  if (flashNotes) flashNotes.classList.toggle("is-hidden", isTemplate || isDownload);
  if (progressRow) progressRow.classList.toggle("is-hidden", hideFlash);
  if (flashActions) flashActions.classList.toggle("is-hidden", hideFlash);
  if (templatePreview) templatePreview.classList.toggle("is-hidden", !isTemplate);
  if (templateExport) templateExport.classList.toggle("is-hidden", !isTemplate);
  if (downloadGuide) downloadGuide.classList.toggle("is-hidden", !isDownload);
  if (downloadActions) downloadActions.classList.toggle("is-hidden", !isDownload);
  if (uf2Panel) uf2Panel.classList.toggle("is-hidden", !isUf2);
  if (isTemplate && installNote) installNote.classList.remove("is-visible");
  if (isTemplate && errorAlert) errorAlert.classList.remove("is-visible");
  if (isDownload && installNote) installNote.classList.remove("is-visible");
  if (isDownload && errorAlert) errorAlert.classList.remove("is-visible");
  if (isUf2) updateFlashState();
}

function renderUf2Panel(isUf2, ready, uf2Url) {
  const uf2Panel = document.getElementById("uf2Panel");
  const downloadBtn = document.getElementById("uf2DownloadButton");
  const writeBtn = document.getElementById("uf2WriteButton");
  if (!uf2Panel) return;

  const volume = selectedDevice?.uf2VolumeLabel || "XIAO-BOOT";
  const boardName = selectedDevice?.name || "board";
  const panelName = selectedPanel?.name || "panel";
  uf2Panel.querySelectorAll("[data-uf2-volume]").forEach((el) => {
    el.textContent = volume;
  });
  uf2Panel.querySelectorAll("[data-uf2-target]").forEach((el) => {
    el.textContent = `${boardName} + ${panelName}`;
  });

  if (downloadBtn) {
    downloadBtn.classList.toggle("is-disabled", !ready);
    if (ready && uf2Url) {
      downloadBtn.href = uf2Url;
      downloadBtn.setAttribute("download", `${getResolvedFirmwareId()}.uf2`);
      downloadBtn.removeAttribute("aria-disabled");
    } else {
      downloadBtn.href = "#";
      downloadBtn.removeAttribute("download");
      downloadBtn.setAttribute("aria-disabled", "true");
    }
  }
  if (writeBtn) {
    writeBtn.disabled = !ready || !("showDirectoryPicker" in window);
    writeBtn.title = "showDirectoryPicker" in window
      ? "Write the .uf2 file directly to the XIAO-BOOT drive"
      : "Direct write needs Chrome or Edge";
  }
}

async function writeUf2ToDrive() {
  const uf2Url = getUf2DownloadUrl();
  if (!uf2Url || !selectedFirmwareOption || !selectedDevice || !selectedPanel) return;
  if (!("showDirectoryPicker" in window)) {
    appendLog("[uf2] Direct write is unavailable in this browser. Download the .uf2 file instead.");
    return;
  }

  try {
    appendLog("[uf2] Choose the XIAO-BOOT drive…");
    const dirHandle = await window.showDirectoryPicker({ mode: "readwrite" });
    appendLog("[uf2] Downloading firmware…");
    const response = await fetch(uf2Url);
    if (!response.ok) throw new Error(`UF2 download failed: ${response.status}`);
    const data = new Uint8Array(await response.arrayBuffer());
    const filename = `${getResolvedFirmwareId()}.uf2`;
    const fileHandle = await dirHandle.getFileHandle(filename, { create: true });
    const writable = await fileHandle.createWritable();
    await writable.write(data);
    await writable.close();
    appendLog(`[uf2] Wrote ${filename} to ${selectedDevice.uf2VolumeLabel || "XIAO-BOOT"}. The board should reboot automatically.`);
  } catch (error) {
    if (error?.name === "AbortError") {
      appendLog("[uf2] Drive selection cancelled.");
      return;
    }
    appendLog(`[uf2] ${error.message || error}`);
  }
}

function renderDownloadGuide() {
  const container = document.getElementById("downloadGuide");
  if (!container || !selectedPlatform?.downloadMode) return;

  const steps = selectedPlatform.downloadSteps || [];
  const devName = selectedDevice?.name || "";
  const devId = selectedDevice?.id || "";
  container.innerHTML = `
    <ol class="download-steps-list">
      ${steps.map((step, i) => `
        <li class="download-step">
          <div class="download-step-number">${i + 1}</div>
          <div class="download-step-content">
            <strong>${step.title}</strong>
            <p>${step.description.replace(/\{deviceName\}/g, devName).replace(/\{deviceId\}/g, devId)}</p>
          </div>
        </li>
      `).join("")}
    </ol>
    <div class="download-wiki-hint">
      Need more help? <a href="https://wiki.seeedstudio.com/reterminal_e10xx_with_eezstudio/" target="_blank" rel="noopener">Read the full tutorial on Seeed Studio Wiki →</a>
    </div>
  `;

  const downloadBtn = document.getElementById("downloadProjectButton");
  if (downloadBtn && selectedPlatform.downloadUrl) {
    downloadBtn.href = selectedPlatform.downloadUrl;
  }
}

// Fetches a binary file as the string format expected by esptool-js.
// 获取二进制文件，并转成 esptool-js 需要的字符串格式。
function fetchBinaryString(url) {
  return fetch(url).then((resp) => {
    if (!resp.ok) throw new Error(`Failed to download ${url}: ${resp.status}`);
    return resp.blob();
  }).then((blob) => new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => resolve(reader.result);
    reader.onerror = reject;
    reader.readAsBinaryString(blob);
  }));
}

async function flashDevice() {
  if (isFlashing) return;
  const manifest = getInstallManifest();
  if (!manifest) return;

  isFlashing = true;
  document.body.classList.add("is-flash-step");
  hideError();
  const flashBtn = document.getElementById("flashButton");
  if (flashBtn) flashBtn.disabled = true;

  let transport = null;
  try {
    // Reuse the monitor port if already connected; otherwise prompt the user.
    // Validate that a cached port is still physically present before reusing it.
    // 如果串口监视器正在连接，先断开并复用同一个端口，避免重复选择。
    // 复用前先检查端口是否仍然物理连接，防止设备拔出后静默失败。
    let port = null;
    const availablePorts = await navigator.serial.getPorts();

    if (monitorPort) {
      if (availablePorts.includes(monitorPort)) {
        appendLog("[flash] Disconnecting monitor to free the port...");
        setProgress("flash", 2, "Releasing serial port");
        port = monitorPort;
        await disconnectMonitor();
      } else {
        appendLog("[flash] Monitor port disconnected, cleaning up...");
        monitorPort = null;
        lastFlashedPort = null;
        setMonitorControls(false);
        setSerialState("disconnected", "Device disconnected");
      }
    }

    if (!port && lastFlashedPort) {
      if (availablePorts.includes(lastFlashedPort)) {
        appendLog("[flash] Reusing previously connected port...");
        setProgress("flash", 2, "Reusing serial port");
        port = lastFlashedPort;
      } else {
        appendLog("[flash] Previous port no longer available...");
        lastFlashedPort = null;
      }
    }

    if (!port) {
      appendLog("[flash] Requesting serial port...");
      setProgress("flash", 2, "Waiting for port selection");
      port = await navigator.serial.requestPort({
        filters: [
          { usbVendorId: 0x303a },
          { usbVendorId: 0x10c4 },
          { usbVendorId: 0x1a86 },
        ],
      });
    }

    appendLog("[flash] Loading esptool-js...");
    setProgress("flash", 5, "Loading flash tool");
    const { ESPLoader, Transport } = await import(
      "https://cdn.jsdelivr.net/npm/esptool-js@0.5.7/+esm"
    );

    transport = new Transport(port);
    const terminal = {
      clean: () => {},
      writeLine: (data) => appendLog(`[esptool] ${data}`),
      write: (data) => appendSerialChunk(data),
    };

    const esploader = new ESPLoader({
      transport,
      baudrate: 115200,
      romBaudrate: 115200,
      terminal,
      enableTracing: false,
    });

    appendLog("[flash] Connecting to device...");
    setProgress("flash", 8, "Connecting to device");
    await esploader.main();
    await esploader.flashId();
    const chip = esploader.chip.CHIP_NAME;
    appendLog(`[flash] Connected: ${chip}`);

    if (eraseBeforeFlash) {
      appendLog("[flash] Erasing entire flash...");
      setProgress("flash", 15, "Erasing flash memory");
      await esploader.eraseFlash();
      appendLog("[flash] Erase complete.");
    }

    appendLog("[flash] Downloading firmware...");
    setProgress("flash", 20, "Downloading firmware binaries");
    const manifestUrl = new URL(manifest, window.location.href).href;
    const manifestRes = await fetch(manifestUrl);
    if (!manifestRes.ok) throw new Error(`Manifest fetch failed: ${manifestRes.status}`);
    const manifestData = await manifestRes.json();

    const build = manifestData.builds?.find((b) => b.chipFamily === chip) || manifestData.builds?.[0];
    if (!build?.parts?.length) throw new Error("No compatible build found in manifest");

    const baseUrl = manifestUrl.substring(0, manifestUrl.lastIndexOf("/") + 1);
    const fileArray = [];
    let totalSize = 0;
    for (const part of build.parts) {
      if (!part.path) continue;
      const binUrl = new URL(part.path, baseUrl).href;
      appendLog(`[flash] Fetching ${part.path}...`);
      // esptool-js expects each binary as a binary string, not a Uint8Array.
      // esptool-js 需要二进制字符串，而不是 Uint8Array。
      const data = await fetchBinaryString(binUrl);
      fileArray.push({ data, address: part.offset });
      totalSize += data.length;
    }

    const configValues = collectConfigValues();
    if (configValues.length > 0) {
      appendLog("[flash] Generating user config for NVS partition...");
      const { generateNvsPartition, NVS_VERSION } = await import("./nvs.js?v=3");
      console.log(`[nvs] generator v${NVS_VERSION} loaded (expect v3)`);
      console.log("[nvs] config entries:", configValues.map(e => ({ key: e.key, type: e.type, value: e.value instanceof Uint8Array ? `blob[${e.value.length}]` : e.value })));
      const nvsData = generateNvsPartition(configValues);
      let nvsBinStr = "";
      for (let i = 0; i < nvsData.length; i++) nvsBinStr += String.fromCharCode(nvsData[i]);
      fileArray.push({ data: nvsBinStr, address: 0x9000 });
      totalSize += nvsData.length;
      appendLog(`[flash] NVS config: ${configValues.length} entries, ${nvsData.length} bytes at 0x9000`);
    }

    appendLog(`[flash] Writing ${fileArray.length} partitions...`);
    setProgress("flash", 30, "Writing firmware to device");

    let totalWritten = 0;
    const flashSize = manifestData.flashSize || build.flashSize || "keep";
    await esploader.writeFlash({
      fileArray,
      flashSize,
      flashMode: "keep",
      flashFreq: "keep",
      eraseAll: false,
      compress: true,
      reportProgress: (fileIndex, written, total) => {
        const uncompressedWritten = (written / total) * fileArray[fileIndex].data.length;
        if (written === total) {
          totalWritten += uncompressedWritten;
          return;
        }
        const overall = 30 + ((totalWritten + written) / totalSize) * 65;
        setProgress("flash", overall, `Writing firmware (${Math.floor(((totalWritten + written) / totalSize) * 100)}%)`);
      },
    });

    appendLog("[flash] Firmware installed successfully!");
    setProgress("flash", 98, "Resetting device");

    // Hard reset asserts RTS, then releases the chip through esptool-js.
    // 硬复位会先拉低 RTS，再通过 esptool-js 释放芯片。
    await transport.setRTS(true);
    await new Promise((r) => setTimeout(r, 100));
    await esploader.after();
    appendLog("[flash] Device reset. New firmware running.");
    setProgress("flash", 100, "Firmware installed successfully");

    await transport.disconnect();
    lastFlashedPort = port;

    appendLog("[system] Auto-connecting serial monitor...");
    await autoConnectMonitor(port);

  } catch (error) {
    const msg = error?.message || "Flash failed";
    appendLog(`[error] ${msg}`);
    setProgress("flash", 0, "Flash failed");
    const alert = document.getElementById("errorAlert");
    const msgEl = document.getElementById("errorMessage");
    if (alert && msgEl) {
      msgEl.textContent = msg;
      alert.classList.add("is-visible");
    }
    // Releases the port so the next flash attempt or monitor can use it.
    // 释放串口，方便下一次烧录或串口监视器继续使用。
    if (transport) {
      try {
        await transport.disconnect();
      } catch (_) {
        // The port is already closed or was never opened.
        // 串口已经关闭，或从未成功打开。
      }
    }
  } finally {
    isFlashing = false;
    if (flashBtn) flashBtn.disabled = false;
  }
}

async function autoConnectMonitor(port) {
  await new Promise((resolve) => setTimeout(resolve, 1500));
  try {
    await port.open({ baudRate: monitorBaudRate });
    monitorPort = port;
    monitorKeepReading = true;
    setMonitorControls(true, port);
    setSerialState("connected", "Monitor connected");
    appendLog(`[monitor] Auto-connected at ${monitorBaudRate} baud.`);
    void readMonitorLoop();
  } catch (error) {
    appendLog(`[monitor] Auto-connect failed: ${error.message || "Unknown error"}. Click "Connect monitor" manually.`);
  }
}

function resetProgress() {
  setProgress("flash", 0, "Select platform, device, and firmware to begin");
  hideError();
}

function setProgress(kind, percent, message) {
  const value = document.getElementById(`${kind}Value`);
  const bar = document.getElementById(`${kind}Bar`);
  const status = document.getElementById(`${kind}Status`);
  if (value) value.textContent = `${Math.round(percent)}%`;
  if (bar) bar.style.width = `${Math.min(Math.max(percent, 0), 100)}%`;
  if (status) status.textContent = message;
}

function hideError() {
  const el = document.getElementById("errorAlert");
  if (el) el.classList.remove("is-visible");
}

function appendLog(message) {
  appendLogText(`${message}\n`, true);
}

function appendSerialChunk(message) {
  pendingSerialText += String(message || "");
  if (pendingSerialText.length > SERIAL_PENDING_MAX_LENGTH) {
    pendingSerialText = pendingSerialText.slice(-SERIAL_PENDING_MAX_LENGTH);
    const firstLineBreak = pendingSerialText.indexOf("\n");
    if (firstLineBreak >= 0 && firstLineBreak < pendingSerialText.length - 1) {
      pendingSerialText = pendingSerialText.slice(firstLineBreak + 1);
    }
    pendingSerialTrimmed = true;
  }
  scheduleLogRender();
}

function scheduleLogRender() {
  if (logRenderTimer !== null) return;
  logRenderTimer = window.setTimeout(() => {
    logRenderTimer = null;
    flushPendingSerialText();
    if (!logPaused) refreshLogView();
  }, LOG_RENDER_INTERVAL_MS);
}

function flushPendingSerialText() {
  if (!pendingSerialText && !pendingSerialTrimmed) return;
  if (pendingSerialTrimmed) {
    logBuffer.append(SERIAL_PENDING_TRIM_NOTICE, true);
    pendingSerialTrimmed = false;
  }
  if (pendingSerialText) {
    logBuffer.append(pendingSerialText, false);
    pendingSerialText = "";
  }
}

// Stores bounded log text and batches visible updates to avoid UI freezes.
// 保存限长日志，并批量刷新可见窗口，避免界面卡死。
function appendLogText(message, prefixLineBreak) {
  flushPendingSerialText();
  logBuffer.append(message, prefixLineBreak);
  if (logPaused) return;
  scheduleLogRender();
}

function seedLogBuffer() {
  const log = document.getElementById("log");
  if (!log) return;
  pendingSerialText = "";
  pendingSerialTrimmed = false;
  logBuffer.seed(log.textContent || "");
}

function refreshLogView() {
  const log = document.getElementById("log");
  if (!log) return;
  flushPendingSerialText();
  log.textContent = logBuffer.view(LOG_VISIBLE_MAX_LENGTH);
  log.scrollTop = log.scrollHeight;
}

function setSerialState(state, label) {
  const el = document.getElementById("serialStatus");
  const led = document.querySelector(".monitor-led");
  const stateText = document.getElementById("monitorStateText");
  if (el) {
    el.className = `serial-status state-${state}`;
    el.innerHTML = `<i></i><b>${label}</b>`;
  }
  if (led) {
    led.className = `monitor-led state-${state}`;
  }
  if (stateText) {
    stateText.textContent = state === "connected" ? "Connected" : label;
    stateText.className = `state-${state}`;
  }
}

function setMonitorControls(connected, port = null) {
  const connectBtn = document.getElementById("connectMonitorButton");
  const disconnectBtn = document.getElementById("disconnectMonitorButton");
  const portValue = document.getElementById("monitorPortValue");
  const baudSelect = document.getElementById("baudRateSelect");
  if (connectBtn) connectBtn.classList.toggle("is-hidden", connected);
  if (disconnectBtn) disconnectBtn.classList.toggle("is-hidden", !connected);
  if (portValue) {
    portValue.textContent = connected
      ? globalThis.formatSerialPortLabel(port)
      : "Unavailable";
  }
  if (baudSelect) baudSelect.disabled = connected;
}

async function connectMonitor() {
  if (!("serial" in navigator)) {
    setSerialState("error", "Unavailable");
    appendLog("[monitor] Web Serial is unavailable in this browser.");
    return;
  }

  try {
    monitorPort = await navigator.serial.requestPort();
    await monitorPort.open({ baudRate: monitorBaudRate });
    monitorKeepReading = true;
    setMonitorControls(true, monitorPort);
    setSerialState("connected", "Monitor connected");
    appendLog(`[monitor] Connected at ${monitorBaudRate} baud.`);
    void readMonitorLoop();
  } catch (error) {
    monitorPort = null;
    monitorKeepReading = false;
    setMonitorControls(false);
    setSerialState("error", "Monitor error");
    appendLog(`[monitor] ${error.message || "Connection failed"}`);
  }
}

// Reads Web Serial data until the user disconnects the monitor.
// 读取 Web Serial 数据，直到用户断开监视器。
async function readMonitorLoop() {
  while (monitorPort?.readable && monitorKeepReading) {
    const reader = monitorPort.readable.getReader();
    monitorReader = reader;
    let bytesSinceYield = 0;
    let chunksSinceYield = 0;
    try {
      while (monitorKeepReading) {
        const { value, done } = await reader.read();
        if (done) break;
        if (value) {
          bytesSinceYield += value.byteLength;
          chunksSinceYield += 1;
          appendSerialChunk(monitorDecoder.decode(value, { stream: true }));
          if (
            bytesSinceYield >= SERIAL_READ_YIELD_BYTES ||
            chunksSinceYield >= SERIAL_READ_YIELD_CHUNKS
          ) {
            bytesSinceYield = 0;
            chunksSinceYield = 0;
            await new Promise((resolve) => setTimeout(resolve, 0));
          }
        }
      }
    } catch (error) {
      if (monitorKeepReading) {
        setSerialState("error", "Read error");
        appendLog(`[monitor] ${error.message || "Read failed"}`);
      }
    } finally {
      reader.releaseLock();
      if (monitorReader === reader) monitorReader = null;
    }
  }
}

async function waitForMonitorReaderRelease() {
  for (let attempt = 0; monitorReader && attempt < 20; attempt += 1) {
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
}

async function disconnectMonitor() {
  monitorKeepReading = false;

  if (monitorReader) {
    try {
      await monitorReader.cancel();
      await waitForMonitorReaderRelease();
    } catch (error) {
      appendLog(`[monitor] ${error.message || "Reader cancel failed"}`);
    }
  }

  if (monitorPort) {
    try {
      await monitorPort.close();
      appendLog("[monitor] Disconnected.");
    } catch (error) {
      appendLog(`[monitor] ${error.message || "Disconnect failed"}`);
    }
  }

  monitorPort = null;
  setMonitorControls(false);
  setSerialState("disconnected", "Disconnected");
}

function clearLog() {
  pendingSerialText = "";
  pendingSerialTrimmed = false;
  logBuffer.clear();
  refreshLogView();
}

function toggleLogPaused() {
  const pauseBtn = document.getElementById("pauseLogButton");
  logPaused = !logPaused;
  if (pauseBtn) pauseBtn.textContent = logPaused ? "Resume" : "Pause";
  if (!logPaused) refreshLogView();
}

// Downloads the retained in-memory log as a local text file.
// 将内存中保留的日志下载为本地文本文件。
function saveLog() {
  flushPendingSerialText();
  const text = logBuffer.text();
  const blob = new Blob([text], { type: "text/plain;charset=utf-8" });
  const url = URL.createObjectURL(blob);
  const link = document.createElement("a");
  const stamp = new Date().toISOString().replace(/[:.]/g, "-");
  link.href = url;
  link.download = `device-log-${stamp}.txt`;
  document.body.appendChild(link);
  link.click();
  link.remove();
  URL.revokeObjectURL(url);
}

function bindFlowEvents() {
  const changeBtn = document.getElementById("changePlatformButton");
  if (changeBtn) {
    changeBtn.addEventListener("click", clearPlatformSelection);
  }

  const panelField = document.getElementById("panelField");
  if (panelField) {
    panelField.addEventListener("change", (event) => {
      if (event.target?.id !== "panelSelect") return;
      const panelId = event.target.value;
      selectedPanel = getSelectedCompatiblePanels().find((panel) => panel.id === panelId) || null;
      // Combo builds publish per-panel versions, so refresh the version list.
      // 组合构建按屏幕发布版本，因此切换屏幕后刷新版本列表。
      selectedVersion = getDefaultVersion(selectedPlatform);
      renderSelectedRelease();
      renderPanelSelect();
      renderVersionPanel();
      updateFlashState();
      renderFlowState();
      resetProgress();
      appendLog(`[system] Selected panel: ${selectedPanel?.name || "None"}`);
    });
  }

  const firmwareField = document.getElementById("firmwareField");
  if (firmwareField) {
    firmwareField.addEventListener("change", (event) => {
      if (event.target?.id !== "firmwareSelect") return;
      const firmwareSelect = event.target;
      const options = getAvailableFirmwareOptions(selectedPlatform, selectedDevice?.id);
      const previousLanguage = getFirmwareLanguage(selectedFirmwareOption);
      selectedFirmwareOption =
        chooseFirmwareOption(options, firmwareSelect.value, previousLanguage);
      selectedVersion = getDefaultVersion(selectedPlatform);
      selectedPanel = choosePanel(getSelectedCompatiblePanels(), selectedPanel?.id);
      applyRecommendedInstallMode();
      renderSelectedRelease();
      renderPanelSelect();
      renderLanguageSelect();
      renderVersionPanel();
      renderConfigArea();
      renderFlashNotes();
      updateFlashState();
      renderFlowState();
      resetProgress();
      appendLog(`[system] Selected demo: ${selectedFirmwareOption?.name || "None"}`);
    });
  }

  const languageField = document.getElementById("languageField");
  if (languageField) {
    languageField.addEventListener("change", (event) => {
      if (event.target?.id !== "languageSelect") return;
      const languageSelect = event.target;
      const variants = getLanguageVariantsForFirmware(
        selectedPlatform,
        selectedDevice?.id,
        selectedFirmwareOption
      );
      selectedFirmwareOption =
        variants.find((item) => item.id === languageSelect.value) ||
        selectedFirmwareOption;
      selectedVersion = getDefaultVersion(selectedPlatform);
      applyRecommendedInstallMode();
      renderSelectedRelease();
      renderVersionPanel();
      renderConfigArea();
      renderFlashNotes();
      updateFlashState();
      resetProgress();
      appendLog(`[system] Selected language: ${selectedFirmwareOption?.languageLabel || "None"}`);
    });
  }

  const versionSelect = document.getElementById("versionSelect");
  if (versionSelect) {
    versionSelect.addEventListener("change", () => {
      selectedVersion =
        getVersionOptions(selectedPlatform).find((item) => item.version === versionSelect.value) ||
        getDefaultVersion(selectedPlatform);
      updateFlashState();
      resetProgress();
      appendLog(`[system] Selected version: ${selectedVersion?.version || "None"}`);
    });
  }

  const modeStandard = document.getElementById("modeStandard");
  const modeErase = document.getElementById("modeErase");
  if (modeStandard && modeErase) {
    modeStandard.addEventListener("click", () =>
      setInstallMode("standard", { log: true })
    );
    modeErase.addEventListener("click", () =>
      setInstallMode("erase", { log: true })
    );
  }
}

function bindWorkspaceEvents() {
  seedLogBuffer();
  setMonitorControls(false);
  setSerialState("disconnected", "Disconnected");

  const baudSelect = document.getElementById("baudRateSelect");
  if (baudSelect) {
    monitorBaudRate = Number.parseInt(baudSelect.value, 10) || monitorBaudRate;
    baudSelect.addEventListener("change", () => {
      monitorBaudRate = Number.parseInt(baudSelect.value, 10) || 115200;
      appendLog(`[monitor] Baud rate set to ${monitorBaudRate}.`);
    });
  }

  const connectBtn = document.getElementById("connectMonitorButton");
  if (connectBtn) {
    connectBtn.addEventListener("click", connectMonitor);
    connectBtn.disabled = !("serial" in navigator);
  }

  const disconnectBtn = document.getElementById("disconnectMonitorButton");
  if (disconnectBtn) {
    disconnectBtn.addEventListener("click", disconnectMonitor);
  }

  const clearBtn = document.getElementById("clearLogButton");
  if (clearBtn) {
    clearBtn.addEventListener("click", clearLog);
  }

  const pauseBtn = document.getElementById("pauseLogButton");
  if (pauseBtn) {
    pauseBtn.addEventListener("click", toggleLogPaused);
  }

  const saveBtn = document.getElementById("saveLogButton");
  if (saveBtn) {
    saveBtn.addEventListener("click", saveLog);
  }

  const flashBtn = document.getElementById("flashButton");
  if (flashBtn) {
    flashBtn.addEventListener("click", flashDevice);
    flashBtn.disabled = !("serial" in navigator);
  }

  const uf2DownloadBtn = document.getElementById("uf2DownloadButton");
  if (uf2DownloadBtn) {
    uf2DownloadBtn.addEventListener("click", (event) => {
      if (uf2DownloadBtn.getAttribute("aria-disabled") === "true") {
        event.preventDefault();
      }
    });
  }

  const uf2WriteBtn = document.getElementById("uf2WriteButton");
  if (uf2WriteBtn) {
    uf2WriteBtn.addEventListener("click", writeUf2ToDrive);
  }

  const copyBtn = document.getElementById("copyTemplateButton");
  if (copyBtn) {
    copyBtn.addEventListener("click", copyTemplateToClipboard);
  }

  const downloadBtn = document.getElementById("downloadTemplateButton");
  if (downloadBtn) {
    downloadBtn.addEventListener("click", generateTemplateFile);
  }
}
