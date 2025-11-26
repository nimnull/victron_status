# Victron Multi-System Monitor

| Target | Display | Touch | Connectivity |
|--------|---------|-------|--------------|
| ESP32-S3 | 2.41" AMOLED 600×450 | FT6336U Capacitive | WiFi 802.11 b/g/n |

A portable, battery-powered energy monitoring solution built for real-time visualization of Victron Energy installations via MQTT.

## Overview

This firmware transforms the Waveshare ESP32-S3-Touch-AMOLED-2.41 into a dedicated monitoring station for Victron energy systems. It connects to up to two independent Victron GX devices via MQTT and displays grid connection status with visual LED indicators on a vibrant AMOLED display.

### Current Features

- **Multi-System Monitoring**: Track two independent Victron installations simultaneously
- **Real-Time MQTT**: Subscribe to Victron GX device MQTT brokers for live data
- **Visual Status Dashboard**: LED indicators for WiFi, MQTT, and grid connection states
- **Thread-Safe Architecture**: FreeRTOS-based with proper mutex protection for LVGL
- **Dark Theme UI**: AMOLED-optimized interface with true blacks for power efficiency
- **Screen Power Management**: Auto-off after 15s inactivity, wake on touch or grid status change
- **Modular Architecture**: Clean separation of display, touch, LVGL port, and application logic
- **Secure Credentials**: Sensitive values stored in gitignored `secrets.txt` file

### Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                         │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ app_victron  │  │  ui_status   │  │ victron_data │      │
│  │  (Main App)  │  │   (LVGL UI)  │  │   (Models)   │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
├─────────────────────────────────────────────────────────────┤
│                    Service Layer                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ victron_mqtt │  │screen_timeout│  │network_manager│     │
│  │  (ESP-MQTT)  │  │(Power Mgmt)  │  │   (WiFi)     │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
├─────────────────────────────────────────────────────────────┤
│                    Platform Layer                            │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │display_driver│  │ touch_driver │  │  lvgl_port   │      │
│  │   (SH8601)   │  │  (FT6336U)   │  │(Tick/Mutex)  │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
├─────────────────────────────────────────────────────────────┤
│                    ESP-IDF v6.1.0                            │
│            FreeRTOS │ LVGL 9.4.0 │ ESP-MQTT                 │
└─────────────────────────────────────────────────────────────┘
```

## Target Hardware

### Waveshare ESP32-S3-Touch-AMOLED-2.41

| Component | Specification |
|-----------|---------------|
| **Processor** | Xtensa 32-bit LX7 dual-core @ 240MHz |
| **Memory** | 512KB SRAM, 384KB ROM |
| **Storage** | 16MB Flash, 8MB PSRAM |
| **Display** | 2.41" AMOLED, 600×450, 16.7M colors |
| **Display Driver** | RM690B0 (SH8601 compatible) via QSPI |
| **Touch** | FT6336U capacitive, 5-point multi-touch |
| **Sensors** | QMI8658C IMU, PCF85063 RTC |
| **Wireless** | WiFi 802.11 b/g/n, Bluetooth 5 LE |
| **Power** | Li-ion battery support with charging circuit |
| **Storage** | TF card slot (up to 64GB) |
| **Interface** | USB-C |

### Pin Configuration

| Function | GPIO | Notes |
|----------|------|-------|
| **QSPI Display** | | |
| LCD_CS | GPIO9 | Chip select |
| LCD_CLK | GPIO10 | Clock |
| LCD_D0-D3 | GPIO11-14 | Data lines |
| LCD_RST | GPIO21 | Reset |
| **I2C Bus** | | Shared: Touch, IMU, RTC |
| SDA | GPIO47 | Data |
| SCL | GPIO48 | Clock |
| **Touch** | | |
| TP_RST | GPIO3 | Reset |
| TP_INT | EXIO2 | Interrupt (via expander) |
| **SD Card** | | |
| CS | GPIO2 | |
| SCLK | GPIO4 | |
| MOSI | GPIO5 | |
| MISO | GPIO6 | |
| **Battery** | | |
| BAT_Control | GPIO16 | Power management |
| BAT_ADC | GPIO17 | Voltage monitoring |

## Building

### Prerequisites

- ESP-IDF v6.1.0 or later
- Python 3.10+
- USB-C cable

### Setup

```bash
# Clone ESP-IDF (if not already installed)
git clone -b v6.1 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf && ./install.sh && source export.sh

# Navigate to project
cd /path/to/project

# Configure target
idf.py set-target esp32s3

# Configure credentials (required)
cp main/secrets.txt.example main/secrets.txt
# Edit main/secrets.txt with your WiFi and MQTT credentials:
#   WIFI_SSID=your_network_name
#   WIFI_PASSWORD=your_password
#   MQTT_BROKER_URL_1=mqtt://192.168.x.x:1883
#   VICTRON_SYSTEM_ID_1=your_portal_id
#   MQTT_BROKER_URL_2=mqtt://192.168.x.x:1883
#   VICTRON_SYSTEM_ID_2=your_portal_id

