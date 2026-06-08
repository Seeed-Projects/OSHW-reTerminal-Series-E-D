(function () {
  const DEFAULT_MAX_LENGTH = 256 * 1024;
  const DEFAULT_TRIM_NOTICE = "[monitor] Older log output was trimmed.\n";

  function normalizeNotice(notice) {
    const text = notice || DEFAULT_TRIM_NOTICE;
    return text.endsWith("\n") ? text : `${text}\n`;
  }

  // Keeps serial monitor text bounded so high-volume logs cannot exhaust memory.
  // 限制串口监视器文本长度，避免高频日志耗尽内存。
  function createLogBuffer(options = {}) {
    const maxLength = Number.isInteger(options.maxLength) && options.maxLength > 0
      ? options.maxLength
      : DEFAULT_MAX_LENGTH;
    const trimNotice = normalizeNotice(options.trimNotice);
    let text = "";

    function trim() {
      if (text.length <= maxLength) return;

      if (maxLength <= trimNotice.length) {
        text = trimNotice.slice(0, maxLength);
        return;
      }

      const retainedLength = maxLength - trimNotice.length;
      let retainedText = text.slice(-retainedLength);
      const firstLineBreak = retainedText.indexOf("\n");
      if (firstLineBreak >= 0 && firstLineBreak < retainedText.length - 1) {
        retainedText = retainedText.slice(firstLineBreak + 1);
      }
      text = `${trimNotice}${retainedText}`;
    }

    return {
      append(fragment, prefixLineBreak = false) {
        const message = String(fragment || "");
        const lineBreak = prefixLineBreak && text && !text.endsWith("\n") ? "\n" : "";
        text += `${lineBreak}${message}`;
        trim();
      },
      clear() {
        text = "";
      },
      seed(value) {
        text = String(value || "");
        trim();
      },
      text() {
        return text;
      },
    };
  }

  globalThis.createLogBuffer = createLogBuffer;

  if (typeof module !== "undefined") {
    module.exports = { createLogBuffer };
  }
})();
