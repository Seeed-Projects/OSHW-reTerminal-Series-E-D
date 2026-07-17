<a id="readme-top"></a>

<!-- Shields -->
[![Contributors][contributors-shield]][contributors-url]
[![Forks][forks-shield]][forks-url]
[![Stargazers][stars-shield]][stars-url]
[![Issues][issues-shield]][issues-url]
[![MIT License][license-shield]][license-url]

<!-- Logo & Title -->
<br />
<div align="center">
  <a href="https://github.com/Seeed-Projects/OSHW-reTerminal-Series-E-D">
    <img src="web/assets/brand/reterminal-epaper-icon.svg" alt="Logo" width="80" height="80">
  </a>

  <h2 align="center">reTerminal E-Series Firmware Hub</h2>

  <p align="center">
    Flash ESP32-S3 firmware to your reTerminal E-Series ePaper device directly from the browser — no IDE setup required.
    <br />
    <br />
    <a href="https://seeed-projects.github.io/OSHW-reTerminal-Series-E-D/"><strong>Open Firmware Hub »</strong></a>
    <br />
    <br />
    <a href="https://wiki.seeedstudio.com/reterminal_e10xx_main_page/">Wiki</a>
    &nbsp;·&nbsp;
    <a href="https://github.com/Seeed-Projects/OSHW-reTerminal-Series-E-D/issues/new?labels=bug">Report Bug</a>
    &nbsp;·&nbsp;
    <a href="https://github.com/Seeed-Projects/OSHW-reTerminal-Series-E-D/issues/new?labels=enhancement">Request Feature</a>
  </p>
</div>

<!-- Table of Contents -->
<details>
  <summary>Table of Contents</summary>
  <ol>
    <li><a href="#about-the-project">About The Project</a></li>
    <li><a href="#supported-hardware">Supported Hardware</a></li>
    <li><a href="#firmware-platforms">Firmware Platforms</a></li>
    <li><a href="#built-with">Built With</a></li>
    <li>
      <a href="#getting-started">Getting Started</a>
      <ul>
        <li><a href="#flash-from-browser">Flash from Browser</a></li>
        <li><a href="#local-development">Local Development</a></li>
        <li><a href="#build-firmware-locally">Build Firmware Locally</a></li>
      </ul>
    </li>
    <li><a href="#firmware-examples">Firmware Examples</a></li>
    <li><a href="#project-structure">Project Structure</a></li>
    <li><a href="#cicd">CI/CD</a></li>
    <li><a href="#roadmap">Roadmap</a></li>
    <li><a href="#contributing">Contributing</a></li>
    <li><a href="#license">License</a></li>
    <li><a href="#contact">Contact</a></li>
    <li><a href="#acknowledgments">Acknowledgments</a></li>
  </ol>
</details>

---

## About The Project

<!-- Replace with an actual screenshot of the Firmware Hub -->
<!-- ![Firmware Hub Screenshot](docs/images/screenshot.png) -->

The **reTerminal E-Series Firmware Hub** is a browser-based tool that lets you flash firmware, export templates, download project templates, or open official platform tools for Seeed Studio's reTerminal E-Series ePaper devices in a focused guided flow:

1. **Select a platform** — choose from official platforms, Base demos, or community projects
2. **Configure the workflow** — pick a device, firmware version, template options, install mode, downloadable project template, or official toolbox
3. **Flash, export, build locally, or continue with the official tool** — write firmware over USB, export an ESPHome YAML file, download a PlatformIO project template, or open an upstream platform toolbox

A built-in **serial monitor** lets you view real-time device logs, choose the baud rate, pause the visible stream, and save the retained recent log without leaving the page.

Browser-flashable workflows run directly from the page. Template and download workflows provide the files needed for local customization and builds. Official tool workflows link to the upstream platform's maintained firmware, documentation, or toolbox page.

Official platform cards include direct Seeed Studio Wiki shortcuts for platform-specific setup tutorials where available.

<p align="right">(<a href="#readme-top">back to top</a>)</p>

## Supported Hardware

