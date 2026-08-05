# TRMNL Firmware for reTerminal E-Series

This folder contains the TRMNL PlatformIO firmware source used by the Firmware Hub build pipeline.

Source snapshot:

- Upstream: https://github.com/usetrmnl/trmnl-firmware
- Snapshot commit: `64ae0ac`
- Hub version: `1.8.10`

## Supported Devices

| Device | PlatformIO environment | Firmware ID |
|---|---|---|
| reTerminal E1001 | `seeed_reTerminal_E1001` | `TRMNL_reTerminal_E1001` |
| reTerminal E1002 | `seeed_reTerminal_E1002` | `TRMNL_reTerminal_E1002` |
| reTerminal E1003 | `TRMNL_X_E1003` | `TRMNL_reTerminal_E1003` |
| reTerminal E1004 | `seeed_reTerminal_E1004` | `TRMNL_reTerminal_E1004` |

## CI Build

GitHub Actions builds these targets through `.github/scripts/firmware_release.py`.
The workflow publishes generated firmware files, manifests, the version index,
and GitHub Release assets. Repository changes stay focused on source and
configuration files.
