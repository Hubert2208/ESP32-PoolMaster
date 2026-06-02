# ESP32-PoolMaster — KC868-A8 Porting Notes

## Overview

This document describes the porting of ESP32-PoolMaster from the original ESP32 DevKit
(direct GPIO connections) to the Kincony KC868-A8 hardware.

## Hardware Differences

### Original (ESP32 DevKit)
- 8 Relays directly connected to GPIO pins (32, 33, 25, 26, 27, 4, 13, 23)
- 3 Digital inputs directly on GPIO pins (39, 36, 34)
- I2C bus on GPIO 21 (SDA), 22 (SCL)
- OneWire temperature sensors on GPIO 18, 19
- Buzzer on GPIO 2
- ADS1115 ADC for pH, ORP, PSI measurements

### KC868-A8
- 8 Relays via PCF8574 I2C expander @ 0x24 (ULN2003A driver, active LOW)
- 8 Optocoupler inputs via PCF8574 I2C expander @ 0x22 (active LOW)
- I2C bus on GPIO 4 (SDA), 5 (SCL)
- OneWire temperature sensors on GPIO 13 (Water), 14 (Air)
- Buzzer on GPIO 27
- No onboard ADS1115 (pH/ORP/PSI require simulation or external ADC)

## Pin Mapping

### Relay Outputs (via PCF8574 @ 0x24)

| Virtual Pin | PCF8574 Bit | Relay | Function |
|-------------|-------------|-------|----------|
| 100 | 0 | 1 | FILTRATION |
| 101 | 1 | 2 | ROBOT |
| 102 | 2 | 3 | PH_PUMP |
| 103 | 3 | 4 | CHL_PUMP |
| 104 | 4 | 5 | PROJ (Projector) |
| 105 | 5 | 6 | SPARE |
| 106 | 6 | 7 | SWG_PUMP |
| 107 | 7 | 8 | FILL_PUMP |

**Note:** PCF8574 outputs are active LOW via ULN2003A driver.
- `digitalWrite(pin, HIGH)` → Relay ON → PCF8574 bit = LOW
- `digitalWrite(pin, LOW)` → Relay OFF → PCF8574 bit = HIGH

### Digital Inputs (via PCF8574 @ 0x22)

| Virtual Pin | PCF8574 Bit | Input | Function |
|-------------|-------------|-------|----------|
| 110 | 0 | 1 | CHL_LEVEL (Chlorine tank) |
| 111 | 1 | 2 | PH_LEVEL (Acid tank) |
| 112 | 2 | 3 | POOL_LEVEL (Water level) |
| 113 | 3 | 4 | Not connected |
| 114 | 4 | 5 | Not connected |
| 115 | 5 | 6 | Not connected |
| 116 | 6 | 7 | Not connected |
| 117 | 7 | 8 | Not connected |

**Note:** Optocoupled inputs are active LOW (circuit closed = LOW).

### Direct GPIO

| Function | GPIO | Notes |
|----------|------|-------|
| I2C SDA | 4 | PCF8574 bus |
| I2C SCL | 5 | PCF8574 bus |
| OneWire Water | 13 | DS18B20 temperature |
| OneWire Air | 14 | DS18B20 temperature |
| Buzzer | 27 | Active HIGH |

## Virtual Pin System

The KC868-A8 uses virtual pins (100+) to map PCF8574 I/O to the existing code structure.

### How it works

1. **Pin.cpp** macros redirect `digitalWrite()`, `digitalRead()`, `pinMode()` for virtual pins
2. **KC868A8_IO.cpp** handles PCF8574 communication
3. All existing code (Relay.cpp, Pump.cpp, InputSensor.cpp) works transparently

### Pin.cpp Macros (when KC868_A8 is defined)

```cpp
#define digitalWrite(pin, val) \
  (IS_KC868_VIRTUAL_PIN(pin) ? KC868.digitalWrite(pin, val) : ::digitalWrite(pin, val))

#define digitalRead(pin) \
  (IS_KC868_VIRTUAL_PIN(pin) ? KC868.digitalRead(pin) : ::digitalRead(pin))

#define pinMode(pin, mode) \
  (IS_KC868_VIRTUAL_PIN(pin) ? KC868.pinMode(pin, mode) : ::pinMode(pin, mode))
```

## Sensor Simulation System

### Configuration (Config.h)

