const assert = require("node:assert/strict");
const { PLATFORM_CARDS } = require("./firmwares.js");
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
  hasRouteParams: true,
});

assert.deepEqual(parseHubRoute("/OSHW-reTerminal-Series-E-D/"), {
  platformId: "",
  deviceId: "",
  hasRouteParams: false,
});

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
assert.equal(
  resolveHubRouteSelection(parseHubRoute("/?platform=trmnl&device=E1004"), PLATFORM_CARDS),
  null
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

assert.equal(
  buildHubRouteUrl(
    "/OSHW-reTerminal-Series-E-D/?source=share&platform=esphome&device=E1004#workspace",
    null
  ),
  "/OSHW-reTerminal-Series-E-D/?source=share#workspace"
);

console.log("route state tests passed");
