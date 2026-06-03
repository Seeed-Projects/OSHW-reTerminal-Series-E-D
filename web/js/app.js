let activeDevice = "all";
let selectedFirmware = FIRMWARES[0];

document.addEventListener("DOMContentLoaded", () => {
  checkBrowser();
  renderDeviceChips();
  renderCatalog();
  renderSelectedRelease();
  bindWorkspaceEvents();
});

function checkBrowser() {
  if (!("serial" in navigator)) {
    const el = document.getElementById("browserWarning");
    if (el) el.classList.add("is-visible");
  }
}

// Count firmware items compatible with each device
// 统计每个设备可用的固件数量
function countForDevice(deviceId) {
  if (deviceId === "all") return FIRMWARES.length;
  return FIRMWARES.filter((fw) => fw.compatible.includes(deviceId)).length;
}

function renderDeviceChips() {
  const container = document.getElementById("deviceChips");
  if (!container) return;

  container.innerHTML = DEVICES.map(
    (d) => `
    <button
      class="device-chip ${d.id === activeDevice ? "is-active" : ""}"
      data-device="${d.id}"
    >
      ${d.name}
      <span class="device-count">${countForDevice(d.id)}</span>
    </button>
  `
  ).join("");

  container.querySelectorAll(".device-chip").forEach((chip) => {
    chip.addEventListener("click", () => {
      activeDevice = chip.dataset.device;
      renderDeviceChips();
      renderCatalog();
    });
  });
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

  const visible =
    activeDevice === "all"
      ? FIRMWARES
      : FIRMWARES.filter((fw) => fw.compatible.includes(activeDevice));

  container.innerHTML = visible.map((fw) => {
    const isSelected = fw.id === selectedFirmware?.id;
    const compatBadges = fw.compatible
      .map(
        (id) =>
          `<span class="compat-badge ${id === activeDevice ? "active" : ""}">${id}</span>`
      )
      .join("");

    return `
      <article class="firmware-card ${isSelected ? "is-selected" : ""}">
        <div class="card-preview">
          <div class="card-preview-placeholder">${getCardIcon(fw.icon)}</div>
        </div>
        <div class="card-content">
          <div class="card-top">
            <span class="tag ${getTagClass(fw.category)}">${fw.category}</span>
          </div>
          <h3>${fw.name}</h3>
          <p>${fw.tagline}</p>
          <div class="card-foot">
            <div class="compat-list">${compatBadges}</div>
            <a href="${fw.sourceUrl}" target="_blank" rel="noreferrer">Source ↗</a>
            <button class="button button-small ${isSelected ? "button-selected" : ""}" data-firmware="${fw.id}" type="button">
              ${isSelected ? "Selected" : "Select"}
            </button>
          </div>
        </div>
      </article>
    `;
  }).join("");

  container.querySelectorAll("[data-firmware]").forEach((btn) => {
    btn.addEventListener("click", () => {
      selectedFirmware = FIRMWARES.find((fw) => fw.id === btn.dataset.firmware);
      renderCatalog();
      renderSelectedRelease();
      resetProgress();
      const workspace = document.querySelector(".workspace");
      if (workspace) workspace.scrollIntoView({ behavior: "smooth", block: "start" });
    });
  });
}

function renderSelectedRelease() {
  const container = document.getElementById("selectedRelease");
  if (!container || !selectedFirmware) return;

  container.innerHTML = `
    <div>
      <span class="tag ${getTagClass(selectedFirmware.category)}">${selectedFirmware.category}</span>
      <h3>${selectedFirmware.name}</h3>
      <p>${selectedFirmware.tagline}</p>
    </div>
  `;

  const espBtn = document.getElementById("espFlashButton");
  if (espBtn) espBtn.setAttribute("manifest", selectedFirmware.manifest);
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

function bindWorkspaceEvents() {
  const clearBtn = document.getElementById("clearLogButton");
  if (clearBtn) {
    clearBtn.addEventListener("click", () => {
      const log = document.getElementById("log");
      if (log) log.textContent = "";
    });
  }

  // Listen for ESP Web Tools events
  // 监听 ESP Web Tools 烧录事件
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
