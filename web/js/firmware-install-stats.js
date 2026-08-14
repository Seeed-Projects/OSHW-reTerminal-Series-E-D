(function () {
  const VISITOR_STORAGE_KEY = "firmware-hub-install-visitor";

  function formatInstallCount(value) {
    const count = Math.max(0, Math.floor(Number(value) || 0));
    if (count < 1000) return String(count);

    const divisor = count < 1000000 ? 1000 : 1000000;
    const suffix = count < 1000000 ? "K" : "M";
    const truncated = Math.floor((count / divisor) * 10) / 10;
    return `${Number.isInteger(truncated) ? truncated.toFixed(0) : truncated.toFixed(1)}${suffix}`;
  }

  function normalizeStatsIdPart(value) {
    return String(value || "")
      .trim()
      .toLowerCase()
      .replace(/[^a-z0-9]+/g, "-")
      .replace(/^-+|-+$/g, "");
  }

  // Builds one stable counter key for a platform card under a specific device.
  // 为指定设备下的平台卡片生成稳定的统计编号。
  function createFirmwareStatsId(platformId, deviceId) {
    const platformPart = normalizeStatsIdPart(platformId);
    const devicePart = normalizeStatsIdPart(deviceId);
    if (!platformPart || !devicePart) return "";
    return `hub-${platformPart}-${devicePart}`.slice(0, 63).replace(/-+$/g, "");
  }

  function supportsFirmwareInstallStats(platform, hardware) {
    const flashMethod = hardware?.flashMethod || "serial";
    return Boolean(
      platform?.installReady
      && (flashMethod === "serial" || flashMethod === "uf2")
    );
  }

  function createVisitorId(cryptoImpl) {
    return `visitor_${cryptoImpl.randomUUID().replace(/-/g, "")}`;
  }

  function getVisitorId(storage, cryptoImpl) {
    try {
      const existing = storage?.getItem(VISITOR_STORAGE_KEY);
      if (existing) return existing;
      const visitorId = createVisitorId(cryptoImpl);
      storage?.setItem(VISITOR_STORAGE_KEY, visitorId);
      return visitorId;
    } catch (_) {
      return createVisitorId(cryptoImpl);
    }
  }

  async function readJson(response) {
    const data = await response.json().catch(() => ({}));
    if (!response.ok) {
      throw new Error(data.error || `Request failed with HTTP ${response.status}`);
    }
    return data;
  }

  function createFirmwareInstallStatsClient(options = {}) {
    const endpoint = String(options.apiUrl || "").replace(/\/+$/, "");
    const fetchImpl = options.fetchImpl || globalThis.fetch;
    const storage = options.storage || globalThis.localStorage;
    const cryptoImpl = options.cryptoImpl || globalThis.crypto;
    const logger = options.logger || globalThis.console;

    async function beginInstall({ firmwareId, version }) {
      if (!endpoint) return null;
      try {
        const response = await fetchImpl(`${endpoint}/v1/install/start`, {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({
            firmwareId,
            version,
            visitorId: getVisitorId(storage, cryptoImpl),
          }),
          keepalive: true,
        });
        const data = await readJson(response);
        return data.token || null;
      } catch (error) {
        logger?.warn?.("Firmware install tracking could not start", error);
        return null;
      }
    }

    async function completeInstall(token) {
      if (!endpoint || !token) return null;
      try {
        const response = await fetchImpl(`${endpoint}/v1/install/complete`, {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ token }),
          keepalive: true,
        });
        return readJson(response);
      } catch (error) {
        logger?.warn?.("Firmware install tracking could not complete", error);
        return null;
      }
    }

    async function loadCounts(root = globalThis.document) {
      if (!endpoint || !root) return {};
      const elements = Array.from(root.querySelectorAll("[data-firmware-install-count]"));
      const ids = Array.from(new Set(
        elements.map((element) => element.dataset.firmwareId).filter(Boolean)
      ));
      if (ids.length === 0) return {};

      try {
        // Loads all counters rendered for the expanded device with one request.
        // 用一个请求加载当前展开设备中显示的全部计数。
        const query = new URLSearchParams({ ids: ids.join(",") });
        query.set("ts", String(Date.now()));
        const response = await fetchImpl(`${endpoint}/v1/stats?${query}`, {
          headers: { Accept: "application/json" },
          cache: "no-store",
        });
        const data = await readJson(response);
        const stats = data.stats || {};

        for (const element of elements) {
          const count = Math.max(0, Number(stats[element.dataset.firmwareId]) || 0);
          const valueElement = element.querySelector("[data-firmware-install-value]");
          const labelElement = element.querySelector("[data-firmware-install-label]");
          if (valueElement) valueElement.textContent = formatInstallCount(count);
          if (labelElement) labelElement.textContent = count === 1 ? "install" : "installs";
          element.setAttribute("aria-hidden", "false");
          element.classList.add("is-visible");
        }
        return stats;
      } catch (error) {
        logger?.warn?.("Firmware install counts could not load", error);
        elements.forEach((element) => element.classList.add("is-unavailable"));
        return {};
      }
    }

    return { beginInstall, completeInstall, loadCounts };
  }

  globalThis.formatInstallCount = formatInstallCount;
  globalThis.createFirmwareStatsId = createFirmwareStatsId;
  globalThis.supportsFirmwareInstallStats = supportsFirmwareInstallStats;
  globalThis.createFirmwareInstallStatsClient = createFirmwareInstallStatsClient;

  if (typeof module !== "undefined") {
    module.exports = {
      formatInstallCount,
      createFirmwareStatsId,
      supportsFirmwareInstallStats,
      createFirmwareInstallStatsClient,
    };
  }
})();
