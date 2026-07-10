# Vice City Map Navigator

A GTA Vice City-style circular minimap navigator for the **Waveshare ESP32-S3 1.28" Round LCD** (GC9A01, 240x240). Uses a GPS module for real-time positioning and the Google Maps Static API to fetch map tiles styled with Vice City's neon aesthetic.

![ESP32-S3](https://img.shields.io/badge/ESP32--S3-blue) ![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.x-green)

## Features

- **Live GPS tracking** — NMEA parsing from any standard GPS module (NEO-6M, etc.)
- **Vice City color scheme** — Neon pink roads, dark teal water, deep purple land, no labels
- **Circular display masking** — Map is clipped to the round display with a clean edge
- **Player arrow** — Rotates with GPS heading, white fill with pink outline
- **Smart tile fetching** — Only re-fetches when you move >5 meters, saving API quota
- **Loading screen** — Teal ring + crosshair while waiting for GPS fix

## Hardware

| Component | Details |
|---|---|
| Display | Waveshare 1.28" Round LCD (GC9A01, 240x240, SPI) |
| MCU | ESP32-S3 (built into the Waveshare module) |
| GPS | NEO-6M or any NMEA-compatible module (UART) |

### Wiring (GPS module)

| GPS Pin | ESP32-S3 GPIO |
|---|---|
| TX | GPIO 44 (UART RX) |
| RX | GPIO 43 (UART TX) |
| VCC | 3.3V |
| GND | GND |

The display is built into the Waveshare module — no additional wiring needed for it.

## Prerequisites

- [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/)
- A [Google Maps Static API key](https://developers.google.com/maps/documentation/maps-static/get-api-key) with the Maps Static API enabled

## Setup

```bash
# Clone
git clone https://github.com/procrastinest0/esp32GTAmap.git
cd esp32GTAmap

# Set target
idf.py set-target esp32s3

# Configure WiFi and API key
idf.py menuconfig
# Navigate to: Vice City Map Configuration
#   - Set WiFi SSID
#   - Set WiFi Password
#   - Set Google Maps API Key
#   - Adjust zoom level (default: 16)

# Build, flash, and monitor
idf.py build flash monitor
```

## Configuration

All settings are in `menuconfig` under **Vice City Map Configuration**:

| Setting | Default | Description |
|---|---|---|
| `WIFI_SSID` | — | Your WiFi network name |
| `WIFI_PASSWORD` | — | Your WiFi password |
| `GOOGLE_MAPS_API_KEY` | — | Google Maps Static API key |
| `GPS_UART_PORT` | 1 | UART port for GPS |
| `GPS_UART_TX` | 43 | TX GPIO |
| `GPS_UART_RX` | 44 | RX GPIO |
| `GPS_UART_BAUD` | 9600 | GPS baud rate |
| `MAP_ZOOM_LEVEL` | 16 | Map zoom (1-20) |
| `MAP_FETCH_INTERVAL_MS` | 5000 | Tile refresh interval |

## How It Works

1. **Boot** — Shows a loading screen (teal ring + crosshair on dark purple)
2. **WiFi** — Connects to configured network
3. **GPS** — Parses NMEA sentences (GGA for position, RMC for heading)
4. **Map fetch** — Requests a styled JPEG tile from Google Maps Static API
5. **Render** — Decodes JPEG to RGB565, applies circular mask, draws player arrow
6. **Loop** — Re-fetches tile when position changes >5 meters

## Project Structure

```
├── CMakeLists.txt
├── sdkconfig.defaults
├── partitions.csv
├── main/
│   ├── main.c            # App entry, task orchestration
│   ├── display.c/h       # Framebuffer, circular mask, player arrow
│   ├── gps.c/h           # NMEA parser (GGA + RMC)
│   ├── map_client.c/h    # Google Maps API client, JPEG decode
│   ├── wifi.c/h          # WiFi STA management
│   └── Kconfig.projbuild # menuconfig options
└── components/
    └── gc9a01/            # GC9A01 round LCD driver (SPI)
```

## API Usage

The Google Maps Static API free tier includes **28,000 static map loads/month** ($200 credit). With the 5-second fetch interval and movement threshold, typical use stays well within limits. Monitor usage in the [Google Cloud Console](https://console.cloud.google.com/apis/dashboard).

## License

MIT