# Build (secrets are loaded automatically)
idf.py build

# Flash
idf.py -p /dev/ttyUSB0 flash monitor
```

> **Note**: The `secrets.txt` file is gitignored. For menuconfig options like system names, use `idf.py menuconfig` → "Victron MQTT Configuration".

### Configuration Options

Access via `idf.py menuconfig` → "Victron MQTT Configuration":

| Option | Description | Default |
|--------|-------------|---------|
| `WIFI_SSID` | WiFi network name | YourWiFiNetwork |
| `WIFI_PASSWORD` | WiFi password | YourPassword |
| `MQTT_BROKER_URL_1` | System 1 MQTT broker | mqtt://192.168.125.10:1883 |
| `VICTRON_SYSTEM_ID_1` | System 1 portal ID | c0619ab40108 |
| `VICTRON_SYSTEM_NAME_1` | System 1 display name | Green |
| `MQTT_BROKER_URL_2` | System 2 MQTT broker | mqtt://192.168.125.11:1883 |
| `VICTRON_SYSTEM_ID_2` | System 2 portal ID | c0619ab40109 |
| `VICTRON_SYSTEM_NAME_2` | System 2 display name | Yellow |
| `WIFI_MAXIMUM_RETRY` | WiFi reconnection attempts | 5 |

## Project Structure

```
├── main/
│   ├── app_main.c            # Application entry point (~70 lines)
│   ├── app_victron.c/h       # Victron application logic (WiFi, MQTT, UI init)
│   ├── display_driver.c/h    # SH8601 AMOLED driver (QSPI, LVGL integration)
│   ├── touch_driver.c/h      # FT6336U touch driver (I2C, input device)
│   ├── lvgl_port.c/h         # LVGL runtime (tick timer, mutex, task)
│   ├── screen_timeout.c/h    # Screen power management (auto-off, wake)
│   ├── network_manager.c/h   # WiFi connection management
│   ├── victron_mqtt.c/h      # MQTT client for Victron GX devices
│   ├── victron_data.c/h      # Data models and thread-safe storage
│   ├── ui_status.c/h         # LVGL status dashboard
│   ├── secrets.txt           # Credentials (gitignored)
│   ├── secrets.txt.example   # Credentials template
│   ├── Kconfig.projbuild     # User-configurable options
│   └── idf_component.yml     # Component dependencies
├── managed_components/       # ESP-IDF component manager dependencies
│   ├── espressif__esp_lcd_sh8601/
│   ├── espressif__esp_lcd_touch_ft5x06/
│   ├── espressif__mqtt/
│   └── lvgl__lvgl/
├── CMakeLists.txt            # Build config (includes secrets parsing)
├── sdkconfig.defaults        # Project-specific SDK defaults
├── sdkconfig.defaults.secrets # Auto-generated from secrets.txt (gitignored)
├── partitions.csv            # Flash partition table
└── CLAUDE.md                 # Development guidelines
```

## Hardware Configuration

### Memory Layout (16MB Flash)

| Partition | Offset | Size | Purpose |
|-----------|--------|------|---------|
| nvs | 0x9000 | 24KB | WiFi credentials, settings |
| phy_init | 0xF000 | 4KB | PHY calibration |
| factory | 0x10000 | 4MB | Main application |
| storage | 0x410000 | 1MB | Historical data (SPIFFS) |
| ota_0 | 0x510000 | 4MB | OTA updates (future) |
| coredump | 0x910000 | 64KB | Crash analysis |

### PSRAM Utilization (8MB)

The 8MB PSRAM is configured for automatic allocation:
- Allocations ≥16KB automatically use PSRAM
- 32KB internal RAM reserved for critical operations
- LVGL display buffers benefit from PSRAM
- MQTT message queues use PSRAM for large payloads

### Performance Optimizations

| Setting | Value | Rationale |
|---------|-------|-----------|
| CPU Frequency | 240MHz | Smooth LVGL animations |
| Flash Speed | 80MHz OPI | Fast asset loading |
| PSRAM Speed | 80MHz Octal | High bandwidth for buffers |
| FreeRTOS Tick | 1000Hz | Responsive touch input |
| Data Cache | 32KB 8-way | LVGL rendering performance |
| Compiler | -O2 (perf) | Optimized for speed |

### LVGL Configuration

| Setting | Value | Purpose |
|---------|-------|---------|
| Color Depth | 24-bit | Full AMOLED color range |
| Memory Pool | 64KB | Internal LVGL heap |
| Refresh Period | 33ms | ~30fps target |
| DPI | 130 | UI element scaling |
| Theme | Dark | AMOLED power efficiency |

### Enabled LVGL Widgets

Core widgets enabled for the status dashboard:
- `label`, `led`, `bar`, `arc` - Status indicators
- `button`, `switch`, `slider` - Controls
- `chart`, `spinner` - Data visualization
- `image` - Icons and graphics
- `flex`, `grid` - Layouts

Disabled widgets to save flash: calendar, keyboard, textarea, dropdown, roller, table, tabview, menu, msgbox, canvas.

## Victron MQTT Integration

### Topic Structure

Victron GX devices publish data to MQTT topics following this pattern:

```
N/<portal_id>/<service>/<instance>/<path>
```

Currently subscribed topics:
- `N/<system_id>/vebus/276/Ac/ActiveIn/Connected` - Grid connection status

### Keepalive Protocol

Victron MQTT requires periodic keepalive messages to maintain subscriptions:
- Publish to `R/<portal_id>/keepalive` every 50 seconds
- Empty payload triggers data republish from GX device

## Development

### Dependencies

Managed via ESP-IDF Component Manager (`idf_component.yml`):

| Component | Version | Purpose |
|-----------|---------|---------|
| `lvgl/lvgl` | ^9.4.0 | Graphics library |
| `espressif/esp_lcd_sh8601` | ^1.0.0 | AMOLED display driver |
| `espressif/esp_lcd_touch_ft5x06` | ^1.0.7 | Touch controller driver |
| `espressif/mqtt` | ^1.0.0 | MQTT client |

### Task Architecture

| Task | Priority | Stack | Purpose |
|------|----------|-------|---------|
| LVGL Port | 2 | 8KB | UI rendering, screen timeout checks |
| MQTT Client | 5 | 4KB | Network communication |
| Main | 1 | 4KB | Status monitoring loop |

### Thread Safety

LVGL operations are protected by a mutex via `lvgl_port`:
```c
#include "lvgl_port.h"

