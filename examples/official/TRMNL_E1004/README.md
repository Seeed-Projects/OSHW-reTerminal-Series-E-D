# TRMNL Firmware for reTerminal E1004

This folder contains the isolated TRMNL PlatformIO firmware source used to
build the reTerminal E1004 Firmware Hub release.

Source snapshot:

- Upstream: https://github.com/usetrmnl/trmnl-firmware
- Snapshot commit: `64ae0ac`
- Hub version: `1.8.10`

## Supported Devices

| Device | PlatformIO environment | Firmware ID |
|---|---|---|
| reTerminal E1004 | `seeed_reTerminal_E1004` | `TRMNL_reTerminal_E1004` |

## CI Build

GitHub Actions builds these targets through `.github/scripts/firmware_release.py`.
The workflow publishes generated firmware files, manifests, the version index,
and GitHub Release assets. Repository changes stay focused on source and
configuration files.