All devices are built around the **XIAO ESP32-S3** with 8 MB flash and OPI PSRAM.

| Model | Display | Size | Color | Highlights |
|:------|:--------|:-----|:------|:-----------|
| **E1001** | ePaper | 7.5″ | Monochrome | 4-level grayscale |
| **E1002** | ePaper | 7.3″ | Full color | E Ink Spectra 6 |
| **E1003** | ePaper + Touch | 10.3″ | Grayscale | 16-level grayscale, capacitive touch |
| **E1004** | ePaper | 13.3″ | Full color | E Ink Spectra 6, largest display |

All models include a **PCF8563 RTC**, **MicroSD slot**, and **deep sleep support** (~14 µA). E1001–E1003 include a PDM microphone.

> For full hardware specs, see the [reTerminal E-Series Wiki](https://wiki.seeedstudio.com/reterminal_e10xx_main_page/).

<p align="right">(<a href="#readme-top">back to top</a>)</p>

## Firmware Platforms

| Platform | Status | Devices | Description |
|:---------|:-------|:--------|:------------|
| **Base** | ✅ Ready | E1001 – E1004 | Smoke-test demos (RTC, deep sleep, mic recording, touch draw) |
| **ESPHome** | ✅ YAML templates | E1001 – E1002 | Smart home integration with Home Assistant |
| **TRMNL** | ✅ Ready | E1001 – E1003 | Official TRMNL dashboard firmware for always-on ePaper panels |
| **EEZ Studio** | ✅ Project template | E1001 – E1004 | Visual LVGL UI design with a downloadable PlatformIO project template |
| **LVGL** | ✅ Ready | E1001 – E1004 | Ready-to-run LVGL 9.5.0 status dashboard rendered through Seeed_GFX |
| **Zephyr** | ✅ Official docs | E1001 – E1003 | Official Zephyr RTOS board documentation and local west build/flash workflow |
| **SquareLine Vision** | 🔜 Coming soon | E1002, E1003 | Visual UI designer for embedded ePaper displays |
| **OpenDisplay** | ✅ Official toolbox | E1001 – E1003 | BLE-powered ePaper control and browser image upload through the official OpenDisplay toolbox |
| **Voice Memo Reminder** | ✅ Community | E1001 – E1003 | AI voice memo firmware with English and Chinese reminder-list builds |
| **ESP32 PhotoFrame** | ✅ Community | E1002, E1004 | Full-color photo frame firmware with measured-palette image quality, web UI, REST API, and Home Assistant integration ([repo](https://github.com/aitjcize/esp32-photoframe)) |

<p align="right">(<a href="#readme-top">back to top</a>)</p>

## Built With

* [![HTML5][HTML5-badge]][HTML5-url]
* [![CSS3][CSS3-badge]][CSS3-url]
* [![JavaScript][JS-badge]][JS-url]
* [![ESP Web Tools][ESPWebTools-badge]][ESPWebTools-url]
* [![Arduino][Arduino-badge]][Arduino-url]
* [PlatformIO](https://platformio.org/)
* [LVGL](https://lvgl.io/)

The web app is a **zero-dependency static site** — no bundler, no framework, no `node_modules`. It uses the [Web Serial API](https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API) via [ESP Web Tools](https://esphome.github.io/esp-web-tools/) to flash firmware directly from the browser.

Firmware projects are built with Arduino CLI or PlatformIO, depending on the target. The release pipeline maps each example to its supported reTerminal model and publishes browser-flashable manifests automatically.

<p align="right">(<a href="#readme-top">back to top</a>)</p>

## Getting Started

### Use the Firmware Hub

The quickest way to get started is to follow the path shown by the selected platform:

1. Open the **[Firmware Hub](https://seeed-projects.github.io/OSHW-reTerminal-Series-E-D/)** in **Chrome** or **Edge** (desktop only)
2. Connect your reTerminal E-Series device via USB
3. Select a platform and device
4. Pick a firmware demo and version; for ESPHome, choose template options, then use **Copy to clipboard** or **Download file**; for EEZ Studio, click **Download project template** and build locally with PlatformIO; for OpenDisplay, click **Open OpenDisplay Toolbox** and continue with the official toolbox; for Zephyr, click **Open Zephyr Board Docs** and follow the selected board's official west workflow
5. For flashable firmware, choose **Standard flash** or **Erase flash + flash**, click **Connect & flash**, select the serial port, and wait for the flash to complete

> **Note:** Web Serial requires **HTTPS** or **localhost** and is only available in Chromium-based browsers (Chrome, Edge, Opera). Safari and Firefox are not supported.

### Local Development

To run the Firmware Hub locally for development:

```bash
# Clone the repository
git clone https://github.com/Seeed-Projects/OSHW-reTerminal-Series-E-D.git
cd OSHW-reTerminal-Series-E-D

# Serve the web directory (any static server works)
npx serve web
# or
python3 -m http.server 8000 -d web
```

Then open `http://localhost:8000` (or whichever port your server uses).

> **Tip:** Web Serial works on `localhost` without HTTPS, so local dev servers work out of the box.

### Build Firmware Locally

To compile Arduino sketches without the CI pipeline, use Arduino CLI:

```bash
# Install arduino-cli
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh

# Add ESP32 board support
arduino-cli config init
arduino-cli config add board_manager.additional_urls \
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32

# Compile a sketch (example: RTC_PCF8563)
arduino-cli compile \
  --fqbn "esp32:esp32:XIAO_ESP32S3:FlashSize=8M,PartitionScheme=default_8MB,PSRAM=opi" \
  --output-dir ./build/RTC_PCF8563 \
  ./examples/base/RTC_PCF8563
```

For sketches that use `Seeed_GFX` (e.g., `E1003_TouchDraw`):

```bash
arduino-cli config set library.enable_unsafe_install true
arduino-cli lib install --git-url https://github.com/Seeed-Studio/Seeed_GFX.git
```

Official PlatformIO projects can be built from their own directories:

```bash
# LVGL status panel for E1001
pio run -d examples/official/LVGLePaperStatusPanel -e reterminal_e1001

# EEZ Studio template for E1004
pio run -d examples/official/EEZStudio -e reterminal_e1004
```

TRMNL firmware uses the PlatformIO environments defined in `examples/official/TRMNL/platformio.ini`.

<p align="right">(<a href="#readme-top">back to top</a>)</p>

## Firmware Examples

Each example lives under a grouped `examples/` category and can be flashed via
the Firmware Hub or compiled manually.

| Example | Compatible Devices | Description |
|:--------|:-------------------|:------------|
| [`RTC_PCF8563`](examples/base/RTC_PCF8563/) | E1001 – E1004 | Read and write the PCF8563 real-time clock over I2C |
| [`LowPower_DeepSleep`](examples/base/LowPower_DeepSleep/) | E1001 – E1004 | Enter ESP32-S3 deep sleep (~14 µA), wake on button press |
| [`MicRecordToSD`](examples/base/MicRecordToSD/) | E1001 – E1003 | Record PDM microphone audio to WAV files on MicroSD |
| [`LED_Control`](examples/base/LED_Control/) | E1001 – E1004 | Toggle the onboard LED with model-specific firmware builds |
| [`Buzzer_Control`](examples/base/Buzzer_Control/) | E1001 – E1004 | Play basic alert tones from the onboard buzzer |
| [`Buzzer_Music`](examples/base/Buzzer_Music/) | E1001 – E1004 | Play musical note patterns from the onboard buzzer |
| [`UserButtons`](examples/base/UserButtons/) | E1001 – E1004 | Detect and report presses on the three user buttons |
| [`Battery_Monitor`](examples/base/Battery_Monitor/) | E1001 – E1004 | Read and report battery voltage through the onboard ADC path |
| [`MicroSD_ListFiles`](examples/base/MicroSD_ListFiles/) | E1001 – E1004 | Mount the MicroSD card and list files over serial |
| [`E1003_TouchDraw`](examples/base/E1003_TouchDraw/) | E1003 | Draw on the 10.3″ ePaper display using touch input |
| [`GxEPD2_reTerminal_E1001`](examples/base/GxEPD2_reTerminal_E1001/) | E1001 | Run a multi-screen black-and-white GxEPD2 display demo |
| [`GxEPD2_reTerminal_E1002`](examples/base/GxEPD2_reTerminal_E1002/) | E1002 | Run a multi-screen 6-color GxEPD2 display demo |
| [`GxEPD2_reTerminal_E1003`](examples/base/GxEPD2_reTerminal_E1003/) | E1003 | Run a 10.3″ grayscale GxEPD2 display demo |
| [`GxEPD2_reTerminal_E1004`](examples/base/GxEPD2_reTerminal_E1004/) | E1004 | Run a 13.3″ 6-color GxEPD2 display demo |
| [`GxEPD2_reTerminal_E1001_Gray4`](examples/base/GxEPD2_reTerminal_E1001_Gray4/) | E1001 | Render a 4-level grayscale display demo through Adafruit GFX canvas logic |
| [`Seeed_GFX_E1001_Gray4`](examples/base/Seeed_GFX_E1001_Gray4/) | E1001 | Render a 4-level grayscale display demo through Seeed_GFX |
| [`GxEPD2_reTerminal_E1003_Gray16`](examples/base/GxEPD2_reTerminal_E1003_Gray16/) | E1003 | Render a 16-level grayscale display demo |
| [`E1001_ChineseTextDemo`](examples/base/E1001_ChineseTextDemo/) | E1001 | Render Chinese text with a TTF font stored in SPIFFS |
| [`E1002_ChineseTextDemo`](examples/base/E1002_ChineseTextDemo/) | E1002 | Render Chinese text with a TTF font stored in SPIFFS |
| [`E1003_ChineseTextDemo`](examples/base/E1003_ChineseTextDemo/) | E1003 | Render Chinese text with a TTF font stored in SPIFFS |
| [`E1004_ChineseTextDemo`](examples/base/E1004_ChineseTextDemo/) | E1004 | Render Chinese text with a TTF font stored in SPIFFS |
| [`SHT4x_Sensor`](examples/base/SHT4x_Sensor/) | E1001 – E1004 | Read temperature and humidity from the SHT40 sensor over I2C |
| [`SD_ImagePipeline_E1001_BW`](examples/base/SD_ImagePipeline_E1001_BW/) | E1001 | Display a JPEG/PNG from MicroSD with B&W dithering |
| [`SD_ImagePipeline_E1001_Gray4`](examples/base/SD_ImagePipeline_E1001_Gray4/) | E1001 | Display a JPEG/PNG from MicroSD in 4-level grayscale |
| [`SD_ImagePipeline_E1002`](examples/base/SD_ImagePipeline_E1002/) | E1002 | Display a JPEG/PNG from MicroSD in 6-color (Spectra 6) |
| [`SD_ImagePipeline_E1003`](examples/base/SD_ImagePipeline_E1003/) | E1003 | Display a JPEG/PNG from MicroSD in 16-level grayscale |
| [`SD_ImagePipeline_E1004`](examples/base/SD_ImagePipeline_E1004/) | E1004 | Display a JPEG/PNG from MicroSD in 6-color (Spectra 6) |
| [`TRMNL`](examples/official/TRMNL/) | E1001 – E1003 | Build the official TRMNL dashboard firmware for reTerminal E-Series |
| [`EEZStudio`](examples/official/EEZStudio/) | E1001 – E1004 | Build an LVGL PlatformIO template generated for EEZ Studio workflows |
| [`LVGLePaperStatusPanel`](examples/official/LVGLePaperStatusPanel/) | E1001 – E1004 | Render a static LVGL 9.5.0 status dashboard with Seeed_GFX |
| [`ePaper-Voice-Memo`](examples/community/ePaper-Voice-Memo/) | E1001 – E1003 | AI voice memo to compact/card ePaper reminder lists with English/Chinese firmware options |

<p align="right">(<a href="#readme-top">back to top</a>)</p>

## Project Structure

```
OSHW-reTerminal-Series-E-D/
├── .github/workflows/
│   └── build-and-deploy.yml    # CI: compile firmware + deploy to GitHub Pages
├── .github/scripts/
│   └── firmware_release.py     # CI helper: changed-example planning + release packaging
├── examples/                   # Firmware examples grouped by product category
│   ├── base/                    # Base hardware demos
│   ├── official/                # Official platform or partner demos
│   └── community/               # Community projects
├── web/                        # Static web app (Firmware Hub)
│   ├── index.html              # Single-page application
│   ├── css/style.css           # Responsive styles
│   ├── js/
│   │   ├── app.js              # UI logic, Web Serial monitor, flash events
│   │   ├── esphome-template.js # ESPHome YAML template generator
│   │   └── firmwares.js        # Device and platform data definitions
│   └── assets/
│       ├── brand/              # Logo and icons
│       ├── devices/            # Device product photos
│       └── platforms/          # Platform logos and preview images
├── LICENSE                     # MIT
└── README.md
```

<p align="right">(<a href="#readme-top">back to top</a>)</p>

## CI/CD

The GitHub Actions workflow ([`build-and-deploy.yml`](.github/workflows/build-and-deploy.yml)) runs on pushes to `main`:

1. **Plan** — detects which `examples/` folders changed during the push
2. **Build** — compiles only the firmware targets mapped to changed examples
3. **Version** — writes changed firmware to `firmware/{id}/{YYYY.MM.DD}/`, or `YYYY.MM.DD.n` for repeated builds on the same date; official external firmware targets may use their upstream version, such as TRMNL `1.8.10`
4. **Deploy** — updates the static site, `firmware/versions.json`, `firmware/catalog.json`, and generated firmware manifests on the `gh-pages` branch
5. **Release** — when firmware changed, creates a GitHub Release containing a full latest firmware package for all examples; unchanged firmware is reused from its previous latest published version

Configure GitHub Pages to serve from the `gh-pages` branch. The deployed site is available at **https://seeed-projects.github.io/OSHW-reTerminal-Series-E-D/**.

<p align="right">(<a href="#readme-top">back to top</a>)</p>

## Roadmap

- [x] Base demo firmware (RTC, deep sleep, mic, touch draw)
- [x] Browser-based flashing via ESP Web Tools
- [x] Standard flash and erase-then-flash install modes
- [x] Built-in serial monitor
- [x] CI/CD pipeline for automated builds and deployment
- [x] ESPHome YAML template generation for Home Assistant workflows
- [x] EEZ Studio PlatformIO project template download
- [x] Official TRMNL firmware entries for E1001, E1002, and E1003
- [x] Official LVGL 9.5.0 status panel firmware entries for E1001, E1002, E1003, and E1004
- [x] Chinese text demos for E1001, E1002, E1003, and E1004
- [x] MicroSD image pipeline demos with browser-configurable image options
- [x] Community Voice Memo Reminder firmware entries for English and Chinese builds
- [x] Community ESP32 PhotoFrame firmware entries with erase-then-flash install flow
- [ ] SquareLine Vision UI designer support
- [x] OpenDisplay official toolbox entry for BLE control and image upload
- [x] Zephyr official board documentation entry for E1001, E1002, and E1003

See the [open issues](https://github.com/Seeed-Projects/OSHW-reTerminal-Series-E-D/issues) for a full list of proposed features and known bugs.

<p align="right">(<a href="#readme-top">back to top</a>)</p>

## Contributing

Contributions make the open-source community thrive. Any contribution you make is **greatly appreciated**.

1. Fork the Project
2. Create your Feature Branch (`git checkout -b feature/amazing-feature`)
3. Commit your Changes (`git commit -m 'feat: add amazing feature'`)
4. Push to the Branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### Adding a New Firmware Example

1. Create a new directory under `examples/base/`, `examples/official/`, or `examples/community/`
2. Make sure it compiles with `arduino-cli` using the XIAO ESP32-S3 FQBN:
   ```
   esp32:esp32:XIAO_ESP32S3:FlashSize=8M,PartitionScheme=default_8MB,PSRAM=opi
   ```
3. Add a short example README if setup steps are not obvious
4. Do not edit generated firmware files, the `gh-pages` branch, or GitHub Releases
5. For standard Arduino examples, GitHub Actions can auto-discover the grouped folder and build only that example; for PlatformIO or multi-device builds, include supported devices and build environment details in the pull request description

<p align="right">(<a href="#readme-top">back to top</a>)</p>

## License

Distributed under the MIT License. See [`LICENSE`](LICENSE) for more information.

<p align="right">(<a href="#readme-top">back to top</a>)</p>

## Contact

**Seeed Projects** — [GitHub](https://github.com/Seeed-Projects)

Project Link: [https://github.com/Seeed-Projects/OSHW-reTerminal-Series-E-D](https://github.com/Seeed-Projects/OSHW-reTerminal-Series-E-D)

<p align="right">(<a href="#readme-top">back to top</a>)</p>

## Acknowledgments

* [ESP Web Tools](https://esphome.github.io/esp-web-tools/) — browser-based ESP flashing by ESPHome
* [Web Serial API](https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API) — serial communication from the browser
* [Arduino ESP32 Core](https://github.com/espressif/arduino-esp32) — ESP32 board support for Arduino
* [Seeed_GFX](https://github.com/Seeed-Studio/Seeed_GFX) — graphics library for Seeed ePaper displays
* [Seeed Studio Wiki](https://wiki.seeedstudio.com/reterminal_e10xx_main_page/) — official hardware documentation
* [Best-README-Template](https://github.com/othneildrew/Best-README-Template) — README template inspiration

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- Shield Links -->
[contributors-shield]: https://img.shields.io/github/contributors/Seeed-Projects/OSHW-reTerminal-Series-E-D.svg?style=for-the-badge
[contributors-url]: https://github.com/Seeed-Projects/OSHW-reTerminal-Series-E-D/graphs/contributors
[forks-shield]: https://img.shields.io/github/forks/Seeed-Projects/OSHW-reTerminal-Series-E-D.svg?style=for-the-badge
[forks-url]: https://github.com/Seeed-Projects/OSHW-reTerminal-Series-E-D/network/members
[stars-shield]: https://img.shields.io/github/stars/Seeed-Projects/OSHW-reTerminal-Series-E-D.svg?style=for-the-badge
[stars-url]: https://github.com/Seeed-Projects/OSHW-reTerminal-Series-E-D/stargazers
[issues-shield]: https://img.shields.io/github/issues/Seeed-Projects/OSHW-reTerminal-Series-E-D.svg?style=for-the-badge
[issues-url]: https://github.com/Seeed-Projects/OSHW-reTerminal-Series-E-D/issues
[license-shield]: https://img.shields.io/github/license/Seeed-Projects/OSHW-reTerminal-Series-E-D.svg?style=for-the-badge
[license-url]: https://github.com/Seeed-Projects/OSHW-reTerminal-Series-E-D/blob/main/LICENSE

<!-- Tech Badges -->
[HTML5-badge]: https://img.shields.io/badge/HTML5-E34F26?style=for-the-badge&logo=html5&logoColor=white
[HTML5-url]: https://developer.mozilla.org/en-US/docs/Web/HTML
[CSS3-badge]: https://img.shields.io/badge/CSS3-1572B6?style=for-the-badge&logo=css3&logoColor=white
[CSS3-url]: https://developer.mozilla.org/en-US/docs/Web/CSS
[JS-badge]: https://img.shields.io/badge/JavaScript-F7DF1E?style=for-the-badge&logo=javascript&logoColor=black
[JS-url]: https://developer.mozilla.org/en-US/docs/Web/JavaScript
[ESPWebTools-badge]: https://img.shields.io/badge/ESP_Web_Tools-000000?style=for-the-badge&logo=esphome&logoColor=white
[ESPWebTools-url]: https://esphome.github.io/esp-web-tools/
[Arduino-badge]: https://img.shields.io/badge/Arduino-00979D?style=for-the-badge&logo=arduino&logoColor=white
[Arduino-url]: https://www.arduino.cc/
