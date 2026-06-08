const assert = require("node:assert/strict");
const { createLogBuffer } = require("./log-buffer.js");

const lineBuffer = createLogBuffer({ maxLength: 64, trimNotice: "[trimmed]\n" });
lineBuffer.append("first", false);
lineBuffer.append("second\n", true);
assert.equal(lineBuffer.text(), "first\nsecond\n");

const cappedBuffer = createLogBuffer({ maxLength: 80, trimNotice: "[trimmed]\n" });
for (let index = 0; index < 5000; index += 1) {
  cappedBuffer.append(`line ${index.toString().padStart(3, "0")}\n`);
}
assert.ok(cappedBuffer.text().length <= 80);
assert.ok(cappedBuffer.text().startsWith("[trimmed]\n"));
assert.ok(cappedBuffer.text().includes("line 4999"));
assert.ok(!cappedBuffer.text().includes("line 000"));

cappedBuffer.clear();
assert.equal(cappedBuffer.text(), "");

cappedBuffer.seed("x".repeat(200));
assert.ok(cappedBuffer.text().length <= 80);
assert.ok(cappedBuffer.text().startsWith("[trimmed]\n"));

console.log("log-buffer tests passed");
