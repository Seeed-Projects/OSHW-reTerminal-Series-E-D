let activePlatform = "all";
let activeFunction = "all";
let activeDevice = "all";
let selectedFirmware = null;
let selectedVersion = null;

document.addEventListener("DOMContentLoaded", () => {
  checkBrowser();
  renderAllFilters();
  renderCatalog();
  renderFlowState();
  bindFlowEvents();
  bindWorkspaceEvents();
});

function checkBrowser() {
  if (!("serial" in navigator)) {
    const el = document.getElementById("browserWarning");
    if (el) el.classList.add("is-visible");
  }
}

function getPlatformName(platformId) {
  return PLATFORMS.find((platform) => platform.id === platformId)?.name || platformId;
}

function getDefaultVersion(firmware) {
  return firmware?.versions?.[0] || null;
}

function firmwareMatchesFilters(firmware) {
  const platformMatches =
    activePlatform === "all" || firmware.platform === activePlatform;
  const functionMatches =
    activeFunction === "all" || firmware.category === activeFunction;
  const deviceMatches =
    activeDevice === "all" || firmware.compatible.includes(activeDevice);

  return platformMatches && functionMatches && deviceMatches;
}

function countForPlatform(platformId) {
  return FIRMWARES.filter((firmware) => {
    const platformMatches = platformId === "all" || firmware.platform === platformId;
    const functionMatches =
      activeFunction === "all" || firmware.category === activeFunction;
    const deviceMatches =
      activeDevice === "all" || firmware.compatible.includes(activeDevice);
    return platformMatches && functionMatches && deviceMatches;
  }).length;
}

function countForFunction(functionId) {
  return FIRMWARES.filter((firmware) => {
    const platformMatches =
      activePlatform === "all" || firmware.platform === activePlatform;
    const functionMatches = functionId === "all" || firmware.category === functionId;
    const deviceMatches =
      activeDevice === "all" || firmware.compatible.includes(activeDevice);
    return platformMatches && functionMatches && deviceMatches;
  }).length;
}

// Count firmware items compatible with each device.
// 统计每个设备可用的固件数量。
function countForDevice(deviceId) {
  return FIRMWARES.filter((firmware) => {
    const platformMatches =
      activePlatform === "all" || firmware.platform === activePlatform;
    const functionMatches =
      activeFunction === "all" || firmware.category === activeFunction;
    const deviceMatches = deviceId === "all" || firmware.compatible.includes(deviceId);
    return platformMatches && functionMatches && deviceMatches;
  }).length;
}

function renderFilterChips(containerId, items, activeId, countFn, onSelect) {
  const container = document.getElementById(containerId);
  if (!container) return;

  container.innerHTML = items.map((item) => {
    const count = countFn(item.id);
    const activeClass = item.id === activeId ? "is-active" : "";
    const disabledAttr = count === 0 && item.id !== activeId ? "disabled" : "";

    return `
      <button
        class="filter-chip ${activeClass}"
        data-filter="${item.id}"
        type="button"
        ${disabledAttr}
      >
        ${item.name}
        <span class="filter-count">${count}</span>
      </button>
    `;
  }).join("");

  container.querySelectorAll(".filter-chip").forEach((chip) => {
    chip.addEventListener("click", () => onSelect(chip.dataset.filter));
  });
}

function renderAllFilters() {
  renderFilterChips(
    "platformChips",
    PLATFORMS,
    activePlatform,
    countForPlatform,
    (id) => {
      activePlatform = id;
      renderAllFilters();
      renderCatalog();
    }
  );

  renderFilterChips(
    "functionChips",
    FUNCTION_GROUPS,
    activeFunction,
    countForFunction,
    (id) => {
      activeFunction = id;
      renderAllFilters();
      renderCatalog();
    }
  );

  renderFilterChips(
    "deviceChips",
    DEVICES,
    activeDevice,
    countForDevice,
    (id) => {
      activeDevice = id;
      renderAllFilters();
      renderCatalog();
    }
  );
}

