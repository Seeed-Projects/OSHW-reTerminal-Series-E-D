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
      },
      E1002: {
        deviceName: "reterminal-e1002",
        friendlyName: "reTerminal_E1002",
        apSsid: "reTerminal-E1002",
      },
    },
    templateHeader: `# ESPHome configuration for Seeed reTerminal E-Series
# Generated by reTerminal Firmware Tool
# Docs: https://esphome.io/

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

logger:
  hardware_uart: UART0

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
      spi: `spi:
  clk_pin: GPIO7
  mosi_pin: GPIO9
  miso_pin: GPIO8`,
      i2c: `i2c:
  scl: GPIO20
  sda: GPIO19`,
      i2s_audio: `i2s_audio:
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
          blocks: [`ota:
  - platform: esphome
    password: !secret ota_password`, `wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
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
          blocks: [`api:
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
              display: `display:
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
      ESP_LOGD("display", "=== ePaper display refresh ===");

      it.printf(400, 15, id(font_medium), TextAlign::TOP_CENTER,
                "reTerminal E1001 Dashboard");
      it.line(20, 55, 780, 55);

{{displayLambda}}

      it.printf(400, 458, id(font_small), TextAlign::TOP_CENTER,
                "Selected reTerminal hardware features enabled");`,
            },
          },
          E1002: {
            contributes: {
              display: `display:
  - platform: epaper_spi
    id: epaper_display
    model: Seeed-reTerminal-E1002
    update_interval: 300s
    lambda: |-
      const auto BLACK   = Color(0,   0,   0,   0);
      const auto RED     = Color(255, 0,   0,   0);
      const auto GREEN   = Color(0,   255, 0,   0);
      const auto BLUE    = Color(0,   0,   255, 0);
      const auto YELLOW  = Color(255, 255, 0,   0);

      ESP_LOGD("display", "=== ePaper display refresh ===");

      it.printf(400, 15, id(font_medium), BLUE, TextAlign::TOP_CENTER,
                "reTerminal E1002 Dashboard");
      it.line(20, 55, 780, 55, BLACK);

{{displayLambda}}

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
          binarySensors: [`  - platform: gpio
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
        - light.turn_off: onboard_led`, `  - platform: gpio
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
          displayLambda: `      it.line(20, 370, 780, 370);
      it.printf(30, 380, id(font_small), "BTN1(Green): LED flash + beep");
      it.printf(30, 405, id(font_small), "BTN2(Right): Deep sleep wake");
      it.printf(30, 430, id(font_small), "BTN3(Left):  Toggle LED on/off");`,
        },
        perDevice: {
          E1002: {
            contributes: {
              displayLambda: `      it.line(20, 370, 780, 370, BLUE);
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
          outputs: [`  - platform: gpio
    pin: GPIO6
    id: bsp_led
    inverted: true`, `  - platform: ledc
    pin: GPIO45
    id: buzzer_pwm
    frequency: 1000Hz`],
          lights: [`  - platform: binary
    name: "Onboard LED"
    output: bsp_led
    id: onboard_led`, `  - platform: monochromatic
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
          outputs: [`  - platform: gpio
    pin: GPIO21
    id: bsp_battery_enable`],
          sensors: [`  - platform: adc
    pin: GPIO1
    name: "Battery Voltage"
    id: battery_voltage
    update_interval: 60s
    attenuation: 12db
    filters:
      - multiply: 2.0`, `  - platform: template
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
          onBoot: [`      - output.turn_on: bsp_battery_enable
      - delay: 200ms
      - component.update: battery_voltage
      - component.update: battery_level`],
          displayLambda: `      if (id(battery_level).has_state()) {
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
              displayLambda: `      if (id(battery_level).has_state()) {
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
          sensors: [`  - platform: sht4x
    temperature:
      name: "Temperature"
      id: temp_sensor
    humidity:
      name: "Relative Humidity"
      id: hum_sensor
    update_interval: 60s`],
          displayLambda: `      if (id(temp_sensor).has_state()) {
        ESP_LOGD("display", "Temperature: %.1f C", id(temp_sensor).state);
        it.printf(30, 120, id(font_large), "Temp: %.1f C", id(temp_sensor).state);
      } else {
        ESP_LOGW("display", "Temperature: NO DATA");
        it.printf(30, 120, id(font_large), "Temp: -- C");
      }

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
              displayLambda: `      if (id(temp_sensor).has_state()) {
        ESP_LOGD("display", "Temperature: %.1f C", id(temp_sensor).state);
        it.printf(30, 120, id(font_large), RED, "Temp: %.1f C", id(temp_sensor).state);
      } else {
        ESP_LOGW("display", "Temperature: NO DATA");
        it.printf(30, 120, id(font_large), RED, "Temp: -- C");
      }

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
          times: [`  - platform: pcf8563
    id: rtc_time
    address: 0x51
    update_interval: never`, `  - platform: homeassistant
    on_time_sync:
      then:
        - pcf8563.write_time:`],
          onBoot: [`      - pcf8563.read_time:`],
          displayLambda: `      auto now = id(rtc_time).now();
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
              displayLambda: `      auto now = id(rtc_time).now();
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
          outputs: [`  - platform: gpio
    pin: GPIO38
    id: mic_power_enable`],
          blocks: [`microphone:
  - platform: i2s_audio
    id: onboard_mic
    adc_type: external
    pdm: true
    i2s_din_pin: GPIO41`],
          onBoot: [`      - output.turn_on: mic_power_enable`],
          displayLambda: `      it.printf(30, 335, id(font_small), "PDM Microphone: enabled (GPIO41/42)");`,
        },
        perDevice: {
          E1002: {
            contributes: {
              displayLambda: `      it.printf(30, 335, id(font_small), BLACK, "PDM Microphone: enabled (GPIO41/42)");`,
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
          outputs: [`  - platform: gpio
    pin: GPIO16
    id: bsp_sd_enable`],
          binarySensors: [`  - platform: gpio
    pin:
      number: GPIO15
      mode: INPUT_PULLUP
      inverted: true
    id: sd_card_detect
    name: "SD Card Detected"`],
          onBoot: [`      - output.turn_on: bsp_sd_enable`],
          displayLambda: `      if (id(sd_card_detect).state) {
        it.printf(30, 305, id(font_small), "SD Card: inserted");
      } else {
        it.printf(30, 305, id(font_small), "SD Card: not detected");
      }`,
        },
        perDevice: {
          E1002: {
            contributes: {
              displayLambda: `      if (id(sd_card_detect).state) {
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
              deepSleep: `deep_sleep:
  id: deep_sleep_1
  run_duration: 30s
  sleep_duration: 5min
  esp32_ext1_wakeup:
    pins:
      - number: GPIO4
        mode: INPUT_PULLUP
    mode: ANY_LOW`,
            },
          },
          E1002: {
            contributes: {
              deepSleep: `deep_sleep:
  id: deep_sleep_1
  run_duration: 150s
  sleep_duration: 5min
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
    id: "squareline",
    group: "official",
    name: "SquareLine Vision",
    tagline: "Visual UI workflow for embedded screen experiences.",
    source: {
      label: "Official website",
      url: "https://app.vision.squareline.io/",
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
    description:
      "OpenDisplay provides firmware and browser tools for low-power ePaper display projects, including BLE control, configuration, and image upload flows.",
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
];

if (typeof module !== "undefined") {
  module.exports = { DEVICES, PLATFORM_GROUPS, PLATFORM_CARDS };
}
