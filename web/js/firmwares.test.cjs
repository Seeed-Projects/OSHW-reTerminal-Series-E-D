const assert = require("node:assert/strict");
const { PLATFORM_CARDS } = require("./firmwares.js");

const SUPPORTED_INSTALL_MODES = new Set(["standard", "erase"]);

const firmwareOptions = PLATFORM_CARDS.flatMap((platform) => platform.firmwareOptions || []);
for (const option of firmwareOptions) {
  if (!option.recommendedInstallMode) continue;
  assert.ok(
    SUPPORTED_INSTALL_MODES.has(option.recommendedInstallMode),
    `${option.id} has an unsupported recommendedInstallMode`
  );
}

const officialWikiUrls = new Map([
  ["esphome", "https://wiki.seeedstudio.com/epaper_work_with_esphome/"],
  ["trmnl", "https://wiki.seeedstudio.com/reterminal_e10xx_trmnl/"],
  ["eezstudio", "https://wiki.seeedstudio.com/reterminal_e10xx_with_eezstudio/"],
  ["lvgl-epaper-status-panel", "https://wiki.seeedstudio.com/epaper_work_with_lvgl"],
  ["squareline", "https://wiki.seeedstudio.com/reterminal_e10xx_with_squareline_vision/"],
  ["opendisplay", "https://wiki.seeedstudio.com/EN04_opendisplay/"],
]);

for (const [platformId, wikiUrl] of officialWikiUrls) {
  const platform = PLATFORM_CARDS.find((item) => item.id === platformId);
  assert.ok(platform, `${platformId} platform is registered`);
  assert.equal(platform.wiki?.url, wikiUrl, `${platformId} wiki URL is registered`);
}

const zephyrPlatform = PLATFORM_CARDS.find((platform) => platform.id === "zephyr");
assert.ok(zephyrPlatform, "Zephyr platform is registered");
assert.equal(zephyrPlatform.group, "official");
assert.equal(zephyrPlatform.installReady, false);
assert.deepEqual(zephyrPlatform.supportedDevices, ["E1001", "E1002", "E1003"]);
assert.equal(zephyrPlatform.externalTool?.stepTitle, "Official docs");
assert.equal(zephyrPlatform.externalTool?.label, "Open Zephyr Board Docs");
assert.deepEqual(
  zephyrPlatform.externalTool?.urlsByDevice,
  {
    E1001: "https://docs.zephyrproject.org/latest/boards/seeed/reterminal_e1001/doc/index.html",
    E1002: "https://docs.zephyrproject.org/latest/boards/seeed/reterminal_e1002/doc/index.html",
    E1003: "https://docs.zephyrproject.org/latest/boards/seeed/reterminal_e1003/doc/index.html",
  }
);
assert.deepEqual(zephyrPlatform.versions, []);
assert.deepEqual(zephyrPlatform.configFields, []);
assert.deepEqual(zephyrPlatform.firmwareOptions, []);

const lvglStatusPanel = PLATFORM_CARDS.find((platform) =>
  platform.id === "lvgl-epaper-status-panel"
);
assert.ok(lvglStatusPanel, "LVGL ePaper Status Panel platform is registered");
assert.equal(lvglStatusPanel.group, "official");
assert.equal(lvglStatusPanel.installReady, true);
assert.deepEqual(
  lvglStatusPanel.supportedDevices,
  ["E1001", "E1002", "E1003", "E1004"]
);
assert.deepEqual(lvglStatusPanel.detailTags, ["LVGL 9.5.0"]);
assert.deepEqual(
  lvglStatusPanel.firmwareOptions.map((option) => option.id),
  [
    "LVGL_StatusPanel_E1001",
    "LVGL_StatusPanel_E1002",
    "LVGL_StatusPanel_E1003",
    "LVGL_StatusPanel_E1004",
  ]
);

const photoframeOptions = firmwareOptions.filter((option) =>
  option.id.startsWith("PhotoFrame_")
);

assert.deepEqual(
  photoframeOptions.map((option) => option.id).sort(),
  ["PhotoFrame_reTerminal_E1002", "PhotoFrame_reTerminal_E1004"]
);

for (const option of photoframeOptions) {
  assert.equal(option.recommendedInstallMode, "erase");
  assert.ok(
    option.flashNotes.some((note) => note.text.includes("full-chip image at 0x0")),
    `${option.id} includes full-chip flashing guidance in flashNotes`
  );
  assert.ok(
    !option.notes.some((note) => note.text.includes("full-chip image at 0x0")),
    `${option.id} keeps flashing guidance out of setup notes`
  );
}

console.log("firmwares metadata tests passed");