if (lvgl_port_lock(timeout_ms)) {
    // Safe to call LVGL APIs
    lvgl_port_unlock();
}
```

### Screen Power Management

The screen automatically turns off after 15 seconds of touch inactivity:
```c
#include "screen_timeout.h"

// Wake screen programmatically (e.g., on grid status change)
screen_timeout_wake();

// Check if screen is currently on
if (screen_timeout_is_on()) { ... }
```

## Roadmap

### Phase 1: Foundation ✅ Complete
- [x] Display initialization with LVGL 9.4
- [x] Touch input handling
- [x] WiFi connection management
- [x] Multi-system MQTT client
- [x] Basic status dashboard
- [x] Modular code architecture (display/touch/lvgl_port drivers)
- [x] Screen timeout with auto-off (15s inactivity)
- [x] Wake on touch (first touch consumed for wake)
- [x] Wake on grid status change (connected ↔ disconnected)
- [x] Secure credentials management (gitignored secrets.txt)

### Phase 2: Enhanced Monitoring
- [ ] Battery voltage/SOC display
- [ ] AC consumption meters (L1, L2, L3)
- [ ] Inverter power/current gauges
- [ ] Historical data graphs

### Phase 3: Advanced Features
- [ ] IMU-based gesture navigation (swipe between screens)
- [ ] SD card data logging
- [ ] RTC-based scheduled updates
- [ ] Battery power management for portable use
- [ ] Power flow animation diagram

### Phase 4: Polish
- [ ] Touch gesture navigation
- [ ] AMOLED burn-in prevention (pixel shifting)
- [ ] Deep sleep with motion wake
- [ ] OTA firmware updates

## Hardware Expansion Opportunities

The ESP32-S3-Touch-AMOLED-2.41 includes sensors not yet utilized:

| Sensor | Model | Potential Use |
|--------|-------|---------------|
| IMU | QMI8658C | Wake-on-motion, gesture control |
| RTC | PCF85063 | Scheduled wake, time-based data logging |
| SD Card | TF slot | Historical data storage, config backup |
| Battery | ADC GPIO17 | Battery level display, low-power alerts |

## Troubleshooting

### Build Issues

**Component not found**: Run `idf.py reconfigure` to fetch managed components.

**Display not working**: Verify QSPI pin assignments match your hardware revision.

### Runtime Issues

**WiFi connection fails**: Check credentials in menuconfig. Monitor output for detailed errors.

**MQTT disconnects**: Verify broker URL and port. Check that keepalive timer is running.

**Touch not responding**: Ensure I2C bus is properly initialized. Check touch reset GPIO.

## Resources

- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- [LVGL Documentation](https://docs.lvgl.io/9.4/)
- [Victron MQTT Documentation](https://github.com/victronenergy/dbus-mqtt)
- [Waveshare ESP32-S3-Touch-AMOLED-2.41 Wiki](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-2.41)

## License

See [LICENSE](LICENSE) file.
