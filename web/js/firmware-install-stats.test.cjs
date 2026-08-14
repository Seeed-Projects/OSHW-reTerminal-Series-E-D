const assert = require("node:assert/strict");
const { readFileSync } = require("node:fs");
const {
  formatInstallCount,
  createFirmwareStatsId,
  supportsFirmwareInstallStats,
  createFirmwareInstallStatsClient,
} = require("./firmware-install-stats.js");

assert.equal(formatInstallCount(0), "0");
assert.equal(formatInstallCount(999), "999");
assert.equal(formatInstallCount(1200), "1.2K");
assert.equal(formatInstallCount(1999999), "1.9M");

assert.equal(createFirmwareStatsId("TRMNL", "E1003"), "hub-trmnl-e1003");
assert.equal(
  createFirmwareStatsId("LVGL ePaper Status Panel", "EE04"),
  "hub-lvgl-epaper-status-panel-ee04"
);
assert.equal(createFirmwareStatsId("", "E1003"), "");

assert.equal(
  supportsFirmwareInstallStats({ installReady: true }, { flashMethod: "serial" }),
  true
);
assert.equal(
  supportsFirmwareInstallStats({ installReady: true }, { flashMethod: "uf2" }),
  true
);
assert.equal(
  supportsFirmwareInstallStats({ installReady: false }, { flashMethod: "serial" }),
  false
);

const requests = [];
const storageValues = new Map();
const valueElement = { textContent: "0" };
const labelElement = { textContent: "installs" };
const classes = new Set();
const counter = {
  dataset: { firmwareId: "hub-trmnl-e1003" },
  classList: { add(value) { classes.add(value); } },
  setAttribute(name, value) { this[name] = value; },
  querySelector(selector) {
    return selector === "[data-firmware-install-value]" ? valueElement : labelElement;
  },
};

const client = createFirmwareInstallStatsClient({
  apiUrl: "https://stats.example.com/",
  cryptoImpl: { randomUUID: () => "12345678-1234-1234-1234-123456789abc" },
  storage: {
    getItem(key) { return storageValues.get(key) || null; },
    setItem(key, value) { storageValues.set(key, value); },
  },
  fetchImpl: async (url, options = {}) => {
    requests.push({ url, options });
    if (url.includes("/v1/install/start")) {
      return new Response(JSON.stringify({ token: "signed-token" }), { status: 201 });
    }
    if (url.includes("/v1/install/complete")) {
      return new Response(JSON.stringify({ counted: true, totalInstalls: 1 }), { status: 200 });
    }
    return new Response(JSON.stringify({ stats: { "hub-trmnl-e1003": 1200 } }), { status: 200 });
  },
});

(async () => {
  const token = await client.beginInstall({
    firmwareId: "hub-trmnl-e1003",
    version: "1.8.10",
  });
  assert.equal(token, "signed-token");
  assert.equal(requests[0].url, "https://stats.example.com/v1/install/start");
  assert.deepEqual(JSON.parse(requests[0].options.body), {
    firmwareId: "hub-trmnl-e1003",
    version: "1.8.10",
    visitorId: "visitor_12345678123412341234123456789abc",
  });

  const completion = await client.completeInstall(token);
  assert.equal(completion.counted, true);

  await client.loadCounts({ querySelectorAll: () => [counter] });
  assert.equal(valueElement.textContent, "1.2K");
  assert.equal(labelElement.textContent, "installs");
  assert.equal(counter["aria-hidden"], "false");
  assert.ok(classes.has("is-visible"));

  const appSource = readFileSync(new URL("./app.js", `file://${__dirname}/`), "utf8");
  assert.match(
    appSource,
    /querySelector\("\.hardware-card\.is-expanded"\)[\s\S]*loadRenderedFirmwareInstallCounts\(expandedCard\)/
  );
  const flashStart = appSource.indexOf("async function flashDevice()");
  const flashEnd = appSource.indexOf("async function autoConnectMonitor", flashStart);
  const flashSource = appSource.slice(flashStart, flashEnd);
  assert.ok(flashSource.indexOf("await esploader.flashId()") >= 0);
  assert.ok(
    flashSource.indexOf("await esploader.flashId()")
      < flashSource.indexOf("beginCurrentInstallStats()")
  );
  assert.ok(
    flashSource.indexOf("await esploader.writeFlash")
      < flashSource.indexOf("completeCurrentInstallStats(installSessionPromise)")
  );

  const uf2Start = appSource.indexOf("async function writeUf2ToDrive()");
  const uf2End = appSource.indexOf("function renderDownloadGuide", uf2Start);
  const uf2Source = appSource.slice(uf2Start, uf2End);
  assert.ok(
    uf2Source.indexOf("await writable.close()")
      < uf2Source.indexOf("completeCurrentInstallStats(installSessionPromise)")
  );

  const downloadListenerStart = appSource.indexOf("if (uf2DownloadBtn)");
  const downloadListenerEnd = appSource.indexOf("const uf2WriteBtn", downloadListenerStart);
  assert.ok(!appSource.slice(downloadListenerStart, downloadListenerEnd).includes("InstallStats"));

  console.log("firmware-install-stats tests passed");
})().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
