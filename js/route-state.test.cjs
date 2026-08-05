const assert = require("node:assert/strict");
const { PLATFORM_CARDS, getCompatiblePanels } = require("./firmwares.js");
const {
  parseHubRoute,
  resolveHubRouteSelection,
  buildHubRouteUrl,
} = require("./route-state.js");

const productionUrl =
  "https://seeed-projects.github.io/OSHW-reTerminal-Series-E-D/?platform=esphome&device=E1004";
assert.deepEqual(parseHubRoute(productionUrl), {
  platformId: "esphome",
  deviceId: "E1004",
  firmwareId: "",
  version: "",
  panelId: "",
  hasRouteParams: true,
});

assert.deepEqual(parseHubRoute("/OSHW-reTerminal-Series-E-D/"), {
  platformId: "",
  deviceId: "",
  firmwareId: "",
  version: "",
  panelId: "",
  hasRouteParams: false,
});

assert.deepEqual(
  parseHubRoute(
    "/?platform=base&device=EE04&firmware=XIAO_EPaper_Hello&version=latest&panel=P073_SP6"
  ),
  {
    platformId: "base",
    deviceId: "EE04",
    firmwareId: "XIAO_EPaper_Hello",
    version: "latest",
    panelId: "P073_SP6",
    hasRouteParams: true,
  }
);

assert.deepEqual(
  resolveHubRouteSelection(
    parseHubRoute("/?platform=ESPHOME&device=e1004"),
    PLATFORM_CARDS
  ),
  { platformId: "esphome", deviceId: "E1004" }
);
assert.equal(
  resolveHubRouteSelection(parseHubRoute("/?platform=esphome&device=E9999"), PLATFORM_CARDS),
  null
);
assert.deepEqual(
  resolveHubRouteSelection(parseHubRoute("/?platform=trmnl&device=E1004"), PLATFORM_CARDS),
  { platformId: "trmnl", deviceId: "E1004" }
);
assert.equal(
  resolveHubRouteSelection(parseHubRoute("/?platform=esphome"), PLATFORM_CARDS),
  null
);

for (const platform of PLATFORM_CARDS) {
  for (const deviceId of platform.supportedDevices || []) {
    const url = buildHubRouteUrl("/OSHW-reTerminal-Series-E-D/?source=share#workspace", {
      platformId: platform.id,
      deviceId,
    });
    assert.equal(
      url,
      `/OSHW-reTerminal-Series-E-D/?source=share&platform=${encodeURIComponent(platform.id)}&device=${encodeURIComponent(deviceId)}#workspace`
    );
    assert.deepEqual(
      resolveHubRouteSelection(parseHubRoute(url), PLATFORM_CARDS),
      { platformId: platform.id, deviceId }
    );
  }
}

let firmwareRouteCount = 0;
let panelRouteCount = 0;
for (const platform of PLATFORM_CARDS) {
  for (const firmware of platform.firmwareOptions || []) {
    for (const deviceId of firmware.compatible || []) {
      const selection = {
        platformId: platform.id,
        deviceId,
        firmwareId: firmware.id,
        version: firmware.defaultVersion || "latest",
        panelId: "",
      };
      const route = parseHubRoute(buildHubRouteUrl("/hub/", selection));
      assert.deepEqual(resolveHubRouteSelection(route, PLATFORM_CARDS), {
        platformId: platform.id,
        deviceId,
      });
      assert.equal(route.firmwareId, firmware.id);
      assert.equal(route.version, selection.version);
      firmwareRouteCount += 1;

      if (!firmware.comboPattern) continue;
      for (const panel of getCompatiblePanels(deviceId, firmware)) {
        const panelRoute = parseHubRoute(
          buildHubRouteUrl("/hub/", { ...selection, panelId: panel.id })
        );
        assert.equal(panelRoute.panelId, panel.id);
        panelRouteCount += 1;
      }
    }
  }
}
assert.ok(firmwareRouteCount > 0);
assert.ok(panelRouteCount > 0);

assert.equal(
  buildHubRouteUrl(
    "/OSHW-reTerminal-Series-E-D/?source=share&platform=base&device=EE04&firmware=XIAO_EPaper_Hello&version=latest&panel=P073_SP6#workspace",
    null
  ),
  "/OSHW-reTerminal-Series-E-D/?source=share#workspace"
);

assert.equal(
  buildHubRouteUrl("/OSHW-reTerminal-Series-E-D/?source=share#workspace", {
    platformId: "base",
    deviceId: "EE04",
    firmwareId: "XIAO_EPaper_Hello",
    version: "latest",
    panelId: "P073_SP6",
  }),
  "/OSHW-reTerminal-Series-E-D/?source=share&platform=base&device=EE04&firmware=XIAO_EPaper_Hello&version=latest&panel=P073_SP6#workspace"
);

console.log(
  `route state tests passed (${firmwareRouteCount} firmware routes, ${panelRouteCount} panel routes)`
);
