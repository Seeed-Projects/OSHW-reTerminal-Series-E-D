let expandedPlatformId = null;
let selectedPlatform = null;
let selectedDevice = null;
let selectedFirmwareOption = null;
let selectedVersion = null;
let monitorPort = null;
let monitorReader = null;
let monitorKeepReading = false;
const monitorDecoder = new TextDecoder();

document.addEventListener("DOMContentLoaded", () => {
  checkBrowser();
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
  return DEVICES.find((device) => device.id === deviceId) || null;
}

function getDefaultVersion(platform) {
  return platform?.versions?.[0] || null;
}

function getAvailableFirmwareOptions(platform, deviceId) {
  if (!platform?.firmwareOptions?.length || !deviceId) return [];
  return platform.firmwareOptions.filter((firmware) =>
    firmware.compatible.includes(deviceId)
  );
}

function getInstallManifest() {
  if (!selectedPlatform?.installReady) return "";
  return selectedFirmwareOption?.manifest || selectedVersion?.manifest || "";
}

function renderDeviceSpecs(device, className = "") {
  if (!device?.specs?.length) return "";
  const classAttr = className ? ` class="${className}"` : "";
  return device.specs.map((spec) => `<span${classAttr}>${spec}</span>`).join("");
}

function renderPlatformCards() {
  const container = document.getElementById("platformGrid");
  if (!container) return;

  container.innerHTML = PLATFORM_CARDS.map((platform) => {
    const isExpanded = platform.id === expandedPlatformId;
    const devices = platform.supportedDevices
      .map((deviceId) => {
        const device = getDevice(deviceId);
        if (!device) return "";
        return `
          <button class="device-option" data-platform="${platform.id}" data-device="${device.id}" type="button">
            <span class="device-image">
              <img src="${device.image}" alt="${device.imageAlt}">
            </span>
            <span class="device-copy">
              <strong>${device.name}</strong>
              <span>${device.description}</span>
              <span class="device-specs">${renderDeviceSpecs(device)}</span>
            </span>
          </button>
        `;
      })
      .join("");
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
              <p class="eyebrow">Supported devices</p>
              <h3>Select device type</h3>
            </div>
            <div class="device-options">${devices}</div>
          </div>
        </div>
      </article>
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
    if (target.closest("button, a, input, select, textarea, esp-web-install-button")) return;
    collapsePlatformCards();
  });
}

// Selects the platform-device pair that drives the remaining setup flow.
// 选择平台和设备组合，用来驱动后续配置流程。
function selectPlatformDevice(platformId, deviceId) {
  selectedPlatform =
    PLATFORM_CARDS.find((platform) => platform.id === platformId) || null;
  selectedDevice = getDevice(deviceId);
  const availableOptions = getAvailableFirmwareOptions(selectedPlatform, deviceId);
  selectedFirmwareOption = availableOptions[0] || null;
  selectedVersion = getDefaultVersion(selectedPlatform);

  renderSelectedRelease();
  renderSetupPanel();
  renderConfigArea();
  updateFlashState();
  renderFlowState();
  resetProgress();
  appendLog(
    `[system] Selected platform: ${selectedPlatform?.name || "None"} / ${selectedDevice?.name || "None"}`
  );
}

function clearPlatformSelection() {
  selectedPlatform = null;
  selectedDevice = null;
  selectedFirmwareOption = null;
  selectedVersion = null;
  renderPlatformCards();
  renderFlowState();
  resetProgress();
}

function renderFlowState() {
  const hasSelection = Boolean(selectedPlatform && selectedDevice);
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
  if (!container || !selectedPlatform || !selectedDevice) return;

  const firmwareName = selectedFirmwareOption?.name || selectedPlatform.name;

  container.innerHTML = `
    <div class="selected-icon">
      <img src="${selectedPlatform.logo}" alt="${selectedPlatform.name} logo">
    </div>
    <div class="selected-copy">
      <div class="selected-tags">
        <span class="tag tag-platform">${selectedPlatform.name}</span>
        <span class="tag tag-device">${selectedDevice.name}</span>
      </div>
      <h3>${firmwareName}</h3>
      <p>${selectedPlatform.description}</p>
      <div class="compat-list">
        ${renderDeviceSpecs(selectedDevice, "compat-badge active")}
      </div>
    </div>
  `;
}

function renderSetupPanel() {
  renderFirmwareSelect();
  renderVersionPanel();
}

function renderFirmwareSelect() {
  const field = document.getElementById("firmwareField");
  const select = document.getElementById("firmwareSelect");
  if (!field || !select || !selectedPlatform || !selectedDevice) return;

  const options = getAvailableFirmwareOptions(selectedPlatform, selectedDevice.id);
  field.classList.toggle("is-hidden", options.length === 0);
  if (!options.length) {
    select.innerHTML = "";
    return;
  }

  select.innerHTML = options.map((firmware) => {
    const selectedAttr =
      firmware.id === selectedFirmwareOption?.id ? "selected" : "";
    return `<option value="${firmware.id}" ${selectedAttr}>${firmware.name}</option>`;
  }).join("");
}

function renderVersionPanel() {
  const versionSelect = document.getElementById("versionSelect");
  if (!versionSelect || !selectedPlatform) return;

  versionSelect.innerHTML = selectedPlatform.versions.map((item) => {
    const label = `${item.version} - ${item.label}`;
    const selectedAttr = item.version === selectedVersion?.version ? "selected" : "";
    return `<option value="${item.version}" ${selectedAttr}>${label}</option>`;
  }).join("");
}

function renderConfigArea() {
  const container = document.getElementById("configArea");
  if (!container || !selectedPlatform) return;

  const notes = selectedFirmwareOption?.notes || [];
  const notesHtml = notes.map((n) => `
    <div class="alert alert-${n.type === "warning" ? "warning" : "info"} is-visible">
      <span>${n.text}</span>
    </div>
  `).join("");

  if (!selectedPlatform.configFields.length) {
    container.innerHTML = notesHtml || `
      <div class="config-empty">
        <strong>No setup fields required</strong>
        <span>This platform can continue without pre-configuring network or API settings.</span>
      </div>
    `;
    return;
  }

  container.innerHTML = notesHtml + selectedPlatform.configFields.map((field) => `
    <label class="field-block" for="${field.id}">
      <span>${field.label}</span>
      <input id="${field.id}" type="${field.type}" placeholder="${field.placeholder || ""}">
    </label>
  `).join("");
}

function updateFlashState() {
  const espBtn = document.getElementById("espFlashButton");
  const disabledBtn = document.getElementById("disabledFlashButton");
  const installNote = document.getElementById("installNote");
  const manifest = getInstallManifest();
  const ready = Boolean(selectedPlatform?.installReady && manifest);

  if (espBtn) {
    espBtn.classList.toggle("is-hidden", !ready);
    if (manifest) espBtn.setAttribute("manifest", manifest);
  }
  if (disabledBtn) disabledBtn.classList.toggle("is-hidden", ready);
  if (installNote) installNote.classList.toggle("is-visible", !ready);
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
  const log = document.getElementById("log");
  if (!log) return;
  log.textContent += `\n${message}`;
  log.scrollTop = log.scrollHeight;
}

function appendSerialChunk(message) {
  const log = document.getElementById("log");
  if (!log) return;
  log.textContent += message;
  log.scrollTop = log.scrollHeight;
}

function setSerialState(state, label) {
  const el = document.getElementById("serialStatus");
  if (!el) return;
  el.className = `serial-status state-${state}`;
  el.innerHTML = `<i></i><b>${label}</b>`;
}

function setMonitorControls(connected) {
  const connectBtn = document.getElementById("connectMonitorButton");
  const disconnectBtn = document.getElementById("disconnectMonitorButton");
  if (connectBtn) connectBtn.classList.toggle("is-hidden", connected);
  if (disconnectBtn) disconnectBtn.classList.toggle("is-hidden", !connected);
}

async function connectMonitor() {
  if (!("serial" in navigator)) {
    setSerialState("error", "Unavailable");
    appendLog("[monitor] Web Serial is unavailable in this browser.");
    return;
  }

  try {
    monitorPort = await navigator.serial.requestPort();
    await monitorPort.open({ baudRate: 115200 });
    monitorKeepReading = true;
    setMonitorControls(true);
    setSerialState("connected", "Monitor connected");
    appendLog("[monitor] Connected at 115200 baud.");
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
    try {
      while (monitorKeepReading) {
        const { value, done } = await reader.read();
        if (done) break;
        if (value) {
          appendSerialChunk(monitorDecoder.decode(value, { stream: true }));
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

function bindFlowEvents() {
  const changeBtn = document.getElementById("changePlatformButton");
  if (changeBtn) {
    changeBtn.addEventListener("click", clearPlatformSelection);
  }

  const firmwareSelect = document.getElementById("firmwareSelect");
  if (firmwareSelect) {
    firmwareSelect.addEventListener("change", () => {
      const options = getAvailableFirmwareOptions(selectedPlatform, selectedDevice?.id);
      selectedFirmwareOption =
        options.find((item) => item.id === firmwareSelect.value) || options[0] || null;
      renderSelectedRelease();
      renderConfigArea();
      updateFlashState();
      resetProgress();
      appendLog(`[system] Selected demo: ${selectedFirmwareOption?.name || "None"}`);
    });
  }

  const versionSelect = document.getElementById("versionSelect");
  if (versionSelect) {
    versionSelect.addEventListener("change", () => {
      selectedVersion =
        selectedPlatform?.versions.find((item) => item.version === versionSelect.value) ||
        getDefaultVersion(selectedPlatform);
      updateFlashState();
      resetProgress();
      appendLog(`[system] Selected version: ${selectedVersion?.version || "None"}`);
    });
  }

  const modeStandard = document.getElementById("modeStandard");
  const modeErase = document.getElementById("modeErase");
  const espBtn = document.getElementById("espFlashButton");
  if (modeStandard && modeErase && espBtn) {
    modeStandard.addEventListener("click", () => {
      modeStandard.classList.add("is-active");
      modeErase.classList.remove("is-active");
      espBtn.removeAttribute("erase-first");
      appendLog("[system] Flash mode: standard");
    });
    modeErase.addEventListener("click", () => {
      modeErase.classList.add("is-active");
      modeStandard.classList.remove("is-active");
      espBtn.setAttribute("erase-first", "");
      appendLog("[system] Flash mode: erase + flash");
    });
  }
}

function bindWorkspaceEvents() {
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
      appendLog("[flash] Connecting to device...");
      setProgress("flash", 5, "Initializing connection");
    });

    espBtn.addEventListener("preparing", () => {
      appendLog("[flash] Preparing firmware...");
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
      appendLog("[flash] Firmware installed successfully!");
      setProgress("flash", 100, "Firmware installed successfully");
    });

    espBtn.addEventListener("error", (e) => {
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