function getCardIcon(icon) {
  const icons = {
    clock: `<svg width="36" height="36" viewBox="0 0 36 36" fill="none">
      <circle cx="18" cy="18" r="13" stroke="#a1a1a6" stroke-width="1.5"/>
      <path d="M18 10v8l5 3" stroke="#a1a1a6" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
    </svg>`,
    sleep: `<svg width="36" height="36" viewBox="0 0 36 36" fill="none">
      <path d="M24 18a8 8 0 11-8-8c0 4.4 3.6 8 8 8z" stroke="#a1a1a6" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
      <path d="M23 8h4l-4 4h4" stroke="#a1a1a6" stroke-width="1.2" stroke-linecap="round" stroke-linejoin="round"/>
    </svg>`,
    mic: `<svg width="36" height="36" viewBox="0 0 36 36" fill="none">
      <rect x="14" y="7" width="8" height="14" rx="4" stroke="#a1a1a6" stroke-width="1.5"/>
      <path d="M10 19a8 8 0 0016 0" stroke="#a1a1a6" stroke-width="1.5" stroke-linecap="round"/>
      <path d="M18 27v3M15 30h6" stroke="#a1a1a6" stroke-width="1.5" stroke-linecap="round"/>
    </svg>`,
    touch: `<svg width="36" height="36" viewBox="0 0 36 36" fill="none">
      <rect x="8" y="5" width="20" height="26" rx="2" stroke="#a1a1a6" stroke-width="1.5"/>
      <circle cx="18" cy="18" r="3" stroke="#a1a1a6" stroke-width="1.5"/>
      <circle cx="18" cy="18" r="1" fill="#a1a1a6"/>
    </svg>`,
  };
  return icons[icon] || icons.clock;
}

function getTagClass(category) {
  return CATEGORY_COLORS[category] || "";
}

function renderCatalog() {
  const container = document.getElementById("catalog");
  if (!container) return;

  const visible = FIRMWARES.filter(firmwareMatchesFilters);

  if (visible.length === 0) {
    container.innerHTML = `
      <div class="empty-state">
        <h3>No firmware yet</h3>
        <p>This category is reserved for future firmware packages.</p>
      </div>
    `;
    return;
  }

  container.innerHTML = visible.map((firmware) => {
    const compatBadges = firmware.compatible
      .map(
        (id) =>
          `<span class="compat-badge ${id === activeDevice ? "active" : ""}">${id}</span>`
      )
      .join("");

    return `
      <article class="firmware-card">
        <div class="card-preview">
          <div class="card-preview-placeholder">${getCardIcon(firmware.icon)}</div>
        </div>
        <div class="card-content">
          <div class="card-top">
            <span class="tag tag-platform">${getPlatformName(firmware.platform)}</span>
            <span class="tag ${getTagClass(firmware.category)}">${firmware.category}</span>
          </div>
          <h3>${firmware.name}</h3>
          <p>${firmware.tagline}</p>
          <div class="card-foot">
            <div class="compat-list">${compatBadges}</div>
            <a href="${firmware.sourceUrl}" target="_blank" rel="noreferrer">Source</a>
            <button class="button button-small" data-firmware="${firmware.id}" type="button">
              Select
            </button>
          </div>
        </div>
      </article>
    `;
  }).join("");

  container.querySelectorAll("[data-firmware]").forEach((btn) => {
    btn.addEventListener("click", () => selectFirmware(btn.dataset.firmware));
  });
}

function selectFirmware(firmwareId) {
  selectedFirmware = FIRMWARES.find((firmware) => firmware.id === firmwareId) || null;
  selectedVersion = getDefaultVersion(selectedFirmware);
  renderSelectedRelease();
  renderVersionPanel();
  renderConfigArea();
  updateFlashManifest();
  renderFlowState();
  resetProgress();
  appendLog(`[system] Selected firmware: ${selectedFirmware?.name || "None"}`);
}

function clearFirmwareSelection() {
  selectedFirmware = null;
  selectedVersion = null;
  renderFlowState();
  resetProgress();
}

function renderFlowState() {
  const hasSelection = Boolean(selectedFirmware);
  const selectionPanel = document.getElementById("selectionPanel");
  const selectedPanel = document.getElementById("selectedPanel");
  const versionPanel = document.getElementById("versionPanel");
  const flashPanel = document.getElementById("flashPanel");

  document.body.classList.toggle("has-selection", hasSelection);
  if (selectionPanel) selectionPanel.classList.toggle("is-collapsed", hasSelection);
  toggleStepPanel(selectedPanel, hasSelection, 0);
  toggleStepPanel(versionPanel, hasSelection, 90);
  toggleStepPanel(flashPanel, hasSelection, 180);
}

function toggleStepPanel(panel, visible, delayMs) {
  if (!panel) return;
  panel.classList.toggle("is-hidden", !visible);
  panel.classList.toggle("is-visible", visible);
  panel.style.setProperty("--step-delay", `${delayMs}ms`);
}

function renderSelectedRelease() {
  const container = document.getElementById("selectedRelease");
  if (!container || !selectedFirmware) return;

  const compatBadges = selectedFirmware.compatible
    .map((id) => `<span class="compat-badge">${id}</span>`)
    .join("");

  container.innerHTML = `
    <div class="selected-icon">${getCardIcon(selectedFirmware.icon)}</div>
    <div class="selected-copy">
      <div class="selected-tags">
        <span class="tag tag-platform">${getPlatformName(selectedFirmware.platform)}</span>
        <span class="tag ${getTagClass(selectedFirmware.category)}">${selectedFirmware.category}</span>
      </div>
      <h3>${selectedFirmware.name}</h3>
      <p>${selectedFirmware.tagline}</p>
      <div class="compat-list">${compatBadges}</div>
    </div>
  `;
}

