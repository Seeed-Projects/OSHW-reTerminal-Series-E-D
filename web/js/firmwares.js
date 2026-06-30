const DEVICES = [
  {
    id: "E1001",
    name: "reTerminal E1001",
    description: '7.5" monochrome ePaper display',
    image: "assets/devices/reterminal-e1001.jpg",
    imageAlt: "reTerminal E1001 product photo",
    specs: [
      '7.5" display',
      "Monochrome",
      "4-level grayscale",
    ],
  },
  {
    id: "E1002",
    name: "reTerminal E1002",
    description: '7.3" full-color ePaper display',
    image: "assets/devices/reterminal-e1002.jpg",
    imageAlt: "reTerminal E1002 product photo",
    specs: [
      '7.3" display',
      "Full color",
      "E Ink Spectra 6",
    ],
  },
  {
    id: "E1003",
    name: "reTerminal E1003",
    description: '10.3" touch ePaper dashboard display',
    image: "assets/devices/reterminal-e1003.jpg",
    imageAlt: "reTerminal E1003 product photo",
    specs: [
      '10.3" display',
      "16-level grayscale",
      "Touch",
    ],
  },
  {
    id: "E1004",
    name: "reTerminal E1004",
    description: '13.3" full-color ePaper display',
    image: "assets/devices/reterminal-e1004.jpg",
    imageAlt: "reTerminal E1004 product photo",
    specs: [
      '13.3" display',
      "Full color",
      "E Ink Spectra 6",
    ],
  },
];

const PLATFORM_GROUPS = [
  {
    id: "official",
    title: "Official Platforms",
    description: "Start from supported ecosystem firmware and product workflows.",
  },
  {
    id: "community",
    title: "Community Projects",
    description: "Explore contributed examples that build on the official device stack.",
  },
];

