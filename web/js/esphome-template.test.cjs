const assert = require("node:assert/strict");
const { PLATFORM_CARDS } = require("./firmwares.js");
const {
  buildEsphomeTemplateContent,
  expandEsphomeTemplateOptionIds,
} = require("./esphome-template.js");

const esphome = PLATFORM_CARDS.find((platform) => platform.id === "esphome");
assert.ok(esphome, "ESPHome platform exists");
assert.deepEqual(esphome.supportedDevices, ["E1001", "E1002", "E1003"]);

const allOptionIds = esphome.templateOptions.map((option) => option.id);

function countTopLevelKey(yaml, key) {
  const matches = yaml.match(new RegExp(`^${key}:`, "gm"));
  return matches ? matches.length : 0;
}

function assertNoDuplicateTopLevelKeys(yaml) {
  [
    "esphome",
    "esp32",
    "psram",
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
    "touchscreen",
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
assert.doesNotMatch(e1001All, /^psram:$/m);
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
assert.doesNotMatch(e1002All, /^psram:$/m);
assertNoDuplicateTopLevelKeys(e1002All);

const e1003All = buildEsphomeTemplateContent(esphome, allOptionIds, "E1003");
assert.match(e1003All, /^  board: seeed_xiao_esp32s3$/m);
assert.match(e1003All, /^    type: esp-idf$/m);
assert.match(e1003All, /^    priority: -100$/m);
assert.match(e1003All, /platform: it8951/);
assert.match(e1003All, /model: seeed-reterminal-e1003/);
assert.match(e1003All, /Requires ESPHome >= 2026\.7\.0/);
assert.match(e1003All, /^psram:\n  mode: octal$/m);
assert.doesNotMatch(e1003All, /^  speed: 80MHz$/m);
assert.doesNotMatch(e1003All, /^external_components:/m);
assert.match(e1003All, /^    spi_id: epaper_spi_bus$/m);
assert.match(e1003All, /^    update_interval: never$/m);
assert.match(e1003All, /^    grayscale: true$/m);
assert.match(e1003All, /^    dithering: true$/m);
assert.match(e1003All, /^    update_mode: GC16$/m);
assert.match(e1003All, /- delay: 2s\n      - it8951\.update:\n          id: epaper_display\n          mode: GC16/);
assert.match(e1003All, /pin: GPIO16\n    id: bsp_led/);
assert.match(e1003All, /pin: GPIO39\n    id: bsp_sd_enable/);
assert.match(e1003All, /pin: GPIO45\n    id: buzzer_pwm/);
assert.match(e1003All, /i2s_lrclk_pin: GPIO42/);
assert.match(e1003All, /i2s_din_pin: GPIO41/);
assert.match(e1003All, /pin: GPIO38\n    id: mic_power_enable/);
assert.match(e1003All, /pin: GPIO40\n    id: bsp_battery_enable/);
assert.match(e1003All, /run_duration: 90s/);
assert.match(
  e1003All,
  /number: GPIO3\n      allow_other_uses: true\n      mode: INPUT\n      inverted: true\n    id: button_1/
);
assert.match(e1003All, /number: GPIO4\n      mode: INPUT\n      inverted: true\n    id: button_2/);
assert.match(e1003All, /number: GPIO5\n      mode: INPUT\n      inverted: true\n    id: button_3/);
assert.match(e1003All, /name: "KEY1 Middle Button"/);
assert.match(e1003All, /esp32_ext1_wakeup:\n    pins:\n      - number: GPIO3/);
assert.match(e1003All, /it\.printf\(100, 300, id\(font_large\), Color::BLACK, "Temp: %\.1f C"/);
assert.match(e1003All, /platform: sht4x\n    id: sht4x_sensor/);
assert.match(e1003All, /- component\.update: sht4x_sensor/);
assert.match(
  e1003All,
  /- wait_until:\n          condition:\n            lambda: return id\(temp_sensor\)\.has_state\(\) && id\(hum_sensor\)\.has_state\(\);\n          timeout: 5s/
);
assert.match(e1003All, /platform: gt911/);
assert.match(e1003All, /interrupt_pin: GPIO2/);
assert.match(e1003All, /reset_pin: GPIO48/);
assert.match(e1003All, /format: "Touch: x=%d, y=%d"/);
assert.match(e1003All, /args: \["touch\.x", "touch\.y"\]/);
assert.doesNotMatch(e1003All, /pin: GPIO6\n    id: bsp_led/);
assert.doesNotMatch(e1003All, /pin: GPIO16\n    id: bsp_sd_enable/);
assert.doesNotMatch(e1003All, /pin: GPIO21\n    id: bsp_battery_enable/);
assertNoDuplicateTopLevelKeys(e1003All);

const e1003DashboardOnly = buildEsphomeTemplateContent(
  esphome,
  ["wifi_ota", "ha_api", "display"],
  "E1003"
);
assert.match(e1003DashboardOnly, /^  board: seeed_xiao_esp32s3$/m);
assert.match(e1003DashboardOnly, /^    type: esp-idf$/m);
assert.match(e1003DashboardOnly, /^psram:\n  mode: octal$/m);
assert.match(e1003DashboardOnly, /^  id: epaper_spi_bus$/m);
assert.match(e1003DashboardOnly, /^api:\n  encryption:\n    key: !secret api_encryption_key$/m);
assert.match(e1003DashboardOnly, /^ota:\n  - platform: esphome\n    password: !secret ota_password$/m);
assert.match(e1003DashboardOnly, /^    spi_id: epaper_spi_bus$/m);
assert.match(e1003DashboardOnly, /^    model: seeed-reterminal-e1003$/m);
assert.match(e1003DashboardOnly, /^    update_interval: never$/m);
assert.match(e1003DashboardOnly, /id\(font_small\)/);
assert.match(e1003DashboardOnly, /it\.fill\(Color::WHITE\);/);
assert.match(e1003DashboardOnly, /id\(font_large\), Color::BLACK, TextAlign::TOP_CENTER/);
assert.match(e1003DashboardOnly, /it\.line\(70, 120, 1802, 120, Color::BLACK\);/);
assert.match(e1003DashboardOnly, /id\(font_small\), Color::BLACK, TextAlign::TOP_CENTER/);
assert.match(e1003DashboardOnly, /- delay: 2s\n      - it8951\.update:\n          id: epaper_display\n          mode: GC16/);
assert.doesNotMatch(e1003DashboardOnly, /^output:$/m);
assert.doesNotMatch(e1003DashboardOnly, /^sensor:$/m);
assert.doesNotMatch(e1003DashboardOnly, /^touchscreen:$/m);
assertNoDuplicateTopLevelKeys(e1003DashboardOnly);

const buttonClosure = expandEsphomeTemplateOptionIds(esphome, ["buttons"]);
assert.deepEqual(buttonClosure, ["buttons"]);
const apiClosure = expandEsphomeTemplateOptionIds(esphome, ["ha_api"]);
assert.deepEqual(apiClosure, ["wifi_ota", "ha_api"]);
const rtcClosure = expandEsphomeTemplateOptionIds(esphome, ["rtc"]);
assert.deepEqual(rtcClosure, ["wifi_ota", "ha_api", "rtc"]);
const buttonsOnly = buildEsphomeTemplateContent(esphome, ["buttons"], "E1001");
assert.match(buttonsOnly, /id: button_1/);
assert.match(buttonsOnly, /id: button_2/);
assert.match(buttonsOnly, /id: button_3/);
assert.doesNotMatch(buttonsOnly, /id: onboard_led/);
assert.doesNotMatch(buttonsOnly, /id: buzzer/);
assert.doesNotMatch(buttonsOnly, /id: buzzer_pwm/);
assert.doesNotMatch(buttonsOnly, /ESPHome 中 SD 卡的限制/);
assertNoDuplicateTopLevelKeys(buttonsOnly);

const e1003ButtonsOnly = buildEsphomeTemplateContent(esphome, ["buttons"], "E1003");
assert.match(e1003ButtonsOnly, /number: GPIO3[\s\S]*id: button_1/);
assert.match(e1003ButtonsOnly, /number: GPIO4[\s\S]*id: button_2/);
assert.match(e1003ButtonsOnly, /number: GPIO5[\s\S]*id: button_3/);
assert.doesNotMatch(e1003ButtonsOnly, /id: bsp_led/);
assert.doesNotMatch(e1003ButtonsOnly, /id: buzzer_pwm/);
assert.doesNotMatch(e1003ButtonsOnly, /allow_other_uses/);
assertNoDuplicateTopLevelKeys(e1003ButtonsOnly);

const e1003ButtonsWithDeepSleep = buildEsphomeTemplateContent(
  esphome,
  ["buttons", "deep_sleep"],
  "E1003"
);
assert.equal((e1003ButtonsWithDeepSleep.match(/allow_other_uses: true/g) || []).length, 2);
assert.match(
  e1003ButtonsWithDeepSleep,
  /number: GPIO3\n      allow_other_uses: true\n      mode: INPUT/
);
assert.match(
  e1003ButtonsWithDeepSleep,
  /number: GPIO3\n        allow_other_uses: true\n        mode: INPUT_PULLUP/
);

const e1003SingleOptionExpectations = {
  wifi_ota: [/^ota:/m, /^wifi:/m, /^captive_portal:/m],
  ha_api: [/^api:/m],
  display: [/^spi:/m, /^display:/m, /model: seeed-reterminal-e1003/],
  buttons: [/^binary_sensor:/m, /id: button_1/, /id: button_2/, /id: button_3/],
  buzzer_led: [
    /^output:/m,
    /^light:/m,
    /pin: GPIO16\n    id: bsp_led/,
    /pin: GPIO45\n    id: buzzer_pwm/,
    /- light\.turn_on: onboard_led/,
    /id: buzzer\n          brightness: 50%/,
    /- delay: 250ms/,
    /- light\.turn_off: buzzer/,
    /- light\.turn_off: onboard_led/,
  ],
  battery: [/^output:/m, /^sensor:/m, /id: bsp_battery_enable/, /id: battery_level/],
  temp_humidity: [/^i2c:/m, /^sensor:/m, /platform: sht4x/],
  rtc: [/^i2c:/m, /^time:/m, /^api:/m, /platform: pcf8563/],
  microphone: [/^i2s_audio:/m, /^output:/m, /^microphone:/m, /id: onboard_mic/],
  touchscreen: [/^spi:/m, /^i2c:/m, /^display:/m, /^touchscreen:/m, /platform: gt911/],
  sd_detect: [/^spi:/m, /^output:/m, /^binary_sensor:/m, /id: sd_card_detect/],
  deep_sleep: [/^deep_sleep:/m, /number: GPIO3/],
};

Object.entries(e1003SingleOptionExpectations).forEach(([optionId, patterns]) => {
  const yaml = buildEsphomeTemplateContent(esphome, [optionId], "E1003");
  patterns.forEach((pattern) => assert.match(yaml, pattern, `${optionId} is missing ${pattern}`));
  assertNoDuplicateTopLevelKeys(yaml);
});

assert.doesNotMatch(
  buildEsphomeTemplateContent(esphome, ["buttons"], "E1003"),
  /^light:|id: bsp_led|id: buzzer_pwm/m
);
assert.doesNotMatch(
  buildEsphomeTemplateContent(esphome, ["buzzer_led"], "E1003"),
  /^binary_sensor:|id: button_[123]/m
);

let testedCombinationCount = 0;
for (const deviceId of esphome.supportedDevices) {
  const supportedOptionIds = esphome.templateOptions
    .filter((option) => !option.supportedDevices || option.supportedDevices.includes(deviceId))
    .map((option) => option.id);
  const combinationCount = 2 ** supportedOptionIds.length;

  for (let mask = 0; mask < combinationCount; mask += 1) {
    const selectedIds = supportedOptionIds.filter((_, index) => mask & (1 << index));
    const yaml = buildEsphomeTemplateContent(esphome, selectedIds, deviceId);
    assert.doesNotMatch(yaml, /\{\{\w+\}\}/, `${deviceId} mask ${mask} has unresolved tokens`);
    assertNoDuplicateTopLevelKeys(yaml);
    testedCombinationCount += 1;
  }
}
assert.equal(testedCombinationCount, 8192);

const tempWithoutDisplay = buildEsphomeTemplateContent(esphome, ["temp_humidity"], "E1001");
assert.match(tempWithoutDisplay, /platform: sht4x\n    id: sht4x_sensor/);
assert.doesNotMatch(tempWithoutDisplay, /- component\.update: sht4x_sensor/);
assert.doesNotMatch(tempWithoutDisplay, /- wait_until:/);
assert.doesNotMatch(tempWithoutDisplay, /display:/m);
assert.doesNotMatch(tempWithoutDisplay, /id\(temp_sensor\)\./);
assert.doesNotMatch(tempWithoutDisplay, /Temp: %\.1f C/);
assertNoDuplicateTopLevelKeys(tempWithoutDisplay);

const e1003DisplayWithTemp = buildEsphomeTemplateContent(
  esphome,
  ["display", "temp_humidity"],
  "E1003"
);
const temperatureUpdateIndex = e1003DisplayWithTemp.indexOf("component.update: sht4x_sensor");
const temperatureWaitIndex = e1003DisplayWithTemp.indexOf("wait_until:");
const displayDelayIndex = e1003DisplayWithTemp.indexOf("delay: 2s");
const displayUpdateIndex = e1003DisplayWithTemp.indexOf("it8951.update:");
assert.ok(temperatureUpdateIndex >= 0, "E1003 triggers the first SHT4x reading on boot");
assert.ok(
  temperatureUpdateIndex < temperatureWaitIndex,
  "E1003 starts the SHT4x reading before waiting for sensor data"
);
assert.ok(temperatureWaitIndex < displayDelayIndex, "E1003 waits for sensor data before display delay");
assert.ok(displayDelayIndex < displayUpdateIndex, "E1003 refreshes after the display delay");

[
  ["display", "temp_humidity", "rtc"],
  ["display", "temp_humidity", "touchscreen"],
  allOptionIds,
].forEach((selectedIds) => {
  const yaml = buildEsphomeTemplateContent(esphome, selectedIds, "E1003");
  const sensorUpdateIndex = yaml.indexOf("component.update: sht4x_sensor");
  const sensorWaitIndex = yaml.indexOf("wait_until:");
  const screenUpdateIndex = yaml.indexOf("it8951.update:");
  assert.ok(sensorUpdateIndex >= 0, `${selectedIds.join("+")} triggers the SHT4x reading`);
  assert.ok(sensorUpdateIndex < sensorWaitIndex, `${selectedIds.join("+")} waits after triggering SHT4x`);
  assert.ok(sensorWaitIndex < screenUpdateIndex, `${selectedIds.join("+")} waits before refreshing E1003`);
  assertNoDuplicateTopLevelKeys(yaml);
});

console.log(`esphome-template tests passed (${testedCombinationCount} combinations)`);
