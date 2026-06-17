# TRMNL Firmware for reTerminal E-Series

This folder contains the TRMNL PlatformIO firmware source used by the Firmware Hub build pipeline.

Source snapshot:

- Upstream: https://github.com/usetrmnl/trmnl-firmware
- Snapshot commit: `7abc83c`
- Hub version: `1.8.7`

## Supported Devices

| Device | PlatformIO environment | Firmware ID |
|---|---|---|
| reTerminal E1001 | `seeed_reTerminal_E1001` | `TRMNL_reTerminal_E1001` |
| reTerminal E1002 | `seeed_reTerminal_E1002` | `TRMNL_reTerminal_E1002` |
| reTerminal E1003 | `TRMNL_X_E1003` | `TRMNL_reTerminal_E1003` |

reTerminal E1004 is intentionally not enabled for TRMNL in the Firmware Hub.

## CI Build

GitHub Actions builds these targets through `.github/scripts/firmware_release.py`.
The generated firmware files, manifests, version index, and GitHub Release assets are published automatically from the workflow.

Do not commit generated `.bin` files or generated `web/firmware/` output from this folder.
