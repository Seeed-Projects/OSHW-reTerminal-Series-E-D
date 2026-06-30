# LVGL ePaper Status Panel

This PlatformIO project builds a simple LVGL status panel for Seeed reTerminal E Series ePaper devices.

The default target is `reterminal_e1001`.

The project uses the PlatformIO `espressif32` platform with the Seeed_GFX display driver and LVGL 9.

## Supported Environments

- `reterminal_e1001`
- `reterminal_e1002`
- `reterminal_e1003`
- `reterminal_e1004`

## Project Files

- `platformio.ini`: PlatformIO environments and build flags.
- `include/driver.h`: Selects the correct ePaper driver configuration.
- `include/lv_conf.h`: LVGL feature and font configuration.
- `src/main.cpp`: Initializes the display, LVGL, and ePaper refresh flow.
- `src/ui_status_panel.cpp`: Creates the status panel UI.

## Build

```bash
pio run
```

Build a specific target:

```bash
pio run -e reterminal_e1001
```

The generated firmware files are placed in:

```text
.pio/build/reterminal_e1001/firmware.bin
.pio/build/reterminal_e1001/firmware.factory.bin
```

## Upload

```bash
pio run -e reterminal_e1001 --target upload
```

## Monitor

```bash
pio device monitor -b 115200
```

Expected serial output:

```text
Seeed ePaper LVGL status panel starting.
LVGL status panel rendered.
```
