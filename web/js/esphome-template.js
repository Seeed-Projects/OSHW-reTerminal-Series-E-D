(function () {
  const SECTION_KEYS = {
    buses: ["i2c", "spi", "i2s_audio"],
    outputs: "output",
    sensors: "sensor",
    binarySensors: "binary_sensor",
    lights: "light",
    times: "time",
  };

  function asArray(value) {
    if (!value) return [];
    return Array.isArray(value) ? value.filter(Boolean) : [value];
  }

  function appendArray(target, key, values) {
    const items = asArray(values);
    if (!items.length) return;
    target[key] = [...(target[key] || []), ...items];
  }

  function mergeContributes(base = {}, override = {}) {
    const merged = { ...base };
    ["buses", "outputs", "sensors", "binarySensors", "lights", "times", "blocks", "onBoot"].forEach((key) => {
      appendArray(merged, key, override[key]);
    });
    ["displayLambda", "display", "deepSleep"].forEach((key) => {
      if (override[key] !== undefined) merged[key] = override[key];
    });
    return merged;
  }

  function getDeviceConfig(platform, deviceId) {
    const devices = platform.templateDevices || {};
    return devices[deviceId] || {
      deviceName: `reterminal-${String(deviceId || "").toLowerCase()}`,
      friendlyName: `reTerminal_${deviceId}`,
      apSsid: `reTerminal-${deviceId}`,
    };
  }

  function replaceTokens(value, tokens) {
    return String(value || "").replace(/\{\{(\w+)\}\}/g, (match, key) =>
      tokens[key] !== undefined ? tokens[key] : match
    );
  }

  function getOptionMap(platform) {
    const map = new Map();
    (platform.templateOptions || []).forEach((option) => {
      map.set(option.id, option);
    });
    return map;
  }

  function expandEsphomeTemplateOptionIds(platform, selectedOptionIds) {
    const optionMap = getOptionMap(platform);
    const selected = new Set(selectedOptionIds || []);
    let changed = true;
    while (changed) {
      changed = false;
      [...selected].forEach((id) => {
        const option = optionMap.get(id);
        (option?.requires || []).forEach((requiredId) => {
          if (!selected.has(requiredId)) {
            selected.add(requiredId);
            changed = true;
          }
        });
      });
    }
    return (platform.templateOptions || [])
      .map((option) => option.id)
      .filter((id) => selected.has(id));
  }

  function resolveContributes(option, deviceId) {
    const base = option.contributes || {};
    const perDevice = option.perDevice?.[deviceId] || {};
    const override = perDevice.contributes || perDevice;
    return mergeContributes(base, override);
  }

  function extractSortKey(item) {
    const idMatch = String(item).match(/^\s+id:\s*([A-Za-z0-9_]+)/m);
    if (idMatch) return idMatch[1];
    const platformMatch = String(item).match(/^\s+- platform:\s*([A-Za-z0-9_]+)/m);
    return platformMatch ? `platform:${platformMatch[1]}` : "";
  }

  function sortYamlListItems(items, order = []) {
    const orderMap = new Map(order.map((key, index) => [key, index]));
    return [...items].sort((a, b) => {
      const aOrder = orderMap.get(extractSortKey(a));
      const bOrder = orderMap.get(extractSortKey(b));
      const aIndex = aOrder === undefined ? Number.MAX_SAFE_INTEGER : aOrder;
      const bIndex = bOrder === undefined ? Number.MAX_SAFE_INTEGER : bOrder;
      return aIndex - bIndex;
    });
  }

  function splitOnBootActions(snippets) {
    return snippets.flatMap((snippet) =>
      String(snippet)
        .split(/\n(?=\s*- )/)
        .map((item) => item.trimEnd())
        .filter(Boolean)
    );
  }

  function sortOnBootActions(actions, order = []) {
    const orderMap = new Map(order.map((needle, index) => [needle, index]));
    return [...actions].sort((a, b) => {
      const aIndex = order.findIndex((needle) => a.includes(needle));
      const bIndex = order.findIndex((needle) => b.includes(needle));
      const normalizedA = aIndex < 0 ? Number.MAX_SAFE_INTEGER : orderMap.get(order[aIndex]);
      const normalizedB = bIndex < 0 ? Number.MAX_SAFE_INTEGER : orderMap.get(order[bIndex]);
      return normalizedA - normalizedB;
    });
  }

  function buildTopLevelList(key, items, order) {
    if (!items.length) return "";
    return `${key}:\n${sortYamlListItems(items, order).join("\n\n")}`;
  }

  function buildOnBootBlock(actions) {
    if (!actions.length) return "      []";
    return actions.join("\n");
  }

  function collectTemplateParts(platform, selectedOptionIds, deviceId) {
    const expandedIds = expandEsphomeTemplateOptionIds(platform, selectedOptionIds);
    const selected = new Set(expandedIds);
    const tokens = getDeviceConfig(platform, deviceId);
    const sections = {
      buses: new Set(),
      outputs: [],
      sensors: [],
      binarySensors: [],
      lights: [],
      times: [],
      blocks: [],
      onBoot: [],
      displayFragments: [],
      display: "",
      deepSleep: "",
    };

    (platform.templateOptions || []).forEach((option) => {
      if (!selected.has(option.id)) return;
      const contributes = resolveContributes(option, deviceId);
      asArray(contributes.buses).forEach((bus) => sections.buses.add(bus));
      appendArray(sections, "outputs", contributes.outputs);
      appendArray(sections, "sensors", contributes.sensors);
      appendArray(sections, "binarySensors", contributes.binarySensors);
      appendArray(sections, "lights", contributes.lights);
      appendArray(sections, "times", contributes.times);
      appendArray(sections, "blocks", contributes.blocks);
      appendArray(sections, "onBoot", contributes.onBoot);
      if (selected.has("display") && option.id !== "display" && contributes.displayLambda) {
        sections.displayFragments.push({ id: option.id, value: contributes.displayLambda });
      }
      if (option.id === "display") sections.display = contributes.display || "";
      if (option.id === "deep_sleep") sections.deepSleep = contributes.deepSleep || "";
    });

    return { expandedIds, selected, tokens, sections };
  }

  function buildEsphomeTemplateContent(platform, selectedOptionIds, deviceId) {
    const { tokens, sections } = collectTemplateParts(platform, selectedOptionIds, deviceId);
    const sectionOrder = platform.templateSectionOrder || {};
    const parts = [];
    const sortedOnBoot = sortOnBootActions(
      splitOnBootActions(sections.onBoot),
      sectionOrder.onBoot || []
    );
    const header = replaceTokens(platform.templateHeader || "", {
      ...tokens,
      onBootActions: buildOnBootBlock(sortedOnBoot),
    });
    if (header) parts.push(header);

    (sectionOrder.buses || SECTION_KEYS.buses).forEach((bus) => {
      if (sections.buses.has(bus) && platform.templateBuses?.[bus]) {
        parts.push(platform.templateBuses[bus]);
      }
    });

    [
      ["outputs", SECTION_KEYS.outputs],
      ["sensors", SECTION_KEYS.sensors],
      ["binarySensors", SECTION_KEYS.binarySensors],
      ["lights", SECTION_KEYS.lights],
      ["times", SECTION_KEYS.times],
    ].forEach(([sectionKey, yamlKey]) => {
      const block = buildTopLevelList(
        yamlKey,
        sections[sectionKey],
        sectionOrder[sectionKey] || []
      );
      if (block) parts.push(block);
    });

    const blockOrder = new Map((sectionOrder.blocks || []).map((id, index) => [id, index]));
    sections.blocks
      .map((block) => ({ block, key: String(block).split(":")[0] }))
      .sort((a, b) => {
        const aIndex = blockOrder.has(a.key) ? blockOrder.get(a.key) : Number.MAX_SAFE_INTEGER;
        const bIndex = blockOrder.has(b.key) ? blockOrder.get(b.key) : Number.MAX_SAFE_INTEGER;
        return aIndex - bIndex;
      })
      .forEach(({ block }) => parts.push(replaceTokens(block, tokens)));

    if (sections.display) {
      const displayOrder = new Map((sectionOrder.displayLambda || []).map((id, index) => [id, index]));
      const displayLambda = sections.displayFragments
        .sort((a, b) => {
          const aIndex = displayOrder.has(a.id) ? displayOrder.get(a.id) : Number.MAX_SAFE_INTEGER;
          const bIndex = displayOrder.has(b.id) ? displayOrder.get(b.id) : Number.MAX_SAFE_INTEGER;
          return aIndex - bIndex;
        })
        .map((fragment) => fragment.value)
        .join("\n\n");
      parts.push(replaceTokens(sections.display, { ...tokens, displayLambda }));
    }

    if (sections.deepSleep) {
      parts.push(replaceTokens(sections.deepSleep, tokens));
    }

    if (platform.templateFooter) parts.push(platform.templateFooter);
    return parts.filter(Boolean).join(platform.templateJoiner || "\n\n") + "\n";
  }

  globalThis.buildEsphomeTemplateContent = buildEsphomeTemplateContent;
  globalThis.expandEsphomeTemplateOptionIds = expandEsphomeTemplateOptionIds;

  if (typeof module !== "undefined") {
    module.exports = {
      buildEsphomeTemplateContent,
      expandEsphomeTemplateOptionIds,
      collectTemplateParts,
    };
  }
})();
