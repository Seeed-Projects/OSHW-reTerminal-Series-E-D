const assert = require("node:assert/strict");
const { formatSerialPortLabel } = require("./serial-port-label.js");

assert.equal(formatSerialPortLabel(null), "Unavailable");
assert.equal(formatSerialPortLabel({}), "Serial port");
assert.equal(
  formatSerialPortLabel({ getInfo: () => ({ usbVendorId: 0x303a, usbProductId: 0x1001 }) }),
  "Espressif USB 303A:1001"
);
assert.equal(
  formatSerialPortLabel({ getInfo: () => ({ usbVendorId: 0x10c4, usbProductId: 0xea60 }) }),
  "Silicon Labs USB 10C4:EA60"
);
assert.equal(
  formatSerialPortLabel({ getInfo: () => ({ usbVendorId: 0x9999, usbProductId: 0x0001 }) }),
  "USB 9999:0001"
);
assert.equal(
  formatSerialPortLabel({ getInfo: () => ({ usbVendorId: 0x1a86 }) }),
  "WCH USB 1A86"
);

console.log("serial-port-label tests passed");
