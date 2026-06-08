(function () {
  const KNOWN_USB_VENDORS = {
    0x303a: "Espressif",
    0x10c4: "Silicon Labs",
    0x1a86: "WCH",
    0x0403: "FTDI",
    0x2341: "Arduino",
  };

  function toHexId(value) {
    if (!Number.isInteger(value)) return "";
    return value.toString(16).toUpperCase().padStart(4, "0");
  }

  // Formats the browser-exposed serial identity for the monitor UI.
  // 格式化浏览器允许暴露的串口身份信息，供监视器界面显示。
  function formatSerialPortLabel(port) {
    const fallbackLabel = port ? "Serial port" : "Unavailable";
    if (!port || typeof port.getInfo !== "function") return fallbackLabel;

    const info = port.getInfo() || {};
    const vendorId = toHexId(info.usbVendorId);
    const productId = toHexId(info.usbProductId);

    if (vendorId && productId) {
      const vendorName = KNOWN_USB_VENDORS[info.usbVendorId];
      return vendorName
        ? `${vendorName} USB ${vendorId}:${productId}`
        : `USB ${vendorId}:${productId}`;
    }

    if (vendorId) {
      const vendorName = KNOWN_USB_VENDORS[info.usbVendorId];
      return vendorName ? `${vendorName} USB ${vendorId}` : `USB ${vendorId}`;
    }

    return fallbackLabel;
  }

  globalThis.formatSerialPortLabel = formatSerialPortLabel;

  if (typeof module !== "undefined") {
    module.exports = { formatSerialPortLabel };
  }
})();
