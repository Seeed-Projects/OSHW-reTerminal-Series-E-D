let expandedPlatformId = null;
let selectedPlatform = null;
let selectedDevice = null;
let selectedFirmwareOption = null;
let selectedVersion = null;
let monitorPort = null;
let monitorReader = null;
let monitorKeepReading = false;
let firmwareVersions = {};
const monitorDecoder = new TextDecoder();
const DEFAULT_FIRMWARE_VERSION = "latest";

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
  return DEVICES.find((device) => device.id === deviceId) || null;
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

function getInstallManifest() {
  if (!selectedPlatform?.installReady || !selectedFirmwareOption) return "";
  const version = selectedVersion?.version || DEFAULT_FIRMWARE_VERSION;
  return `firmware/${selectedFirmwareOption.id}/${version}/manifest.json`;
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

async function loadFirmwareVersions() {
  try {
    const response = await fetch("firmware/versions.json");
    if (!response.ok) throw new Error(`Version manifest fetch failed: ${response.status}`);
    firmwareVersions = normalizeFirmwareVersions(await response.json());
  } catch (_) {
    firmwareVersions = {};
  }

  if (selectedPlatform) {
    selectedVersion = getDefaultVersion(selectedPlatform);
    renderVersionPanel();
    updateFlashState();
  }
}

function getFirmwareVersionValues(firmwareOption) {
  if (!firmwareOption?.id) return [DEFAULT_FIRMWARE_VERSION];
  const versions = firmwareVersions[firmwareOption.id];
  return Array.isArray(versions) && versions.length
    ? versions
    : [DEFAULT_FIRMWARE_VERSION];
}

function getVersionOptions(platform = selectedPlatform) {
  if (platform?.installReady && selectedFirmwareOption) {
    return getFirmwareVersionValues(selectedFirmwareOption).map((version) => ({
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
    if (target.closest("button, a, input, select, textarea")) return;
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
  const firmwareDescription =
    selectedFirmwareOption?.description || selectedPlatform.description;

  container.innerHTML = `
    <div class="selected-copy">
      <div class="selected-tags">
        <span class="tag tag-platform">${selectedPlatform.name}</span>
        <span class="tag tag-device">${selectedDevice.name}</span>
      </div>
      <h3>${firmwareName}</h3>
      <p>${firmwareDescription}</p>
      <div class="compat-list">
        ${renderDeviceSpecs(selectedDevice, "compat-badge active")}
      </div>
    </div>
    <div class="selected-device-photo">
      <img src="${selectedDevice.image}" alt="${selectedDevice.imageAlt}">
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

  const versions = getVersionOptions(selectedPlatform);
  if (!versions.some((item) => item.version === selectedVersion?.version)) {
    selectedVersion = versions[0] || null;
  }

  versionSelect.innerHTML = versions.map((item) => {
    const label = item.label && item.label !== item.version
      ? `${item.version} - ${item.label}`
      : item.version;
    const selectedAttr = item.version === selectedVersion?.version ? "selected" : "";
    return `<option value="${item.version}" ${selectedAttr}>${label}</option>`;
  }).join("");
}

function renderConfigArea() {
  const container = document.getElementById("configArea");
  if (!container || !selectedPlatform) return;

  const notes = selectedFirmwareOption?.notes || [];
  const platformFields = selectedPlatform?.configFields || [];
  const fwFields = selectedFirmwareOption?.configFields || [];
  const allFields = [...platformFields, ...fwFields];
  const notesHtml = notes.map((n) => `
    <div class="alert alert-${n.type === "warning" ? "warning" : "info"} is-visible">
      <span>${n.text}</span>
    </div>
  `).join("");

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
}

function collectConfigValues() {
  const platformFields = selectedPlatform?.configFields || [];
  const fwFields = selectedFirmwareOption?.configFields || [];
  const allFields = [...platformFields, ...fwFields];
  if (!allFields.length) return [];
  return allFields.map((field) => {
    const el = document.getElementById(field.id);
    if (!el) return null;
    let value;
    if (field.type === "checkbox") value = el.checked ? 1 : 0;
    else if (field.type === "select" || field.type === "number") value = field.nvsType === "float" ? parseFloat(el.value) : parseInt(el.value, 10);
    else value = el.value || field.defaultValue || "";
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
}

let eraseBeforeFlash = false;
let isFlashing = false;
let lastFlashedPort = null;

function updateFlashState() {
  const flashBtn = document.getElementById("flashButton");
  const disabledBtn = document.getElementById("disabledFlashButton");
  const installNote = document.getElementById("installNote");
  const manifest = getInstallManifest();
  const ready = Boolean(selectedPlatform?.installReady && manifest);

  if (flashBtn) flashBtn.classList.toggle("is-hidden", !ready);
  if (disabledBtn) disabledBtn.classList.toggle("is-hidden", ready);
  if (installNote) installNote.classList.toggle("is-visible", !ready);
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
    await esploader.writeFlash({
      fileArray,
      flashSize: "keep",
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
    await port.open({ baudRate: 115200 });
    monitorPort = port;
    monitorKeepReading = true;
    setMonitorControls(true);
    setSerialState("connected", "Monitor connected");
    appendLog("[monitor] Auto-connected at 115200 baud.");
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
      selectedVersion = getDefaultVersion(selectedPlatform);
      renderSelectedRelease();
      renderVersionPanel();
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
    modeStandard.addEventListener("click", () => {
      modeStandard.classList.add("is-active");
      modeErase.classList.remove("is-active");
      eraseBeforeFlash = false;
      appendLog("[system] Flash mode: standard");
    });
    modeErase.addEventListener("click", () => {
      modeErase.classList.add("is-active");
      modeStandard.classList.remove("is-active");
      eraseBeforeFlash = true;
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

  const flashBtn = document.getElementById("flashButton");
  if (flashBtn) {
    flashBtn.addEventListener("click", flashDevice);
    flashBtn.disabled = !("serial" in navigator);
  }
}
