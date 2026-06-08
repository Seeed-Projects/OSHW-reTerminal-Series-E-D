(function () {
  const DEFAULT_MAX_LENGTH = 256 * 1024;
  const DEFAULT_TRIM_NOTICE = "[monitor] Older log output was trimmed.\n";
  const DEFAULT_VIEW_NOTICE = "[monitor] Showing latest retained log output.\n";

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
    const viewNotice = normalizeNotice(options.viewNotice || DEFAULT_VIEW_NOTICE);
    let text = "";

    function trimText(value, limit, notice) {
      if (value.length <= limit) return value;

      if (limit <= notice.length) {
        return value.slice(-limit);
      }

      const retainedLength = limit - notice.length;
      let retainedText = value.slice(-retainedLength);
      const firstLineBreak = retainedText.indexOf("\n");
      if (firstLineBreak >= 0 && firstLineBreak < retainedText.length - 1) {
        retainedText = retainedText.slice(firstLineBreak + 1);
      }
      return `${notice}${retainedText}`;
    }

    function trim() {
      text = trimText(text, maxLength, trimNotice);
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
      view(maxLength) {
        if (!Number.isInteger(maxLength) || maxLength <= 0) return text;
        return trimText(text, maxLength, viewNotice);
      },
    };
  }

  globalThis.createLogBuffer = createLogBuffer;

  if (typeof module !== "undefined") {
    module.exports = { createLogBuffer };
  }
})();