```cpp
#define SIMU_CHL_LEVEL      0   // Set to 1 to simulate chlorine tank level
#define SIMU_PH_LEVEL       0   // Set to 1 to simulate acid tank level
#define SIMU_POOL_LEVEL     0   // Set to 1 to simulate pool water level
#define SIMU_PH             0   // Set to 1 to simulate pH sensor
#define SIMU_ORP            0   // Set to 1 to simulate ORP sensor
#define SIMU_PSI            0   // Set to 1 to simulate pressure sensor

// Default simulation values
#define SIMU_PH_VALUE       7.2
#define SIMU_ORP_VALUE      720.0
#define SIMU_PSI_VALUE      0.35
```

### Usage

1. Set `SIMU_*` flags to 1 in Config.h for sensors you want to simulate
2. Optionally adjust `SIMU_*_VALUE` for custom initial values
3. Use `SimSensor.setSimPH()`, `SimSensor.setSimORP()`, etc. at runtime
4. Simulation does not affect real sensor logic when disabled

### API

```cpp
// Get simulated values (returns -1 or NAN if not simulating)
int8_t state = SimSensor.getSimulatedInput(CHL_LEVEL);
double ph = SimSensor.getSimulatedValue(SENSOR_PH);

// Set simulation values at runtime
SimSensor.setSimPH(7.5);
SimSensor.setSimORP(700.0);
```

## TFT Display

The TFT/Nextion display is optional. To enable:

1. Uncomment `#define TFT_CONNECTED` in Config.h
2. Connect Nextion display to Serial2

When `TFT_CONNECTED` is not defined:
- All Nextion code is compiled out
- Stub functions are provided
- No display headers are required

## Build Configuration

### PlatformIO Environments

```ini
[env:kc868_a8]      ; KC868-A8 (default)
[env:esp32_devkit]   ; Original ESP32 DevKit
```

### Build Command

```bash
# For KC868-A8
pio run -e kc868_a8

# For Original ESP32
pio run -e esp32_devkit
```

## Known Limitations

1. **No ADS1115 on KC868-A8**: pH, ORP, and PSI sensors require either:
   - External ADS1115 module connected to I2C bus
   - Sensor simulation enabled in Config.h
   - Custom analog input solution

2. **Status LEDs**: The original PCF8574A @ 0x20 for status LEDs is not available
   on KC868-A8. Status LED functionality is disabled.

3. **Buzzer Pin**: KC868-A8 uses GPIO 27 for buzzer (vs GPIO 2 on original).
   The `BUZZER != 255` guard prevents compilation errors if buzzer is not connected.

4. **PCF8574 Timing**: I2C communication adds ~1ms latency per read/write operation.
   This is negligible for relay switching but should be considered for high-frequency
   operations.

## Testing

### Compile Test
```bash
pio run -e kc868_a8
```

### Functional Test
1. Flash firmware to KC868-A8
2. Check serial output for KC868-A8 initialization messages
3. Verify relays switch via MQTT commands
4. Verify inputs read correctly
5. Test sensor simulation by enabling SIMU_* flags

### MQTT Test
```bash
# Toggle filtration pump
mosquitto_pub -t "Home/Pool/Cmd" -m '{"FiltPump":1}'

# Check status
mosquitto_sub -t "Home/Pool/#"
```

## Files Modified

- `include/Config.h` — Hardware selection, pin definitions, simulation config
- `include/KC868A8_Pins.h` — **NEW** — Virtual pin definitions
- `include/KC868A8_IO.h` — **NEW** — I/O abstraction layer header
- `include/SensorSimulation.h` — **NEW** — Simulation system header
- `src/KC868A8_IO.cpp` — **NEW** — I/O abstraction layer implementation
- `src/SensorSimulation.cpp` — **NEW** — Simulation system implementation
- `src/Setup.cpp` — KC868 I/O init, sensor simulation init
- `src/Loops.cpp` — Buzzer guard
- `src/PoolServer.cpp` — Buzzer guard
- `src/Nextion.cpp` — TFT_CONNECTED guard
- `src/Nextion_Menu.cpp` — TFT_CONNECTED guard
- `src/Nextion_Pages.cpp` — TFT_CONNECTED guard
- `src/Nextion_Events.cpp` — TFT_CONNECTED guard
- `lib/Pump-master/src/Pin.cpp` — KC868 virtual pin redirection
- `platformio.ini` — KC868-A8 build environment

## Author

Ported by OpenClaw AI Assistant for Hubert Mayer
Date: 2026-06-02
