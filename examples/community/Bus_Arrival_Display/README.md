# reTerminal E1001 Bus Arrival Display

This community application retrieves live arrivals from Singapore ArriveLah or
Hong Kong KMB/LWB and shows the stop name, time, and upcoming buses on a
reTerminal E1001 ePaper display.

## Hub configuration

Before flashing, enter Wi-Fi and bus-stop values in the Firmware Hub. The Hub
writes them to the ESP32 NVS `config` namespace; no credentials are stored in
the source code.

| NVS key | Description |
| --- | --- |
| `wifiSsid` | Wi-Fi network name |
| `wifiPassword` | Wi-Fi password |
| `busProvider` | `singapore` or `hongkongkmb` |
| `busId` | Provider-specific bus-stop ID |
| `busName` | Display name for the stop |
| `refreshSeconds` | Refresh interval from 30 to 3600 seconds |

The project uses `TFT_eSPI` and `ArduinoJson` in addition to the ESP32 Arduino
core libraries. It supports E1001 only.