const PLATFORM_CARDS = [
  {
    id: "base",
    group: "base",
    name: "Base",
    tagline: "Product bring-up demos for first-time hardware checks.",
    description:
      "Use Base firmware when you want to verify core hardware quickly: RTC, low-power mode, microphone recording, touch, and display behavior.",
    logo: "assets/platforms/base-logo.svg",
    preview: "assets/platforms/base-preview.svg?v=20260604-platform-art",
    previewAlt: "Basic firmware setup preview",
    accent: "#004966",
    highlight: "#8FC31F",
    supportedDevices: ["E1001", "E1002", "E1003", "E1004"],
    installReady: true,
    bullets: [
      "Fast product smoke tests",
      "No ecosystem account required",
      "Good starting point before advanced integrations",
    ],
    versions: [
      {
        version: "1.0.0",
        label: "Stable",
      },
    ],
    configFields: [],
    firmwareOptions: [
      {
        id: "RTC_PCF8563",
        name: "RTC Clock Demo",
        description: "Read and write the onboard PCF8563 real-time clock.",
        category: "Peripheral",
        compatible: ["E1001", "E1002", "E1003", "E1004"],
        notes: [
          { type: "warning", text: "Install a CR1220 coin cell battery in the RTC slot before flashing — without it, the clock resets every time the board loses power." },
          { type: "info", text: "First flash: the firmware automatically writes the build timestamp to the RTC. Subsequent reboots: the stored time is preserved (battery must be installed). The clock prints every second on Serial1 (GPIO43 TX, 115200 baud)." },
        ],
      },
      {
        id: "LowPower_DeepSleep",
        name: "Deep Sleep Demo",
        description: "Enter ESP32-S3 deep sleep and wake on button press.",
        category: "Power",
        compatible: ["E1001", "E1002", "E1003", "E1004"],
      },
      {
        id: "MicRecordToSD_E1001",
        name: "Microphone Recording",
        description: "Records audio from the onboard PDM microphone and saves it as a WAV file on the MicroSD card. Press the green KEY0 button once to start recording; press it again to stop. The LED blinks while recording is in progress.",
        category: "Audio",
        compatible: ["E1001", "E1002"],
      },
      {
        id: "MicRecordToSD_E1003",
        name: "Microphone Recording",
        description: "Records audio from the onboard PDM microphone and saves it as a WAV file on the MicroSD card. Press the green KEY0 button once to start recording; press it again to stop. The LED blinks while recording is in progress.",
        category: "Audio",
        compatible: ["E1003"],
      },
      {
        id: "E1003_TouchDraw",
        name: "Touch Draw",
        description: "Draw dots on the E1003 ePaper display from touch input.",
        category: "Display",
        compatible: ["E1003"],
      },
      {
        id: "LED_Control_E1001",
        name: "LED Blink Demo",
        description: "Toggle the onboard LED with 1-second intervals.",
        category: "Peripheral",
        compatible: ["E1001", "E1002"],
      },
      {
        id: "LED_Control_E1003",
        name: "LED Blink Demo",
        description: "Toggle the onboard LED with 1-second intervals.",
        category: "Peripheral",
        compatible: ["E1003"],
      },
      {
        id: "LED_Control_E1004",
        name: "LED Blink Demo",
        description: "Toggle the onboard LED with 1-second intervals.",
        category: "Peripheral",
        compatible: ["E1004"],
      },
      {
        id: "Buzzer_Control",
        name: "Buzzer Alert Demo",
        description: "Play basic alert tones: single beep, double beep, and alarm.",
        category: "Peripheral",
        compatible: ["E1001", "E1002", "E1003", "E1004"],
      },
      {
        id: "Buzzer_Music",
        name: "Buzzer Music Tones",
        description: "Play musical note arpeggios using the onboard buzzer.",
        category: "Peripheral",
        compatible: ["E1001", "E1002", "E1003", "E1004"],
      },
      {
        id: "UserButtons",
        name: "Button Press Demo",
        description: "Detect and report presses on all three user buttons.",
        category: "Peripheral",
        compatible: ["E1001", "E1002", "E1003", "E1004"],
      },
      {
        id: "Battery_Monitor",
        name: "Battery Voltage Monitor",
        description: "Read and display the battery voltage via ADC.",
        category: "Power",
        compatible: ["E1001", "E1002", "E1003", "E1004"],
      },
      {
        id: "MicroSD_ListFiles",
        name: "MicroSD File Listing",
        description: "Mount the MicroSD card and list files to serial output.",
        category: "Peripheral",
        compatible: ["E1001", "E1002", "E1003", "E1004"],
      },
      {
        id: "GxEPD2_reTerminal_E1001",
        name: "Display Demo (GxEPD2)",
        description: "Six-screen B&W demo: splash, system info, typography, geometry, patterns, dashboard.",
        category: "Display",
        compatible: ["E1001"],
        notes: [
          { type: "warning", text: "If a MicroSD card is inserted, the display may not work due to SPI bus conflict. Please remove the SD card before flashing this firmware." },
        ],
      },
      {
        id: "GxEPD2_reTerminal_E1002",
        name: "Color Display Demo (GxEPD2)",
        description: "Six-screen 6-color demo showcasing the full color palette and patterns.",
        category: "Display",
        compatible: ["E1002"],
        notes: [
          { type: "warning", text: "If a MicroSD card is inserted, the display may not work due to SPI bus conflict. Please remove the SD card before flashing this firmware." },
        ],
      },
      {
        id: "GxEPD2_reTerminal_E1003",
        name: "Display Demo (GxEPD2)",
        description: "Six-screen monochrome demo optimized for the 10.3\" 1872x1404 panel.",
        category: "Display",
        compatible: ["E1003"],
      },
      {
        id: "GxEPD2_reTerminal_E1004",
        name: "Color Display Demo (GxEPD2)",
        description: "Six-screen 6-color demo for the 13.3\" dual-chip Spectra 6 panel.",
        category: "Display",
        compatible: ["E1004"],
      },
      {
        id: "GxEPD2_reTerminal_E1001_Gray4",
        name: "4-Level Grayscale Demo (Adafruit GFX)",
        description: "Four grayscale bands and concentric circles on the E1001 panel. Uses Adafruit_GFX canvas with raw SPI register access.",
        category: "Display",
        compatible: ["E1001"],
        notes: [
          { type: "warning", text: "If a MicroSD card is inserted, the display may not work due to SPI bus conflict. Please remove the SD card before flashing this firmware." },
        ],
      },
      {
        id: "Seeed_GFX_E1001_Gray4",
        name: "4-Level Grayscale Demo (Seeed GFX)",
        description: "Four horizontal grayscale bands on the E1001 panel using the Seeed_GFX / TFT_eSPI high-level EPaper API.",
        category: "Display",
        compatible: ["E1001"],
        notes: [
          { type: "warning", text: "If a MicroSD card is inserted, the display may not work due to SPI bus conflict. Please remove the SD card before flashing this firmware." },
        ],
      },
      {
        id: "GxEPD2_reTerminal_E1003_Gray16",
        name: "16-Level Grayscale Demo",
        description: "Sixteen grayscale bands and gradient bar on the E1003 IT8951 panel.",
        category: "Display",
        compatible: ["E1003"],
      },
      {
        id: "E1001_ChineseTextDemo",
        name: "Chinese Text Demo",
        description: "Load a TrueType font from SPIFFS and render UTF-8 Chinese text on the E1001 ePaper display via OpenFontRender.",
        category: "Display",
        compatible: ["E1001"],
        notes: [
          { type: "info", text: "The embedded Chinese font (~2.2 MB) is flashed to a SPIFFS partition together with the firmware. Test strings appear on screen; check Serial1 (GPIO43 TX, 115200 baud) for status." },
        ],
      },
      {
        id: "E1002_ChineseTextDemo",
        name: "Chinese Text Demo",
        description: "Load a TrueType font from SPIFFS and render UTF-8 Chinese text on the E1002 ePaper display via OpenFontRender.",
        category: "Display",
        compatible: ["E1002"],
        notes: [
          { type: "info", text: "The embedded Chinese font (~2.2 MB) is flashed to a SPIFFS partition together with the firmware. Test strings appear on screen; check Serial1 (GPIO43 TX, 115200 baud) for status." },
        ],
      },
      {
        id: "E1003_ChineseTextDemo",
        name: "Chinese Text Demo",
        description: "Load a TrueType font from SPIFFS and render UTF-8 Chinese text on the E1003 ePaper display via OpenFontRender.",
        category: "Display",
        compatible: ["E1003"],
        notes: [
          { type: "info", text: "The embedded Chinese font (~2.2 MB) is flashed to a SPIFFS partition together with the firmware. Test strings appear on screen; check Serial1 (GPIO43 TX, 115200 baud) for status." },
        ],
      },
      {
        id: "E1004_ChineseTextDemo",
        name: "Chinese Text Demo",
        description: "Load a TrueType font from SPIFFS and render UTF-8 Chinese text on the E1004 ePaper display via OpenFontRender.",
        category: "Display",
        compatible: ["E1004"],
        notes: [
          { type: "info", text: "The embedded Chinese font (~2.2 MB) is flashed to a SPIFFS partition together with the firmware. Test strings appear on screen; check Serial1 (GPIO43 TX, 115200 baud) for status." },
        ],
      },
      {
        id: "SHT4x_Sensor",
        name: "Temperature & Humidity Sensor",
        description: "Read temperature and humidity from the SHT40 sensor via I2C every 5 seconds.",
        category: "Peripheral",
        compatible: ["E1001", "E1002", "E1003", "E1004"],
        notes: [
          { type: "info", text: "An SHT40 sensor must be connected to the I2C Grove port (SDA=GPIO19, SCL=GPIO20). Readings print every 5 seconds on Serial1 (GPIO43 TX, 115200 baud)." },
        ],
      },
      {
        id: "SD_ImagePipeline_E1001_BW",
        name: "SD Image Display (B&W)",
        description: "Load a JPEG/PNG from MicroSD and render it on the E1001 panel with dithering.",
        category: "Display",
        compatible: ["E1001"],
        configFields: [
          { id: "cfgImagePath", nvsKey: "imagePath", nvsType: "string", label: "Image file path", type: "text", defaultValue: "/img/demo.jpg", placeholder: "/img/demo.jpg" },
          { id: "cfgDither", nvsKey: "dither", nvsType: "i32", label: "Dithering algorithm", type: "select", defaultValue: 2, options: [{ value: 0, label: "None" }, { value: 1, label: "Bayer 8x8" }, { value: 2, label: "Floyd-Steinberg" }, { value: 3, label: "Jarvis-Judice-Ninke" }, { value: 4, label: "Atkinson" }] },
          { id: "cfgGamma", nvsKey: "gamma", nvsType: "float", label: "Gamma correction", type: "number", defaultValue: 1.0, min: 0.1, max: 3.0, step: 0.1 },
          { id: "cfgInvert", nvsKey: "invert", nvsType: "u8", label: "Invert black/white", type: "checkbox", defaultValue: 0 },
          { id: "cfgAnchor", nvsKey: "anchor", nvsType: "i32", label: "Image position", type: "select", defaultValue: 4, options: [{ value: 0, label: "Top-Left" }, { value: 1, label: "Top-Center" }, { value: 2, label: "Top-Right" }, { value: 3, label: "Middle-Left" }, { value: 4, label: "Center" }, { value: 5, label: "Middle-Right" }, { value: 6, label: "Bottom-Left" }, { value: 7, label: "Bottom-Center" }, { value: 8, label: "Bottom-Right" }] },
          { id: "cfgFitMode", nvsKey: "fitMode", nvsType: "i32", label: "Scaling mode", type: "select", defaultValue: 2, options: [{ value: 0, label: "Original size" }, { value: 1, label: "Fit to screen" }, { value: 2, label: "Custom scale" }] },
          { id: "cfgScale", nvsKey: "scale", nvsType: "float", label: "Scale factor", type: "number", defaultValue: 0.7, min: 0.1, max: 2.0, step: 0.1 },
        ],
        notes: [
          { type: "warning", text: "Place a JPEG or PNG image at /img/demo.jpg on the MicroSD card before flashing." },
          { type: "info", text: "Supports Floyd-Steinberg, Bayer, Jarvis, and Atkinson dithering. Configure before flashing in Firmware Hub." },
        ],
      },
      {
        id: "SD_ImagePipeline_E1001_Gray4",
        name: "SD Image Display (4-Gray)",
        description: "Load a JPEG/PNG from MicroSD and render it in 4-level grayscale on the E1001 panel.",
        category: "Display",
        compatible: ["E1001"],
        configFields: [
          { id: "cfgImagePath", nvsKey: "imagePath", nvsType: "string", label: "Image file path", type: "text", defaultValue: "/img/demo.jpg", placeholder: "/img/demo.jpg" },
          { id: "cfgDither", nvsKey: "dither", nvsType: "i32", label: "Dithering algorithm", type: "select", defaultValue: 2, options: [{ value: 0, label: "None" }, { value: 1, label: "Bayer 8x8" }, { value: 2, label: "Floyd-Steinberg" }, { value: 3, label: "Jarvis-Judice-Ninke" }, { value: 4, label: "Atkinson" }] },
          { id: "cfgGamma", nvsKey: "gamma", nvsType: "float", label: "Gamma correction", type: "number", defaultValue: 1.0, min: 0.1, max: 3.0, step: 0.1 },
          { id: "cfgAnchor", nvsKey: "anchor", nvsType: "i32", label: "Image position", type: "select", defaultValue: 4, options: [{ value: 0, label: "Top-Left" }, { value: 1, label: "Top-Center" }, { value: 2, label: "Top-Right" }, { value: 3, label: "Middle-Left" }, { value: 4, label: "Center" }, { value: 5, label: "Middle-Right" }, { value: 6, label: "Bottom-Left" }, { value: 7, label: "Bottom-Center" }, { value: 8, label: "Bottom-Right" }] },
          { id: "cfgFitMode", nvsKey: "fitMode", nvsType: "i32", label: "Scaling mode", type: "select", defaultValue: 2, options: [{ value: 0, label: "Original size" }, { value: 1, label: "Fit to screen" }, { value: 2, label: "Custom scale" }] },
          { id: "cfgScale", nvsKey: "scale", nvsType: "float", label: "Scale factor", type: "number", defaultValue: 0.7, min: 0.1, max: 2.0, step: 0.1 },
        ],
        notes: [
          { type: "warning", text: "Place a JPEG or PNG image at /img/demo.jpg on the MicroSD card before flashing." },
        ],
      },
      {
        id: "SD_ImagePipeline_E1002",
        name: "SD Image Display (Color)",
        description: "Load a JPEG/PNG from MicroSD and render it in 6-color on the E1002 Spectra 6 panel.",
        category: "Display",
        compatible: ["E1002"],
        configFields: [
          { id: "cfgImagePath", nvsKey: "imagePath", nvsType: "string", label: "Image file path", type: "text", defaultValue: "/img/demo.jpg", placeholder: "/img/demo.jpg" },
          { id: "cfgDither", nvsKey: "dither", nvsType: "i32", label: "Dithering algorithm", type: "select", defaultValue: 2, options: [{ value: 0, label: "None" }, { value: 1, label: "Bayer 8x8" }, { value: 2, label: "Floyd-Steinberg" }, { value: 3, label: "Jarvis-Judice-Ninke" }, { value: 4, label: "Atkinson" }] },
          { id: "cfgGamma", nvsKey: "gamma", nvsType: "float", label: "Gamma correction", type: "number", defaultValue: 1.0, min: 0.1, max: 3.0, step: 0.1 },
          { id: "cfgAnchor", nvsKey: "anchor", nvsType: "i32", label: "Image position", type: "select", defaultValue: 4, options: [{ value: 0, label: "Top-Left" }, { value: 1, label: "Top-Center" }, { value: 2, label: "Top-Right" }, { value: 3, label: "Middle-Left" }, { value: 4, label: "Center" }, { value: 5, label: "Middle-Right" }, { value: 6, label: "Bottom-Left" }, { value: 7, label: "Bottom-Center" }, { value: 8, label: "Bottom-Right" }] },
          { id: "cfgFitMode", nvsKey: "fitMode", nvsType: "i32", label: "Scaling mode", type: "select", defaultValue: 2, options: [{ value: 0, label: "Original size" }, { value: 1, label: "Fit to screen" }, { value: 2, label: "Custom scale" }] },
          { id: "cfgScale", nvsKey: "scale", nvsType: "float", label: "Scale factor", type: "number", defaultValue: 0.7, min: 0.1, max: 2.0, step: 0.1 },
        ],
        notes: [
          { type: "warning", text: "Place a JPEG or PNG image at /img/demo.jpg on the MicroSD card before flashing." },
        ],
      },
      {
        id: "SD_ImagePipeline_E1003",
        name: "SD Image Display (16-Gray)",
        description: "Load a JPEG/PNG from MicroSD and render it in 16-level grayscale on the E1003 panel.",
        category: "Display",
        compatible: ["E1003"],
        configFields: [
          { id: "cfgImagePath", nvsKey: "imagePath", nvsType: "string", label: "Image file path", type: "text", defaultValue: "/img/demo.jpg", placeholder: "/img/demo.jpg" },
          { id: "cfgDither", nvsKey: "dither", nvsType: "i32", label: "Dithering algorithm", type: "select", defaultValue: 2, options: [{ value: 0, label: "None" }, { value: 1, label: "Bayer 8x8" }, { value: 2, label: "Floyd-Steinberg" }, { value: 3, label: "Jarvis-Judice-Ninke" }, { value: 4, label: "Atkinson" }] },
          { id: "cfgGamma", nvsKey: "gamma", nvsType: "float", label: "Gamma correction", type: "number", defaultValue: 1.0, min: 0.1, max: 3.0, step: 0.1 },
          { id: "cfgAnchor", nvsKey: "anchor", nvsType: "i32", label: "Image position", type: "select", defaultValue: 4, options: [{ value: 0, label: "Top-Left" }, { value: 1, label: "Top-Center" }, { value: 2, label: "Top-Right" }, { value: 3, label: "Middle-Left" }, { value: 4, label: "Center" }, { value: 5, label: "Middle-Right" }, { value: 6, label: "Bottom-Left" }, { value: 7, label: "Bottom-Center" }, { value: 8, label: "Bottom-Right" }] },
          { id: "cfgFitMode", nvsKey: "fitMode", nvsType: "i32", label: "Scaling mode", type: "select", defaultValue: 0, options: [{ value: 0, label: "Original size" }, { value: 1, label: "Fit to screen" }, { value: 2, label: "Custom scale" }] },
          { id: "cfgScale", nvsKey: "scale", nvsType: "float", label: "Scale factor", type: "number", defaultValue: 1.0, min: 0.1, max: 2.0, step: 0.1 },
        ],
        notes: [
          { type: "warning", text: "Place a JPEG or PNG image at /img/demo.jpg on the MicroSD card before flashing." },
        ],
      },
      {
        id: "SD_ImagePipeline_E1004",
        name: "SD Image Display (Color)",
        description: "Load a JPEG/PNG from MicroSD and render it in 6-color on the E1004 Spectra 6 panel.",
        category: "Display",
        compatible: ["E1004"],
        configFields: [
          { id: "cfgImagePath", nvsKey: "imagePath", nvsType: "string", label: "Image file path", type: "text", defaultValue: "/img/demo.jpg", placeholder: "/img/demo.jpg" },
          { id: "cfgDither", nvsKey: "dither", nvsType: "i32", label: "Dithering algorithm", type: "select", defaultValue: 1, options: [{ value: 0, label: "None" }, { value: 1, label: "Bayer 8x8" }, { value: 2, label: "Floyd-Steinberg" }, { value: 3, label: "Jarvis-Judice-Ninke" }, { value: 4, label: "Atkinson" }] },
          { id: "cfgGamma", nvsKey: "gamma", nvsType: "float", label: "Gamma correction", type: "number", defaultValue: 1.0, min: 0.1, max: 3.0, step: 0.1 },
          { id: "cfgAnchor", nvsKey: "anchor", nvsType: "i32", label: "Image position", type: "select", defaultValue: 4, options: [{ value: 0, label: "Top-Left" }, { value: 1, label: "Top-Center" }, { value: 2, label: "Top-Right" }, { value: 3, label: "Middle-Left" }, { value: 4, label: "Center" }, { value: 5, label: "Middle-Right" }, { value: 6, label: "Bottom-Left" }, { value: 7, label: "Bottom-Center" }, { value: 8, label: "Bottom-Right" }] },
          { id: "cfgFitMode", nvsKey: "fitMode", nvsType: "i32", label: "Scaling mode", type: "select", defaultValue: 0, options: [{ value: 0, label: "Original size" }, { value: 1, label: "Fit to screen" }, { value: 2, label: "Custom scale" }] },
          { id: "cfgScale", nvsKey: "scale", nvsType: "float", label: "Scale factor", type: "number", defaultValue: 1.0, min: 0.1, max: 2.0, step: 0.1 },
        ],
        notes: [
          { type: "warning", text: "Place a JPEG or PNG image at /img/demo.jpg on the MicroSD card before flashing." },
        ],
      },
    ],
  },
  {
    id: "esphome",
    group: "official",
    name: "ESPHome",
    tagline: "Smart-home firmware with YAML setup and Home Assistant integration.",
    source: {
      label: "Official website",
      url: "https://esphome.io/",
    },
    wiki: {
      label: "Wiki",
      url: "https://wiki.seeedstudio.com/epaper_work_with_esphome/",
    },
    description:
      "ESPHome turns microcontroller boards into local smart-home devices using simple YAML configuration, web tools, and Home Assistant workflows.",
    logo: "assets/platforms/esphome-logo-card.png",
    preview: "assets/platforms/esphome-preview-enhanced.png",
    previewAlt: "ESPHome dashboard preview",
    accent: "#004966",
    highlight: "#8FC31F",
    supportedDevices: ["E1001", "E1002"],
    installReady: false,
    bullets: [
      "Home Assistant friendly",
      "Wi-Fi provisioning expected",
      "Good for dashboards, sensors, and local automation",
    ],
    versions: [
      {
        version: "2026.5.2",
        label: "Preview",
      },
    ],
    configFields: [
      {
        id: "wifiSsid",
        label: "Wi-Fi SSID",
        type: "text",
        placeholder: "Office Wi-Fi",
      },
      {
        id: "wifiPassword",
        label: "Wi-Fi password",
        type: "password",
        placeholder: "Stored locally before flashing",
      },
    ],
    templateMode: true,
    templateDevices: {
      E1001: {
        deviceName: "reterminal-e1001",
        friendlyName: "reTerminal_E1001",
        apSsid: "reTerminal-E1001",
        headerBanner: `# =============================================================================
# reTerminal E1001 — ESPHome Full Hardware Example
# All onboard peripherals enabled in the most basic way
# reTerminal E1001 — ESPHome 全硬件基础示例
# 以最基本的方式启用板载所有外设
# =============================================================================
#
# Hardware list / 硬件清单:
#   1. 7.5" B/W ePaper display (800x480, SPI)     / 7.5 英寸黑白墨水屏
#   2. 3x buttons (GPIO3, GPIO4, GPIO5)            / 3 个按钮
#   3. Buzzer (GPIO45, PWM)                         / 蜂鸣器
#   4. Onboard LED (GPIO6)                          / 板载 LED
#   5. Battery voltage (GPIO1 ADC + GPIO21 EN)      / 电池电压监测
#   6. SHT4x temp & humidity sensor (I2C)           / SHT4x 温湿度传感器
#   7. PCF8563 RTC (I2C addr 0x51, CR1220 backup)   / PCF8563 实时时钟
#   8. PDM Microphone (CLK=42, DATA=41, EN=38)      / PDM 麦克风
#   9. MicroSD card slot (SPI + DET=15, EN=16)      / MicroSD 卡槽
#  10. Deep sleep with button wake-up                / 深度睡眠 + 按钮唤醒
#
# Serial debug: UART0 via CH340K USB bridge
# 串口调试：通过 CH340K USB 桥接芯片走 UART0
# =============================================================================`,
      },
      E1002: {
        deviceName: "reterminal-e1002",
        friendlyName: "reTerminal_E1002",
        apSsid: "reTerminal-E1002",
        headerBanner: `# =============================================================================
# reTerminal E1002 — ESPHome Full Hardware Example
# All onboard peripherals enabled in the most basic way
# reTerminal E1002 — ESPHome 全硬件基础示例
# 以最基本的方式启用板载所有外设
# =============================================================================
#
# Hardware list / 硬件清单:
#   1. 7.5" COLOR ePaper display (800x480, SPI)    / 7.5 英寸彩色墨水屏
#      Supported colors: BLACK, RED, GREEN, BLUE, YELLOW
#      支持颜色：黑、红、绿、蓝、黄
#   2. 3x buttons (GPIO3, GPIO4, GPIO5)            / 3 个按钮
#   3. Buzzer (GPIO45, PWM)                         / 蜂鸣器
#   4. Onboard LED (GPIO6)                          / 板载 LED
#   5. Battery voltage (GPIO1 ADC + GPIO21 EN)      / 电池电压监测
#   6. SHT4x temp & humidity sensor (I2C)           / SHT4x 温湿度传感器
#   7. PCF8563 RTC (I2C addr 0x51, CR1220 backup)   / PCF8563 实时时钟
#   8. PDM Microphone (CLK=42, DATA=41, EN=38)      / PDM 麦克风
#   9. MicroSD card slot (SPI + DET=15, EN=16)      / MicroSD 卡槽
#  10. Deep sleep with button wake-up                / 深度睡眠 + 按钮唤醒
#
# NOTE: Requires ESPHome >= 2025.11.1 for epaper_spi platform
# 注意：需要 ESPHome >= 2025.11.1 才能使用 epaper_spi 平台
#
# Serial debug: UART0 via CH340K USB bridge
# 串口调试：通过 CH340K USB 桥接芯片走 UART0
# =============================================================================`,
      },
    },
    templateHeader: `{{headerBanner}}
substitutions:
  device_name: {{deviceName}}
  friendly_name: "{{friendlyName}}"

esphome:
  name: \${device_name}
  friendly_name: \${friendly_name}
  on_boot:
    priority: 600
    then:
{{onBootActions}}

esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: arduino

# Enable serial logging / 启用串口日志
# The reTerminal E series has a CH340K USB-to-UART bridge on UART0.
# Default ESP32-S3 logger uses USB_CDC — won't produce output on the physical port.
# reTerminal E 系列板载 CH340K 桥接芯片，串口走 UART0，非默认的 USB_CDC。
logger:
  hardware_uart: UART0

# =============================================================================
# Fonts / 字体
# =============================================================================

font:
  - file: "gfonts://Inter@700"
    id: font_small
    size: 20
  - file: "gfonts://Inter@700"
    id: font_medium
    size: 32
  - file: "gfonts://Inter@700"
    id: font_large
    size: 48`,
    templateFooter: `# secrets.yaml (create this file alongside your config):
# wifi_ssid: "Your WiFi SSID"
# wifi_password: "Your WiFi Password"
# ota_password: "choose-a-strong-password"
# api_encryption_key: "generate with: openssl rand -base64 32"`,
    templateFileExtension: "yaml",
    templateFileMimeType: "text/yaml",
    templateFilePattern: "esphome-reterminal-{deviceId}",
    templateBuses: {
      spi: `# SPI — shared by ePaper display and SD card
# SPI — 墨水屏和 SD 卡共用
spi:
  clk_pin: GPIO7
  mosi_pin: GPIO9
  # MISO is needed for SD card reading (ePaper is write-only)
  # MISO 用于 SD 卡读取（墨水屏只写不读）
  miso_pin: GPIO8`,
      i2c: `# I2C — shared by SHT4x sensor, PCF8563 RTC
# I2C — SHT4x 传感器和 PCF8563 实时时钟共用
i2c:
  scl: GPIO20
  sda: GPIO19`,
      i2s_audio: `# I2S audio bus — for the onboard PDM microphone
# I2S 音频总线 — 用于板载 PDM 麦克风
# i2s_lrclk_pin serves as the PDM clock output in PDM mode
# PDM 模式下 i2s_lrclk_pin 充当 PDM 时钟输出
i2s_audio:
  i2s_lrclk_pin: GPIO42`,
    },
    templateSectionOrder: {
      buses: ["spi", "i2c", "i2s_audio"],
      outputs: ["bsp_led", "bsp_sd_enable", "bsp_battery_enable", "buzzer_pwm", "mic_power_enable"],
      sensors: ["temp_sensor", "battery_voltage", "battery_level"],
      binarySensors: ["button_1", "button_3", "sd_card_detect"],
      lights: ["onboard_led", "buzzer"],
      times: ["rtc_time", "platform:homeassistant"],
      blocks: ["api", "ota", "wifi", "captive_portal", "microphone"],
      onBoot: [
        "output.turn_on: bsp_sd_enable",
        "output.turn_on: bsp_battery_enable",
        "output.turn_on: mic_power_enable",
        "delay: 200ms",
        "component.update: battery_voltage",
        "component.update: battery_level",
        "pcf8563.read_time",
      ],
      displayLambda: ["rtc", "temp_humidity", "battery", "sd_detect", "microphone", "buttons"],
    },
    templateOptions: [
      {
        id: "wifi_ota",
        label: "Wi-Fi + OTA + captive portal",
        defaultChecked: true,
        description: "Wi-Fi, OTA updates, and fallback AP provisioning",
        requires: [],
        contributes: {
          blocks: [`# OTA (Over-The-Air) firmware update / 空中固件更新
ota:
  - platform: esphome
    password: !secret ota_password`, `# Wi-Fi settings / Wi-Fi 设置
wifi:
  ssid: {{wifiSsidValue}}
  password: {{wifiPasswordValue}}
  ap:
    ssid: "{{apSsid}}"
    password: "ChangeMe123"`, `captive_portal:`],
        },
      },
      {
        id: "ha_api",
        label: "Home Assistant API",
        defaultChecked: true,
        description: "Native encrypted integration with Home Assistant",
        requires: [],
        contributes: {
          blocks: [`# Home Assistant native API / Home Assistant 原生 API
api:
  encryption:
    key: !secret api_encryption_key`],
        },
      },
      {
        id: "display",
        label: "ePaper dashboard",
        defaultChecked: true,
        description: "Device-specific reTerminal ePaper dashboard",
        requires: [],
        contributes: {
          buses: ["spi"],
        },
        perDevice: {
          E1001: {
            contributes: {
              display: `# =============================================================================
# [1] ePaper display (7.5" B/W, 800x480)
# 墨水屏（7.5 英寸黑白，800×480）
# =============================================================================

display:
  - platform: waveshare_epaper
    id: epaper_display
    model: 7.50inv2
    cs_pin: GPIO10
    dc_pin: GPIO11
    reset_pin:
      number: GPIO12
      inverted: false
    busy_pin:
      number: GPIO13
      inverted: true
    update_interval: 300s
    lambda: |-
      // ---- Log all sensor values / 打印所有传感器值到日志 ----
      ESP_LOGD("display", "=== ePaper display refresh ===");

      // ---- Title / 标题 ----
      it.printf(400, 15, id(font_medium), TextAlign::TOP_CENTER,
                "reTerminal E1001 Dashboard");
      it.line(20, 55, 780, 55);

{{displayLambda}}

      // ---- Footer / 页脚 ----
      it.printf(400, 458, id(font_small), TextAlign::TOP_CENTER,
                "Selected reTerminal hardware features enabled");`,
            },
          },
          E1002: {
            contributes: {
              display: `# =============================================================================
# [1] ePaper display (7.5" COLOR, 800x480)
# 墨水屏（7.5 英寸彩色，800×480）
# Requires ESPHome >= 2025.11.1 / 需要 ESPHome >= 2025.11.1
# =============================================================================

display:
  - platform: epaper_spi
    id: epaper_display
    model: Seeed-reTerminal-E1002
    update_interval: 300s
    lambda: |-
      // Define available colors / 定义可用颜色
      const auto BLACK   = Color(0,   0,   0,   0);
      const auto RED     = Color(255, 0,   0,   0);
      const auto GREEN   = Color(0,   255, 0,   0);
      const auto BLUE    = Color(0,   0,   255, 0);
      const auto YELLOW  = Color(255, 255, 0,   0);

      // ---- Log all sensor values / 打印所有传感器值到日志 ----
      ESP_LOGD("display", "=== ePaper display refresh ===");

      // ---- Title (BLUE) / 标题（蓝色）----
      it.printf(400, 15, id(font_medium), BLUE, TextAlign::TOP_CENTER,
                "reTerminal E1002 Dashboard");
      it.line(20, 55, 780, 55, BLACK);

{{displayLambda}}

      // ---- Footer (BLUE) / 页脚（蓝色）----
      it.printf(400, 458, id(font_small), BLUE, TextAlign::TOP_CENTER,
                "Selected reTerminal hardware features enabled");`,
            },
          },
        },
      },
      {
        id: "buttons",
        label: "3 buttons",
        defaultChecked: true,
        description: "GPIO buttons with LED and buzzer feedback",
        requires: ["buzzer_led"],
        contributes: {
          binarySensors: [`  # Button 1 (green) — flash LED + short beep
  # 按钮 1（绿色）— LED 闪烁 + 短蜂鸣
  - platform: gpio
    pin:
      number: GPIO3
      mode: INPUT_PULLUP
      inverted: true
    id: button_1
    name: "Button 1 (Green)"
    on_press:
      then:
        - logger.log: "Button 1 (Green) pressed - flash LED + beep"
        - light.turn_on: onboard_led
        - light.turn_on:
            id: buzzer
            brightness: 50%
        - delay: 200ms
        - light.turn_off: buzzer
        - light.turn_off: onboard_led`, `  # Button 2 (right white, GPIO4) — deep sleep wake-up button
  # 按钮 2（右白，GPIO4）— 深度睡眠唤醒按钮

  # Button 3 (left white) — toggle LED on/off
  # 按钮 3（左白）— 切换 LED 开/关
  - platform: gpio
    pin:
      number: GPIO5
      mode: INPUT_PULLUP
      inverted: true
    id: button_3
    name: "Button 3 (Left White)"
    on_press:
      then:
        - logger.log: "Button 3 (Left White) pressed - toggle LED"
        - light.toggle: onboard_led`],
          displayLambda: `      // ---- Button hints / 按钮提示 ----
      it.line(20, 370, 780, 370);
      it.printf(30, 380, id(font_small), "BTN1(Green): LED flash + beep");
      it.printf(30, 405, id(font_small), "BTN2(Right): Deep sleep wake");
      it.printf(30, 430, id(font_small), "BTN3(Left):  Toggle LED on/off");`,
        },
        perDevice: {
          E1002: {
            contributes: {
              displayLambda: `      // ---- Button hints (BLACK) / 按钮提示（黑色）----
      it.line(20, 370, 780, 370, BLUE);
      it.printf(30, 380, id(font_small), BLACK, "BTN1(Green): LED flash + beep");
      it.printf(30, 405, id(font_small), BLACK, "BTN2(Right): Deep sleep wake");
      it.printf(30, 430, id(font_small), BLACK, "BTN3(Left):  Toggle LED on/off");`,
            },
          },
        },
      },
      {
        id: "buzzer_led",
        label: "Buzzer + onboard LED",
        defaultChecked: true,
        description: "Onboard active-low LED and PWM buzzer exposed as lights",
        requires: [],
        contributes: {
          outputs: [`  # [4] Onboard LED (active low) / 板载 LED（低电平有效）
  - platform: gpio
    pin: GPIO6
    id: bsp_led
    inverted: true`, `  # [3] Buzzer PWM output / 蜂鸣器 PWM 输出
  # 'ledc' is ESP32 hardware PWM / 'ledc' 是 ESP32 硬件 PWM 外设
  - platform: ledc
    pin: GPIO45
    id: buzzer_pwm
    frequency: 1000Hz`],
          lights: [`  - platform: binary
    name: "Onboard LED"
    output: bsp_led
    id: onboard_led`, `  # Buzzer wrapped as a "light": brightness = volume
  # 蜂鸣器包装成"灯"实体：亮度 = 音量
  - platform: monochromatic
    output: buzzer_pwm
    name: "Buzzer"
    id: buzzer
    default_transition_length: 0s`],
        },
      },
      {
        id: "battery",
        label: "Battery monitor",
        defaultChecked: true,
        description: "Battery measurement enable, ADC voltage, and percentage estimate",
        requires: [],
        contributes: {
          outputs: [`  # [5] Battery measurement enable / 电池测量使能
  - platform: gpio
    pin: GPIO21
    id: bsp_battery_enable`],
          sensors: [`  # --- Battery voltage (ADC on GPIO1) ---
  - platform: adc
    pin: GPIO1
    name: "Battery Voltage"
    id: battery_voltage
    update_interval: 60s
    attenuation: 12db
    filters:
      # Compensate for onboard voltage divider (1:2)
      # 补偿板载分压电阻（1:2 比例）
      - multiply: 2.0`, `  # --- Battery level (percentage) ---
  - platform: template
    name: "Battery Level"
    id: battery_level
    unit_of_measurement: "%"
    icon: "mdi:battery"
    device_class: battery
    state_class: measurement
    lambda: 'return id(battery_voltage).state;'
    update_interval: 60s
    filters:
      - calibrate_linear:
          - 4.15 -> 100.0
          - 3.96 -> 90.0
          - 3.91 -> 80.0
          - 3.85 -> 70.0
          - 3.80 -> 60.0
          - 3.75 -> 50.0
          - 3.68 -> 40.0
          - 3.58 -> 30.0
          - 3.49 -> 20.0
          - 3.41 -> 10.0
          - 3.30 -> 5.0
          - 3.27 -> 0.0
      - clamp:
          min_value: 0
          max_value: 100`],
          onBoot: [
            `      # Turn on battery measurement circuit / 打开电池测量电路
      - output.turn_on: bsp_battery_enable`,
            `      # Wait for voltages to stabilize / 等待电压稳定
      - delay: 200ms`,
            `      # Take first battery reading / 采集首次电池读数
      - component.update: battery_voltage
      - component.update: battery_level`,
          ],
          displayLambda: `      // ---- Battery / 电池 ----
      if (id(battery_level).has_state()) {
        ESP_LOGD("display", "Battery: %.0f%%  Voltage: %.2fV",
                 id(battery_level).state, id(battery_voltage).state);
        it.printf(30, 260, id(font_medium), "Battery: %.0f%%  (%.2fV)",
                  id(battery_level).state, id(battery_voltage).state);
      } else {
        it.printf(30, 260, id(font_medium), "Battery: --");
      }`,
        },
        perDevice: {
          E1002: {
            contributes: {
              displayLambda: `      // ---- Battery (YELLOW) / 电池（黄色）----
      if (id(battery_level).has_state()) {
        ESP_LOGD("display", "Battery: %.0f%%  Voltage: %.2fV",
                 id(battery_level).state, id(battery_voltage).state);
        it.printf(30, 260, id(font_medium), YELLOW, "Battery: %.0f%%  (%.2fV)",
                  id(battery_level).state, id(battery_voltage).state);
      } else {
        it.printf(30, 260, id(font_medium), YELLOW, "Battery: --");
      }`,
            },
          },
        },
      },
      {
        id: "temp_humidity",
        label: "SHT4x temperature + humidity",
        defaultChecked: true,
        description: "SHT4x temperature and humidity sensor on I2C",
        requires: [],
        contributes: {
          buses: ["i2c"],
          sensors: [`  # --- SHT4x temperature & humidity (I2C) ---
  - platform: sht4x
    temperature:
      name: "Temperature"
      id: temp_sensor
    humidity:
      name: "Relative Humidity"
      id: hum_sensor
    update_interval: 60s`],
          displayLambda: `      // ---- Temperature / 温度 ----
      if (id(temp_sensor).has_state()) {
        ESP_LOGD("display", "Temperature: %.1f C", id(temp_sensor).state);
        it.printf(30, 120, id(font_large), "Temp: %.1f C", id(temp_sensor).state);
      } else {
        ESP_LOGW("display", "Temperature: NO DATA");
        it.printf(30, 120, id(font_large), "Temp: -- C");
      }

      // ---- Humidity / 湿度 ----
      if (id(hum_sensor).has_state()) {
        ESP_LOGD("display", "Humidity: %.1f %%", id(hum_sensor).state);
        it.printf(30, 185, id(font_large), "Hum:  %.1f %%", id(hum_sensor).state);
      } else {
        ESP_LOGW("display", "Humidity: NO DATA");
        it.printf(30, 185, id(font_large), "Hum:  -- %%");
      }`,
        },
        perDevice: {
          E1002: {
            contributes: {
              displayLambda: `      // ---- Temperature (RED) / 温度（红色）----
      if (id(temp_sensor).has_state()) {
        ESP_LOGD("display", "Temperature: %.1f C", id(temp_sensor).state);
        it.printf(30, 120, id(font_large), RED, "Temp: %.1f C", id(temp_sensor).state);
      } else {
        ESP_LOGW("display", "Temperature: NO DATA");
        it.printf(30, 120, id(font_large), RED, "Temp: -- C");
      }

      // ---- Humidity (GREEN) / 湿度（绿色）----
      if (id(hum_sensor).has_state()) {
        ESP_LOGD("display", "Humidity: %.1f %%", id(hum_sensor).state);
        it.printf(30, 185, id(font_large), GREEN, "Hum:  %.1f %%", id(hum_sensor).state);
      } else {
        ESP_LOGW("display", "Humidity: NO DATA");
        it.printf(30, 185, id(font_large), GREEN, "Hum:  -- %%");
      }`,
            },
          },
        },
      },
      {
        id: "rtc",
        label: "PCF8563 RTC",
        defaultChecked: true,
        description: "Hardware RTC plus Home Assistant time sync",
        requires: [],
        contributes: {
          buses: ["i2c"],
          times: [`  # PCF8563 is at I2C address 0x51 with a CR1220 coin cell backup.
  # On boot we read the RTC; when HA syncs, we write back to the RTC.
  # PCF8563 在 I2C 地址 0x51，由 CR1220 纽扣电池备份。
  # 启动时读取 RTC；当 HA 同步时间后，回写到 RTC。
  - platform: pcf8563
    id: rtc_time
    address: 0x51
    # No periodic re-reads needed; HA sync handles it
    # 无需定期重读；HA 同步来处理
    update_interval: never`, `  - platform: homeassistant
    # When HA pushes a time sync, write it to the hardware RTC
    # 当 HA 推送时间同步时，写入硬件 RTC
    on_time_sync:
      then:
        - pcf8563.write_time:`],
          onBoot: [`      # Read RTC time on boot / 启动时读取 RTC 时间
      - pcf8563.read_time:`],
          displayLambda: `      // ---- RTC Time / RTC 时间 ----
      auto now = id(rtc_time).now();
      if (now.is_valid()) {
        it.strftime(400, 65, id(font_medium), TextAlign::TOP_CENTER,
                    "%Y-%m-%d  %H:%M", now);
        ESP_LOGD("display", "RTC time: %04d-%02d-%02d %02d:%02d",
                 now.year, now.month, now.day_of_month, now.hour, now.minute);
      } else {
        it.printf(400, 65, id(font_medium), TextAlign::TOP_CENTER,
                  "RTC: waiting for sync...");
        ESP_LOGW("display", "RTC: not synced yet");
      }`,
        },
        perDevice: {
          E1002: {
            contributes: {
              displayLambda: `      // ---- RTC Time (BLACK) / RTC 时间（黑色）----
      auto now = id(rtc_time).now();
      if (now.is_valid()) {
        it.strftime(400, 65, id(font_medium), BLACK, TextAlign::TOP_CENTER,
                    "%Y-%m-%d  %H:%M", now);
        ESP_LOGD("display", "RTC time: %04d-%02d-%02d %02d:%02d",
                 now.year, now.month, now.day_of_month, now.hour, now.minute);
      } else {
        it.printf(400, 65, id(font_medium), BLACK, TextAlign::TOP_CENTER,
                  "RTC: waiting for sync...");
        ESP_LOGW("display", "RTC: not synced yet");
      }`,
            },
          },
        },
      },
      {
        id: "microphone",
        label: "PDM microphone",
        defaultChecked: true,
        description: "Onboard PDM microphone power and I2S audio input",
        requires: [],
        contributes: {
          buses: ["i2s_audio"],
          outputs: [`  # [8] Microphone power enable (TPS22916 load switch)
  # 麦克风供电使能（TPS22916 负载开关）
  # Must be HIGH before recording / 录音前必须拉高
  - platform: gpio
    pin: GPIO38
    id: mic_power_enable`],
          blocks: [`# =============================================================================
# [8] PDM Microphone / PDM 麦克风
# =============================================================================
# The onboard PDM mic outputs a 1-bit sigma-delta stream.
# ESP32-S3 built-in PDM peripheral decodes it — no external codec needed.
# 板载 PDM 麦克风输出 1 位 sigma-delta 流。
# ESP32-S3 内置 PDM 外设解码 — 无需外部编解码器。
#
# In HA, this microphone can be used with Voice Assistant / Assist Pipeline.
# 在 HA 中，此麦克风可配合语音助手 / Assist 管道使用。

microphone:
  - platform: i2s_audio
    id: onboard_mic
    adc_type: external
    pdm: true
    i2s_din_pin: GPIO41`],
          onBoot: [`      # Turn on microphone power / 打开麦克风供电
      - output.turn_on: mic_power_enable`],
          displayLambda: `      // ---- Microphone status / 麦克风状态 ----
      it.printf(30, 335, id(font_small), "PDM Microphone: enabled (GPIO41/42)");`,
        },
        perDevice: {
          E1002: {
            contributes: {
              displayLambda: `      // ---- Microphone status (BLACK) / 麦克风状态（黑色）----
      it.printf(30, 335, id(font_small), BLACK, "PDM Microphone: enabled (GPIO41/42)");`,
            },
          },
        },
      },
      {
        id: "sd_detect",
        label: "MicroSD detect",
        defaultChecked: true,
        description: "MicroSD power enable and card-detect input",
        requires: [],
        contributes: {
          buses: ["spi"],
          outputs: [`  # [9] SD card power enable / SD 卡供电使能
  - platform: gpio
    pin: GPIO16
    id: bsp_sd_enable`],
          binarySensors: [`  # [9] SD card detect (LOW = card present)
  # SD 卡检测（低电平 = 卡已插入）
  #
  # NOTE on SD card in ESPHome:
  # ESPHome cannot directly access the SD card file system (read/write files,
  # create directories, etc.) the way Arduino can. In ESPHome, the SD card is
  # used indirectly by specific components:
  #   - online_image: caches downloaded images to SD card
  #   - media_player: plays audio files stored on the card
  #   - Works with PSRAM for large buffer allocation
  # For direct file operations (e.g. recording WAV files), use the Arduino framework.
  #
  # ESPHome 中 SD 卡的限制：
  # ESPHome 不能像 Arduino 那样直接操作 SD 卡文件系统（读写文件、创建目录等）。
  # SD 卡在 ESPHome 中由特定组件间接使用：
  #   - online_image：将下载的图片缓存到 SD 卡
  #   - media_player：播放卡上存储的音频文件
  #   - 配合 PSRAM 做大缓冲
  # 如需直接操作文件（如录音存 WAV），请使用 Arduino 框架。
  - platform: gpio
    pin:
      number: GPIO15
      mode: INPUT_PULLUP
      inverted: true
    id: sd_card_detect
    name: "SD Card Detected"`],
          onBoot: [`      # Turn on SD card power rail / 打开 SD 卡供电
      - output.turn_on: bsp_sd_enable`],
          displayLambda: `      // ---- SD card status / SD 卡状态 ----
      if (id(sd_card_detect).state) {
        it.printf(30, 305, id(font_small), "SD Card: inserted");
      } else {
        it.printf(30, 305, id(font_small), "SD Card: not detected");
      }`,
        },
        perDevice: {
          E1002: {
            contributes: {
              displayLambda: `      // ---- SD card status (BLACK) / SD 卡状态（黑色）----
      if (id(sd_card_detect).state) {
        it.printf(30, 305, id(font_small), BLACK, "SD Card: inserted");
      } else {
        it.printf(30, 305, id(font_small), BLACK, "SD Card: not detected");
      }`,
            },
          },
        },
      },
      {
        id: "deep_sleep",
        label: "Deep sleep",
        defaultChecked: true,
        description: "Deep sleep with GPIO4 ext1 wake-up",
        requires: [],
        contributes: {},
        perDevice: {
          E1001: {
            contributes: {
              deepSleep: `# =============================================================================
# [10] Deep sleep / 深度睡眠
# The device wakes for run_duration, updates sensors & display, then sleeps.
# Press the right white button (GPIO4) to wake manually at any time.
# 设备醒来 run_duration 时间后进入睡眠。按右白按钮可手动唤醒。
# =============================================================================

deep_sleep:
  id: deep_sleep_1
  run_duration: 30s
  sleep_duration: 5min
  # ESP32-S3 only supports ext1 wakeup (ext0 not available)
  # ESP32-S3 仅支持 ext1 唤醒（ext0 不可用）
  esp32_ext1_wakeup:
    pins:
      - number: GPIO4
        mode: INPUT_PULLUP
    mode: ANY_LOW`,
            },
          },
          E1002: {
            contributes: {
              deepSleep: `# =============================================================================
# [10] Deep sleep / 深度睡眠
# The device wakes for run_duration, updates sensors & display, then sleeps.
# Press the right white button (GPIO4) to wake manually at any time.
# Color ePaper full refresh takes 25-40s — run_duration must exceed that.
# 设备醒来 run_duration 时间后进入睡眠。按右白按钮可手动唤醒。
# 彩色墨水屏全刷需要 25-40 秒 — run_duration 必须大于此值。
# =============================================================================

deep_sleep:
  id: deep_sleep_1
  run_duration: 150s
  sleep_duration: 5min
  # ESP32-S3 only supports ext1 wakeup (ext0 not available)
  # ESP32-S3 仅支持 ext1 唤醒（ext0 不可用）
  esp32_ext1_wakeup:
    pins:
      - number: GPIO4
        mode: INPUT_PULLUP
    mode: ANY_LOW`,
            },
          },
        },
      },
    ],
    firmwareOptions: [],
  },
  {
    id: "trmnl",
    group: "official",
    name: "TRMNL",
    tagline: "Cloud-connected ePaper dashboard firmware for always-on information panels.",
    source: {
      label: "Official website",
      url: "https://trmnl.com/",
    },
    wiki: {
      label: "Wiki",
      url: "https://wiki.seeedstudio.com/reterminal_e10xx_trmnl/",
    },
    description:
      "TRMNL provides ready-to-run dashboard firmware for publishing calendar, task, metric, and plugin-based content to reTerminal ePaper displays.",
    logo: "assets/platforms/trmnl-logo.svg",
    preview: "assets/platforms/trmnl-preview.png",
    previewAlt: "TRMNL dashboard firmware preview",
    accent: "#3D3D3E",
    highlight: "#F8654B",
    supportedDevices: ["E1001", "E1002", "E1003"],
    installReady: true,
    bullets: [
      "Official TRMNL firmware workflow",
      "Web-flashable device builds",
      "Available for E1001, E1002, and E1003",
    ],
    versions: [
      {
        version: "1.8.7",
        label: "Stable",
      },
    ],
    configFields: [],
    firmwareOptions: [
      {
        id: "TRMNL_reTerminal_E1001",
        name: "TRMNL Firmware",
        description: "Install TRMNL dashboard firmware for reTerminal E1001.",
        category: "Dashboard",
        compatible: ["E1001"],
        defaultVersion: "1.8.7",
        notes: [
          { type: "info", text: "After flashing, follow the TRMNL setup flow to connect Wi-Fi and pair the device with your TRMNL account." },
        ],
      },
      {
        id: "TRMNL_reTerminal_E1002",
        name: "TRMNL Firmware",
        description: "Install TRMNL dashboard firmware for reTerminal E1002.",
        category: "Dashboard",
        compatible: ["E1002"],
        defaultVersion: "1.8.7",
        notes: [
          { type: "info", text: "After flashing, follow the TRMNL setup flow to connect Wi-Fi and pair the device with your TRMNL account." },
        ],
      },
      {
        id: "TRMNL_reTerminal_E1003",
        name: "TRMNL Firmware",
        description: "Install TRMNL dashboard firmware for reTerminal E1003.",
        category: "Dashboard",
        compatible: ["E1003"],
        defaultVersion: "1.8.7",
        notes: [
          { type: "info", text: "After flashing, follow the TRMNL setup flow to connect Wi-Fi and pair the device with your TRMNL account." },
        ],
      },
    ],
  },
  {
    id: "eezstudio",
    group: "official",
    name: "EEZ Studio",
    tagline: "Visual LVGL UI design and code generation for ePaper displays.",
    source: {
      label: "Official website",
      url: "https://www.envox.eu/studio/studio-introduction/",
    },
    wiki: {
      label: "Wiki",
      url: "https://wiki.seeedstudio.com/reterminal_e10xx_with_eezstudio/",
    },
    description:
      "EEZ Studio provides a visual drag-and-drop environment for designing LVGL-based UIs. Export your screen layout as C code, drop it into the project template, and compile with PlatformIO.",
    logo: "assets/platforms/eezstudio-logo.png",
    preview: "assets/platforms/eezstudio-preview.jpg",
    previewAlt: "EEZ Studio EcoLife dashboard on reTerminal ePaper display",
    accent: "#004966",
    highlight: "#00AEEF",
    supportedDevices: ["E1001", "E1002", "E1003", "E1004"],
    installReady: false,
    downloadMode: true,
    downloadUrl: "downloads/EEZStudio.zip",
    downloadSteps: [
      {
        title: "Download project template",
        description: "Click the button below to download the PlatformIO project template pre-configured for {deviceName}.",
      },
      {
        title: "Design your UI in EEZ Studio",
        description: "Open EEZ Studio, design your screen layout with widgets and images, then export the project as LVGL v9 C source code.",
      },
      {
        title: "Replace the UI folder",
        description: "Copy the exported files into the project's src/ui/ directory, replacing the default Hello World example.",
      },
      {
        title: "Compile and flash with PlatformIO",
        description: "Open the project in VS Code with PlatformIO, select the {deviceId} environment, click Build, then Upload to flash your custom UI.",
      },
    ],
    bullets: [
      "Visual drag-and-drop LVGL v9 UI editor",
      "Export C code directly into the project",
      "PlatformIO build workflow for all E-series devices",
    ],
    versions: [],
    configFields: [],
    firmwareOptions: [],
  },
  {
    id: "lvgl-epaper-status-panel",
    group: "official",
    name: "LVGL",
    tagline: "Ready-to-run LVGL status dashboard for reTerminal ePaper displays.",
    source: {
      label: "Official website",
      url: "https://lvgl.io/",
    },
    wiki: {
      label: "Wiki",
      url: "https://wiki.seeedstudio.com/epaper_work_with_lvgl",
    },
    description:
      "LVGL ePaper Status Panel renders a static device, network, and battery dashboard through LVGL 9 and the Seeed_GFX ePaper driver.",
    logo: "assets/platforms/lvgl-logo-colored.png",
    preview: "assets/platforms/lvgl-epaper-status-panel-preview.jpg",
    previewAlt: "LVGL ePaper status panel running on a reTerminal display",
    accent: "#353A40",
    highlight: "#4466F2",
    supportedDevices: ["E1001", "E1002", "E1003", "E1004"],
    detailTags: ["LVGL 9.5.0"],
    installReady: true,
    bullets: [
      "Static LVGL 9 status panel",
      "Seeed_GFX ePaper display pipeline",
      "Web-flashable builds for every E-Series display",
    ],
    versions: [
      {
        version: "1.0.0",
        label: "Stable",
      },
    ],
    configFields: [],
    firmwareOptions: [
      {
        id: "LVGL_StatusPanel_E1001",
        name: "LVGL Status Panel",
        description: "Install the LVGL status panel firmware for reTerminal E1001.",
        category: "Dashboard",
        compatible: ["E1001"],
        notes: [
          { type: "info", text: "After flashing, the device renders a static LVGL dashboard with device, network, and battery demo panels." },
        ],
      },
      {
        id: "LVGL_StatusPanel_E1002",
        name: "LVGL Status Panel",
        description: "Install the LVGL status panel firmware for reTerminal E1002.",
        category: "Dashboard",
        compatible: ["E1002"],
        notes: [
          { type: "info", text: "After flashing, the device renders a static LVGL dashboard with device, network, and battery demo panels." },
        ],
      },
      {
        id: "LVGL_StatusPanel_E1003",
        name: "LVGL Status Panel",
        description: "Install the LVGL status panel firmware for reTerminal E1003.",
        category: "Dashboard",
        compatible: ["E1003"],
        notes: [
          { type: "info", text: "After flashing, the device renders a static LVGL dashboard with device, network, and battery demo panels." },
        ],
      },
      {
        id: "LVGL_StatusPanel_E1004",
        name: "LVGL Status Panel",
        description: "Install the LVGL status panel firmware for reTerminal E1004.",
        category: "Dashboard",
        compatible: ["E1004"],
        notes: [
          { type: "info", text: "After flashing, the device renders a static LVGL dashboard with device, network, and battery demo panels." },
        ],
      },
    ],
  },
  {
    id: "zephyr",
    group: "official",
    name: "Zephyr",
    tagline: "RTOS board support for local firmware development.",
    source: {
      label: "Official website",
      url: "https://www.zephyrproject.org/",
    },
    wiki: {
      label: "Wiki",
      url: "https://wiki.seeedstudio.com/epaper_work_with_zephyr/",
    },
    description:
      "Zephyr provides upstream board documentation for reTerminal E1001, E1002, and E1003, including west-based build, flash, and monitor commands for ESP32-S3 development.",
    externalTool: {
      stepTitle: "Official docs",
      label: "Open Zephyr Board Docs",
      url: "https://docs.zephyrproject.org/latest/boards/seeed/reterminal_e1001/doc/index.html",
      urlsByDevice: {
        E1001: "https://docs.zephyrproject.org/latest/boards/seeed/reterminal_e1001/doc/index.html",
        E1002: "https://docs.zephyrproject.org/latest/boards/seeed/reterminal_e1002/doc/index.html",
        E1003: "https://docs.zephyrproject.org/latest/boards/seeed/reterminal_e1003/doc/index.html",
      },
      title: "Use the official Zephyr board documentation",
      description: "Open the selected board's Zephyr documentation to set up the west workspace, build samples, flash firmware, and monitor logs with the official Zephyr workflow.",
    },
    logo: "assets/platforms/zephyr-logo-card.svg",
    preview: "assets/platforms/zephyr-preview.svg",
    previewAlt: "Zephyr board documentation and west workflow preview",
    accent: "#1B6CA8",
    highlight: "#5DBB63",
    supportedDevices: ["E1001", "E1002", "E1003"],
    detailTags: ["Zephyr RTOS"],
    installReady: false,
    bullets: [
      "Official upstream board documentation",
      "Local west build and flash workflow",
      "RTOS development path for ESP32-S3 projects",
    ],
    versions: [],
    configFields: [],
    firmwareOptions: [],
  },
  {
    id: "squareline",
    group: "official",
    name: "SquareLine Vision",
    tagline: "Visual UI workflow for embedded screen experiences.",
    source: {
      label: "Official website",
      url: "https://app.vision.squareline.io/",
    },
    wiki: {
      label: "Wiki",
      url: "https://wiki.seeedstudio.com/reterminal_e10xx_with_squareline_vision/",
    },
    description:
      "SquareLine Vision focuses on designing embedded interfaces visually, then preparing UI assets and implementation output for screen-based products.",
    logo: "assets/platforms/squareline-logo.png",
    preview: "assets/platforms/squareline-preview-43.png",
    previewAlt: "SquareLine Vision product preview",
    accent: "#004966",
    highlight: "#8FC31F",
    supportedDevices: ["E1002", "E1003"],
    installReady: false,
    bullets: [
      "Visual UI design workflow",
      "Useful for screen-first product demos",
      "Best with display-capable devices",
    ],
    versions: [
      {
        version: "1.3.0",
        label: "Preview",
      },
    ],
    configFields: [],
    firmwareOptions: [],
  },
  {
    id: "opendisplay",
    group: "official",
    name: "OpenDisplay",
    tagline: "Open-source ePaper firmware and protocol for BLE display control.",
    source: {
      label: "Official website",
      url: "https://www.opendisplay.org/",
    },
    wiki: {
      label: "Wiki",
      url: "https://wiki.seeedstudio.com/EN04_opendisplay/",
    },
    description:
      "OpenDisplay provides firmware and browser tools for low-power ePaper display projects, including BLE control, configuration, and image upload flows.",
    externalTool: {
      label: "Open OpenDisplay Toolbox",
      url: "https://opendisplay.org/firmware/toolbox/index.html",
      title: "Use the official OpenDisplay toolbox",
      description: "OpenDisplay firmware and browser tools are maintained by the OpenDisplay project. Continue there to install firmware, configure BLE workflows, and upload images.",
    },
    logo: "assets/platforms/opendisplay-logo-card-v2.png",
    preview: "assets/platforms/opendisplay-screen-enhanced.png",
    previewAlt: "OpenDisplay browser upload flow",
    accent: "#004966",
    highlight: "#8FC31F",
    supportedDevices: ["E1001", "E1002", "E1003"],
    installReady: false,
    bullets: [
      "BLE-oriented ePaper control",
      "Browser-based configuration and image upload",
      "Good for always-on information panels",
    ],
    versions: [
      {
        version: "0.1.0",
        label: "Preview",
      },
    ],
    configFields: [],
    firmwareOptions: [],
  },
  {
    id: "voice-memo-reminder",
    group: "community",
    name: "Voice Memo Reminder",
    tagline: "Community AI voice memo project for ePaper reminder lists.",
    author: "limengdu",
    source: {
      label: "limengdu/ePaper-Voice-Memo",
      url: "https://github.com/limengdu/ePaper-Voice-Memo",
    },
    description:
      "Voice Memo Reminder records a spoken memo, sends it to Groq Whisper and Llama 3.3, then turns it into a sorted ePaper reminder list.",
    logo: "assets/brand/reterminal-epaper-icon.svg",
    preview: "assets/devices/reterminal-e1003.jpg",
    previewAlt: "reTerminal E1003 running an ePaper reminder workflow",
    accent: "#004966",
    highlight: "#8FC31F",
    supportedDevices: ["E1001", "E1002", "E1003"],
    installReady: true,
    bullets: [
      "Community project",
      "Voice-to-reminder workflow",
      "Requires Wi-Fi and a Groq API key",
    ],
    versions: [
      {
        version: "1.0.0",
        label: "Stable",
      },
    ],
    configFields: [],
    firmwareOptions: [
      {
        id: "ePaper_VoiceMemo_E1001",
        name: "Voice Memo Reminder",
        baseName: "Voice Memo Reminder",
        variantGroup: "ePaper_VoiceMemo_E1001",
        language: "en",
        languageLabel: "English",
        description: "AI-powered voice memo to ePaper reminder list. Hold KEY0 to record, release to create a sorted to-do item via Groq Whisper + Llama 3.3.",
        category: "Application",
        compatible: ["E1001"],
        configFields: [
          { id: "cfgWifiSsid", nvsKey: "wifiSsid", nvsType: "string", label: "Wi-Fi SSID", type: "text", defaultValue: "", placeholder: "Your Wi-Fi network name" },
          { id: "cfgWifiPass", nvsKey: "wifiPass", nvsType: "string", label: "Wi-Fi Password", type: "text", defaultValue: "", placeholder: "Your Wi-Fi password" },
          { id: "cfgApiKey", nvsKey: "apiKey", nvsType: "string", label: "Groq API Key", type: "text", defaultValue: "", placeholder: "gsk_..." },
        ],
        notes: [
          { type: "warning", text: "Requires a free Groq API key. Get one at https://console.groq.com — the free tier includes Whisper and Llama 3.3." },
          { type: "info", text: "Hold KEY0 to record (up to 20s), release to process. Reminders persist across reboots via NVS. The screen auto-refreshes every 5 minutes." },
        ],
      },
      {
        id: "ePaper_VoiceMemo_E1001_ZH",
        name: "Voice Memo Reminder (Chinese)",
        baseName: "Voice Memo Reminder",
        variantGroup: "ePaper_VoiceMemo_E1001",
        language: "zh",
        languageLabel: "Simplified Chinese",
        description: "Chinese language voice memo reminder with embedded TrueType font rendering. Hold KEY0 to record in Chinese.",
        category: "Application",
        compatible: ["E1001"],
        configFields: [
          { id: "cfgWifiSsid", nvsKey: "wifiSsid", nvsType: "string", label: "Wi-Fi SSID", type: "text", defaultValue: "", placeholder: "Your Wi-Fi network name" },
          { id: "cfgWifiPass", nvsKey: "wifiPass", nvsType: "string", label: "Wi-Fi Password", type: "text", defaultValue: "", placeholder: "Your Wi-Fi password" },
          { id: "cfgApiKey", nvsKey: "apiKey", nvsType: "string", label: "Groq API Key", type: "text", defaultValue: "", placeholder: "gsk_..." },
        ],
        notes: [
          { type: "warning", text: "Requires a free Groq API key. Get one at https://console.groq.com — the free tier includes Whisper and Llama 3.3." },
          { type: "info", text: "This firmware includes an embedded Chinese TrueType font. The E1001 layout uses a compact 4-card page without touch controls." },
        ],
      },
      {
        id: "ePaper_VoiceMemo_E1002",
        name: "Voice Memo Reminder",
        baseName: "Voice Memo Reminder",
        variantGroup: "ePaper_VoiceMemo_E1002",
        language: "en",
        languageLabel: "English",
        description: "AI-powered voice memo to ePaper reminder list. Hold KEY0 to record, release to create a sorted to-do item via Groq Whisper + Llama 3.3.",
        category: "Application",
        compatible: ["E1002"],
        configFields: [
          { id: "cfgWifiSsid", nvsKey: "wifiSsid", nvsType: "string", label: "Wi-Fi SSID", type: "text", defaultValue: "", placeholder: "Your Wi-Fi network name" },
          { id: "cfgWifiPass", nvsKey: "wifiPass", nvsType: "string", label: "Wi-Fi Password", type: "text", defaultValue: "", placeholder: "Your Wi-Fi password" },
          { id: "cfgApiKey", nvsKey: "apiKey", nvsType: "string", label: "Groq API Key", type: "text", defaultValue: "", placeholder: "gsk_..." },
        ],
        notes: [
          { type: "warning", text: "Requires a free Groq API key. Get one at https://console.groq.com — the free tier includes Whisper and Llama 3.3." },
          { type: "info", text: "Hold KEY0 to record (up to 20s), release to process. Reminders persist across reboots via NVS. The screen auto-refreshes every 5 minutes." },
        ],
      },
      {
        id: "ePaper_VoiceMemo_E1002_ZH",
        name: "Voice Memo Reminder (Chinese)",
        baseName: "Voice Memo Reminder",
        variantGroup: "ePaper_VoiceMemo_E1002",
        language: "zh",
        languageLabel: "Simplified Chinese",
        description: "Chinese language voice memo reminder with embedded TrueType font rendering. Hold KEY0 to record in Chinese.",
        category: "Application",
        compatible: ["E1002"],
        configFields: [
          { id: "cfgWifiSsid", nvsKey: "wifiSsid", nvsType: "string", label: "Wi-Fi SSID", type: "text", defaultValue: "", placeholder: "Your Wi-Fi network name" },
          { id: "cfgWifiPass", nvsKey: "wifiPass", nvsType: "string", label: "Wi-Fi Password", type: "text", defaultValue: "", placeholder: "Your Wi-Fi password" },
          { id: "cfgApiKey", nvsKey: "apiKey", nvsType: "string", label: "Groq API Key", type: "text", defaultValue: "", placeholder: "gsk_..." },
        ],
        notes: [
          { type: "warning", text: "Requires a free Groq API key. Get one at https://console.groq.com — the free tier includes Whisper and Llama 3.3." },
          { type: "info", text: "This firmware includes an embedded Chinese TrueType font. The E1002 layout uses a compact 4-card color page without touch controls." },
        ],
      },
      {
        id: "ePaper_VoiceMemo_E1003",
        name: "Voice Memo Reminder",
        baseName: "Voice Memo Reminder",
        variantGroup: "ePaper_VoiceMemo_E1003",
        language: "en",
        languageLabel: "English",
        description: "AI-powered voice memo to ePaper reminder list with touch checkboxes. Hold KEY0 to record, tap checkboxes to mark items done.",
        category: "Application",
        compatible: ["E1003"],
        configFields: [
          { id: "cfgWifiSsid", nvsKey: "wifiSsid", nvsType: "string", label: "Wi-Fi SSID", type: "text", defaultValue: "", placeholder: "Your Wi-Fi network name" },
          { id: "cfgWifiPass", nvsKey: "wifiPass", nvsType: "string", label: "Wi-Fi Password", type: "text", defaultValue: "", placeholder: "Your Wi-Fi password" },
          { id: "cfgApiKey", nvsKey: "apiKey", nvsType: "string", label: "Groq API Key", type: "text", defaultValue: "", placeholder: "gsk_..." },
        ],
        notes: [
          { type: "warning", text: "Requires a free Groq API key. Get one at https://console.groq.com — the free tier includes Whisper and Llama 3.3." },
          { type: "info", text: "Hold KEY0 to record (up to 20s), release to process. Tap a checkbox to mark it done. Reminders persist across reboots. The screen auto-refreshes every 5 minutes." },
        ],
      },
      {
        id: "ePaper_VoiceMemo_E1003_ZH",
        name: "Voice Memo Reminder (Chinese)",
        baseName: "Voice Memo Reminder",
        variantGroup: "ePaper_VoiceMemo_E1003",
        language: "zh",
        languageLabel: "Simplified Chinese",
        description: "Chinese language voice memo reminder with embedded TrueType font rendering. Hold KEY0 to record in Chinese, tap checkboxes to mark items done.",
        category: "Application",
        compatible: ["E1003"],
        configFields: [
          { id: "cfgWifiSsid", nvsKey: "wifiSsid", nvsType: "string", label: "Wi-Fi SSID", type: "text", defaultValue: "", placeholder: "Your Wi-Fi network name" },
          { id: "cfgWifiPass", nvsKey: "wifiPass", nvsType: "string", label: "Wi-Fi Password", type: "text", defaultValue: "", placeholder: "Your Wi-Fi password" },
          { id: "cfgApiKey", nvsKey: "apiKey", nvsType: "string", label: "Groq API Key", type: "text", defaultValue: "", placeholder: "gsk_..." },
        ],
        notes: [
          { type: "warning", text: "Requires a free Groq API key. Get one at https://console.groq.com — the free tier includes Whisper and Llama 3.3." },
          { type: "info", text: "This firmware includes an embedded Chinese TrueType font (~800KB). It renders Chinese text natively via OpenFontRender." },
        ],
      },
    ],
  },
  {
    id: "photoframe",
    group: "community",
    name: "ESP32 PhotoFrame",
    tagline: "Community ePaper photo frame firmware with measured-palette image quality and a web UI.",
    author: "aitjcize",
    source: {
      label: "aitjcize/esp32-photoframe",
      url: "https://github.com/aitjcize/esp32-photoframe",
    },
    description:
      "ESP32 PhotoFrame replaces the stock firmware with a measured-palette dithering pipeline, a drag-and-drop web interface, deep-sleep power management, SD card and URL image sources, and Home Assistant integration.",
    logo: "assets/brand/reterminal-epaper-icon.svg",
    preview: "assets/platforms/photoframe-preview.png",
    previewAlt: "ESP32 PhotoFrame web interface running on a reTerminal ePaper display",
    accent: "#5A3A1E",
    highlight: "#C8843C",
    supportedDevices: ["E1002", "E1004"],
    installReady: true,
    bullets: [
      "Measured-palette dithering for accurate ePaper color",
      "Drag-and-drop web UI, REST API, and Home Assistant integration",
      "Deep-sleep power management with SD card and URL image sources",
    ],
    versions: [
      {
        version: "2.8.0",
        label: "Stable",
      },
    ],
    configFields: [],
    firmwareOptions: [
      {
        id: "PhotoFrame_reTerminal_E1002",
        name: "ESP32 PhotoFrame",
        description: "Full-color photo frame firmware for the reTerminal E1002 (7.3\" Spectra 6). Includes the web interface, REST API, and Home Assistant integration.",
        category: "Application",
        compatible: ["E1002"],
        defaultVersion: "2.8.0",
        recommendedInstallMode: "erase",
        flashNotes: [
          { type: "warning", text: "This replaces the stock firmware and rewrites the partition table. Use the \"Erase + flash\" mode so the full-chip image at 0x0 writes over the stock layout cleanly." },
        ],
        notes: [
          { type: "info", text: "After flashing, the device starts a Wi-Fi setup portal. Join it (or use the companion app) to connect your network, then open the web interface to upload images and configure auto-rotation." },
        ],
      },
      {
        id: "PhotoFrame_reTerminal_E1004",
        name: "ESP32 PhotoFrame",
        description: "Full-color photo frame firmware for the reTerminal E1004 (13.3\" Spectra 6). Includes the web interface, REST API, and Home Assistant integration.",
        category: "Application",
        compatible: ["E1004"],
        defaultVersion: "2.8.0",
        recommendedInstallMode: "erase",
        flashNotes: [
          { type: "warning", text: "This replaces the stock firmware and rewrites the partition table. Use the \"Erase + flash\" mode so the full-chip image at 0x0 writes over the stock layout cleanly." },
        ],
        notes: [
          { type: "info", text: "After flashing, the device starts a Wi-Fi setup portal. Join it (or use the companion app) to connect your network, then open the web interface to upload images and configure auto-rotation." },
        ],
      },
    ],
  },
];

if (typeof module !== "undefined") {
  module.exports = { DEVICES, PLATFORM_GROUPS, PLATFORM_CARDS };
}
