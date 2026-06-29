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
