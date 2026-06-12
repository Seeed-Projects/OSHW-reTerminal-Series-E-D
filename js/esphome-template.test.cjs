const assert = require("node:assert/strict");
const { PLATFORM_CARDS } = require("./firmwares.js");
const {
  buildEsphomeTemplateContent,
  expandEsphomeTemplateOptionIds,
} = require("./esphome-template.js");

const esphome = PLATFORM_CARDS.find((platform) => platform.id === "esphome");
assert.ok(esphome, "ESPHome platform exists");
assert.deepEqual(esphome.supportedDevices, ["E1001", "E1002"]);

const allOptionIds = esphome.templateOptions.map((option) => option.id);

function countTopLevelKey(yaml, key) {
  const matches = yaml.match(new RegExp(`^${key}:`, "gm"));
  return matches ? matches.length : 0;
}

function assertNoDuplicateTopLevelKeys(yaml) {
  [
    "esphome",
    "esp32",
    "logger",
    "font",
    "spi",
    "i2c",
    "i2s_audio",
    "output",
    "sensor",
    "binary_sensor",
    "light",
    "time",
    "api",
    "ota",
    "wifi",
    "captive_portal",
    "microphone",
    "display",
    "deep_sleep",
  ].forEach((key) => {
    assert.ok(countTopLevelKey(yaml, key) <= 1, `${key} appears more than once`);
  });
}

const e1001All = buildEsphomeTemplateContent(esphome, allOptionIds, "E1001");
assert.equal(countTopLevelKey(e1001All, "output"), 1);
assert.equal(countTopLevelKey(e1001All, "sensor"), 1);
assert.equal(countTopLevelKey(e1001All, "binary_sensor"), 1);
assert.equal(countTopLevelKey(e1001All, "light"), 1);
assert.equal(countTopLevelKey(e1001All, "time"), 1);
assert.match(e1001All, /platform: waveshare_epaper/);
assert.match(e1001All, /model: 7\.50inv2/);
assert.match(e1001All, /run_duration: 30s/);
assert.match(e1001All, /it\.printf\(30, 120, id\(font_large\), "Temp: %\.1f C"/);
assert.match(e1001All, /ESPHome 不能像 Arduino 那样直接操作 SD 卡文件系统/);
assert.match(e1001All, /# --- SHT4x temperature & humidity \(I2C\) ---/);
assert.match(e1001All, /# Turn on SD card power rail \/ 打开 SD 卡供电\n      - output\.turn_on: bsp_sd_enable/);
assert.match(e1001All, /^  ssid: !secret wifi_ssid$/m);
assert.match(e1001All, /^  password: !secret wifi_password$/m);
assert.doesNotMatch(e1001All, /id\(font_large\), RED, "Temp:/);
assertNoDuplicateTopLevelKeys(e1001All);

const e1001AllWithWifiValues = buildEsphomeTemplateContent(
  esphome,
  allOptionIds,
  "E1001",
  { wifiSsid: "Office WiFi", wifiPassword: "p@ss:word #1" }
);
assert.match(e1001AllWithWifiValues, /^  ssid: "Office WiFi"$/m);
assert.match(e1001AllWithWifiValues, /^  password: "p@ss:word #1"$/m);
assert.doesNotMatch(e1001AllWithWifiValues, /^  ssid: !secret wifi_ssid$/m);
assert.doesNotMatch(e1001AllWithWifiValues, /^  password: !secret wifi_password$/m);
assertNoDuplicateTopLevelKeys(e1001AllWithWifiValues);

const e1001AllWithEmptyWifiValues = buildEsphomeTemplateContent(
  esphome,
  allOptionIds,
  "E1001",
  { wifiSsid: "", wifiPassword: "" }
);
assert.match(e1001AllWithEmptyWifiValues, /^  ssid: !secret wifi_ssid$/m);
assert.match(e1001AllWithEmptyWifiValues, /^  password: !secret wifi_password$/m);
assertNoDuplicateTopLevelKeys(e1001AllWithEmptyWifiValues);

const e1002All = buildEsphomeTemplateContent(esphome, allOptionIds, "E1002");
assert.match(e1002All, /platform: epaper_spi/);
assert.match(e1002All, /model: Seeed-reTerminal-E1002/);
assert.match(e1002All, /run_duration: 150s/);
assert.match(e1002All, /const auto RED\s+= Color/);
assert.match(e1002All, /id\(font_large\), RED, "Temp: %\.1f C"/);
assert.match(e1002All, /it\.line\(20, 55, 780, 55, BLACK\)/);
assert.match(e1002All, /需要 ESPHome >= 2025\.11\.1 才能使用 epaper_spi 平台/);
assert.match(e1002All, /\/\/ ---- Title \(BLUE\) \/ 标题（蓝色）----/);
assertNoDuplicateTopLevelKeys(e1002All);

const buttonClosure = expandEsphomeTemplateOptionIds(esphome, ["buttons"]);
assert.deepEqual(buttonClosure, ["buttons", "buzzer_led"]);
const buttonsOnly = buildEsphomeTemplateContent(esphome, ["buttons"], "E1001");
assert.match(buttonsOnly, /id: onboard_led/);
assert.match(buttonsOnly, /id: buzzer/);
assert.match(buttonsOnly, /id: buzzer_pwm/);
assert.doesNotMatch(buttonsOnly, /ESPHome 中 SD 卡的限制/);
assertNoDuplicateTopLevelKeys(buttonsOnly);

const tempWithoutDisplay = buildEsphomeTemplateContent(esphome, ["temp_humidity"], "E1001");
assert.doesNotMatch(tempWithoutDisplay, /display:/m);
assert.doesNotMatch(tempWithoutDisplay, /id\(temp_sensor\)\./);
assert.doesNotMatch(tempWithoutDisplay, /Temp: %\.1f C/);
assertNoDuplicateTopLevelKeys(tempWithoutDisplay);

console.log("esphome-template tests passed");
