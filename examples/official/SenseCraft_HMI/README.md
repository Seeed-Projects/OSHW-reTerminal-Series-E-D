# SenseCraft HMI Firmware

This directory contains the SenseCraft HMI production firmware source used by
the reTerminal E-Series Firmware Hub.

The source snapshot comes from
[`Seeed-Studio/Seeed-reTerminal-E10xx-Firmware`](https://github.com/Seeed-Studio/Seeed-reTerminal-E10xx-Firmware)
commit `f5c21cc088291f901da5ca181af7460776092799`. The published firmware version is
`1.1.5`, and every release target uses the production SenseCraft HMI API.

## Supported targets

The Firmware Hub builds 21 targets from this source:

- reTerminal E1001, E1002, E1003, and E1004.
- EE02 with the 13.3-inch Spectra 6 panel.
- EE03 with the 10.3-inch monochrome panel.
- EE04 with eight registered SPI or Spectra 6 panels.
- EE05 with seven registered SPI panels.

Each XIAO ePaper target has a dedicated PlatformIO environment. The environment
selects one driver board and one panel at compile time, so CI jobs can run
independently without editing shared source files.

## Build locally

Install [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html),
then run one environment from the repository root:

```bash
# reTerminal E1001
pio run -d examples/official/SenseCraft_HMI -e reterminal_e1001

# EE04 with the 7.3-inch Spectra 6 panel
pio run -d examples/official/SenseCraft_HMI -e sensecraft_hmi_ee04_p073_sp6

# EE05 with the 7.5-inch monochrome panel
pio run -d examples/official/SenseCraft_HMI -e sensecraft_hmi_ee05_p075_mono
```

Build output is written under `.pio/build/<environment>/`.

## Flash layout

The browser installer publishes four firmware segments:

| Segment | Offset |
|:--|--:|
| Bootloader | `0x0` |
| Partition table | `0x8000` |
| Boot application selector | `0xE000` |
| SenseCraft HMI application | `0x90000` |

reTerminal targets use the 32 MB partition layout. EE02–EE05 targets use the
16 MB partition layout.

## Brand asset

The Firmware Hub platform icon mirrors the SVG currently used by the official
[SenseCraft HMI web app](https://sensecraft.seeed.cc/hmi/):
`https://sensecraft.seeed.cc/hmi/assets/seeedash_logo-CWk51vR-.svg`.

## License

The firmware source is licensed under the [MIT License](LICENSE).
