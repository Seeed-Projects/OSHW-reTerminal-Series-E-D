const DEVICES = [
  {
    id: "all",
    name: "All Devices",
  },
  {
    id: "E1001",
    name: "E1001",
    description: '5.5" ePaper',
  },
  {
    id: "E1002",
    name: "E1002",
    description: '7.5" ePaper',
  },
  {
    id: "E1003",
    name: "E1003",
    description: '10.3" ePaper + Touch',
  },
  {
    id: "E1004",
    name: "E1004",
    description: "No Display",
  },
];

const PLATFORMS = [
  {
    id: "all",
    name: "All Platforms",
  },
  {
    id: "reterminal",
    name: "reTerminal",
    description: "Official reTerminal E-Series firmware",
  },
  {
    id: "esphome",
    name: "ESPHome",
    description: "Reserved for ESPHome builds",
  },
  {
    id: "arduino",
    name: "Arduino Examples",
    description: "Reserved for Arduino example builds",
  },
];

const FUNCTION_GROUPS = [
  {
    id: "all",
    name: "All Functions",
  },
  {
    id: "Peripheral",
    name: "Peripheral",
  },
  {
    id: "Power",
    name: "Power",
  },
  {
    id: "Audio",
    name: "Audio",
  },
  {
    id: "Display",
    name: "Display",
  },
];

const CATEGORY_COLORS = {
  Peripheral: "tag-peripheral",
  Power: "tag-power",
  Audio: "tag-audio",
  Display: "tag-display",
};

const FIRMWARES = [
  {
    id: "RTC_PCF8563",
    name: "RTC Clock Demo",
    tagline:
      "Read and write the onboard PCF8563 real-time clock. Prints date and time every second via Serial1.",
    category: "Peripheral",
    platform: "reterminal",
    compatible: ["E1001", "E1002", "E1003", "E1004"],
    versions: [
      {
        version: "1.0.0",
        label: "Stable",
        manifest: "firmware/RTC_PCF8563/manifest.json",
      },
    ],
    configFields: [],
    icon: "clock",
    sourceUrl:
      "https://github.com/Seeed-Projects/OSHW-reTerminal-Series-E-D/tree/main/examples/RTC_PCF8563",
  },
  {
    id: "LowPower_DeepSleep",
    name: "Deep Sleep Demo",
    tagline:
      "Enter ESP32-S3 deep sleep (~14 uA) and wake on button press. Ultra-low-power standby mode.",
    category: "Power",
    platform: "reterminal",
    compatible: ["E1001", "E1002", "E1003", "E1004"],
    versions: [
      {
        version: "1.0.0",
        label: "Stable",
        manifest: "firmware/LowPower_DeepSleep/manifest.json",
      },
    ],
    configFields: [],
    icon: "sleep",
    sourceUrl:
      "https://github.com/Seeed-Projects/OSHW-reTerminal-Series-E-D/tree/main/examples/LowPower_DeepSleep",
  },
  {
    id: "MicRecordToSD",
    name: "Microphone Recording",
    tagline:
      "Record audio from the onboard PDM microphone and save WAV files to MicroSD card.",
    category: "Audio",
    platform: "reterminal",
    compatible: ["E1001", "E1002", "E1003"],
    versions: [
      {
        version: "1.0.0",
        label: "Stable",
        manifest: "firmware/MicRecordToSD/manifest.json",
      },
    ],
    configFields: [],
    icon: "mic",
    sourceUrl:
      "https://github.com/Seeed-Projects/OSHW-reTerminal-Series-E-D/tree/main/examples/MicRecordToSD",
  },
  {
    id: "E1003_TouchDraw",
    name: "Touch Draw",
    tagline:
      "Tap the 10.3\" capacitive touch panel to draw dots on the ePaper screen in real time.",
    category: "Display",
    platform: "reterminal",
    compatible: ["E1003"],
    versions: [
      {
        version: "1.0.0",
        label: "Stable",
        manifest: "firmware/E1003_TouchDraw/manifest.json",
      },
    ],
    configFields: [],
    icon: "touch",
    sourceUrl:
      "https://github.com/Seeed-Projects/OSHW-reTerminal-Series-E-D/tree/main/examples/E1003_TouchDraw",
  },
];
