(function () {
  const PLATFORM_PARAM = "platform";
  const DEVICE_PARAM = "device";
  const DEFAULT_BASE_URL = "http://localhost/";

  function parseHubRoute(urlValue, baseUrl = DEFAULT_BASE_URL) {
    const url = new URL(String(urlValue), baseUrl);
    return {
      platformId: (url.searchParams.get(PLATFORM_PARAM) || "").trim(),
      deviceId: (url.searchParams.get(DEVICE_PARAM) || "").trim(),
      hasRouteParams:
        url.searchParams.has(PLATFORM_PARAM) || url.searchParams.has(DEVICE_PARAM),
    };
  }

  // Resolves URL values to the canonical ids registered by the firmware catalog.
  // 将 URL 参数解析为固件目录中登记的标准 id。
  function resolveHubRouteSelection(route, platforms) {
    if (!route?.platformId || !route?.deviceId || !Array.isArray(platforms)) {
      return null;
    }

    const platformId = route.platformId.toLowerCase();
    const platform = platforms.find((item) =>
      String(item.id).toLowerCase() === platformId
    );
    if (!platform || !Array.isArray(platform.supportedDevices)) return null;

    const deviceId = route.deviceId.toLowerCase();
    const canonicalDeviceId = platform.supportedDevices.find((id) =>
      String(id).toLowerCase() === deviceId
    );
    if (!canonicalDeviceId) return null;

    return {
      platformId: platform.id,
      deviceId: canonicalDeviceId,
    };
  }

  // Builds a same-page URL while preserving unrelated query parameters and hash state.
  // 构建同页 URL，同时保留其他查询参数和哈希状态。
  function buildHubRouteUrl(urlValue, selection, baseUrl = DEFAULT_BASE_URL) {
    const url = new URL(String(urlValue), baseUrl);
    if (selection?.platformId && selection?.deviceId) {
      url.searchParams.set(PLATFORM_PARAM, selection.platformId);
      url.searchParams.set(DEVICE_PARAM, selection.deviceId);
    } else {
      url.searchParams.delete(PLATFORM_PARAM);
      url.searchParams.delete(DEVICE_PARAM);
    }
    return `${url.pathname}${url.search}${url.hash}`;
  }

  globalThis.parseHubRoute = parseHubRoute;
  globalThis.resolveHubRouteSelection = resolveHubRouteSelection;
  globalThis.buildHubRouteUrl = buildHubRouteUrl;

  if (typeof module !== "undefined") {
    module.exports = {
      parseHubRoute,
      resolveHubRouteSelection,
      buildHubRouteUrl,
    };
  }
})();
