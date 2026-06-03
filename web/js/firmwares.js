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
    compatible: ["E1001", "E1002", "E1003", "E1004"],
    manifest: "firmware/RTC_PCF8563/manifest.json",
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
    compatible: ["E1001", "E1002", "E1003", "E1004"],
    manifest: "firmware/LowPower_DeepSleep/manifest.json",
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
    compatible: ["E1001", "E1002", "E1003"],
    manifest: "firmware/MicRecordToSD/manifest.json",
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
    compatible: ["E1003"],
    manifest: "firmware/E1003_TouchDraw/manifest.json",
    icon: "touch",
    sourceUrl:
      "https://github.com/Seeed-Projects/OSHW-reTerminal-Series-E-D/tree/main/examples/E1003_TouchDraw",
  },
];