function renderVersionPanel() {
  const versionSelect = document.getElementById("versionSelect");
  if (!versionSelect || !selectedFirmware) return;

  versionSelect.innerHTML = selectedFirmware.versions.map((item) => {
    const label = `${item.version} - ${item.label}`;
    const selectedAttr = item.version === selectedVersion?.version ? "selected" : "";
    return `<option value="${item.version}" ${selectedAttr}>${label}</option>`;
  }).join("");
}

function renderConfigArea() {
  const container = document.getElementById("configArea");
  if (!container || !selectedFirmware) return;

  if (!selectedFirmware.configFields.length) {
    container.innerHTML = `
      <div class="config-empty">
        <strong>No setup fields required</strong>
        <span>This firmware can be flashed without pre-configuring network or API settings.</span>
      </div>
    `;
    return;
  }

  container.innerHTML = selectedFirmware.configFields.map((field) => `
    <label class="field-block" for="${field.id}">
      <span>${field.label}</span>
      <input id="${field.id}" type="${field.type}" placeholder="${field.placeholder || ""}">
    </label>
  `).join("");
}

function updateFlashManifest() {
  const espBtn = document.getElementById("espFlashButton");
  if (!espBtn || !selectedVersion) return;
  espBtn.setAttribute("manifest", selectedVersion.manifest);
}

function resetProgress() {
  setProgress("flash", 0, "Select firmware and click Flash to begin");
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
  const log = document.getElementById("log");
  if (!log) return;
  log.textContent += `\n${message}`;
  log.scrollTop = log.scrollHeight;
}

function setSerialState(state, label) {
  const el = document.getElementById("serialStatus");
  if (!el) return;
  el.className = `serial-status state-${state}`;
  el.innerHTML = `<i></i><b>${label}</b>`;
}

function bindFlowEvents() {
  const changeBtn = document.getElementById("changeFirmwareButton");
  if (changeBtn) {
    changeBtn.addEventListener("click", clearFirmwareSelection);
  }

  const versionSelect = document.getElementById("versionSelect");
  if (versionSelect) {
    versionSelect.addEventListener("change", () => {
      selectedVersion =
        selectedFirmware?.versions.find((item) => item.version === versionSelect.value) ||
        getDefaultVersion(selectedFirmware);
      updateFlashManifest();
      resetProgress();
      appendLog(`[system] Selected version: ${selectedVersion?.version || "None"}`);
    });
  }
}

function bindWorkspaceEvents() {
  const clearBtn = document.getElementById("clearLogButton");
  if (clearBtn) {
    clearBtn.addEventListener("click", () => {
      const log = document.getElementById("log");
      if (log) log.textContent = "";
    });
  }

  // Listen for ESP Web Tools events.
  // 监听 ESP Web Tools 烧录事件。
  const espBtn = document.getElementById("espFlashButton");
  if (espBtn) {
    espBtn.addEventListener("initializing", () => {
      setSerialState("connecting", "Connecting");
      appendLog("[system] Connecting to device...");
      setProgress("flash", 5, "Initializing connection");
    });

    espBtn.addEventListener("preparing", () => {
      setSerialState("connecting", "Preparing");
      appendLog("[system] Preparing firmware...");
      setProgress("flash", 15, "Preparing firmware");
    });

    espBtn.addEventListener("erasing", () => {
      appendLog("[flash] Erasing flash...");
      setProgress("flash", 30, "Erasing flash memory");
    });

    espBtn.addEventListener("writing", () => {
      appendLog("[flash] Writing firmware...");
      setProgress("flash", 50, "Writing firmware to device");
    });

    espBtn.addEventListener("finished", () => {
      setSerialState("success", "Flash complete");
      appendLog("[system] Firmware installed successfully!");
      setProgress("flash", 100, "Firmware installed successfully");
    });

    espBtn.addEventListener("error", (e) => {
      setSerialState("error", "Error");
      appendLog(`[error] ${e.detail?.message || "Flash failed"}`);
      setProgress("flash", 0, "Flash failed");
      const alert = document.getElementById("errorAlert");
      const msg = document.getElementById("errorMessage");
      if (alert && msg) {
        msg.textContent = e.detail?.message || "An error occurred during flashing.";
        alert.classList.add("is-visible");
      }
    });
  }
}
