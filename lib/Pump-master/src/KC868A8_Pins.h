/*
 * KC868-A8 Pin Definitions
 * Hardware: Kincony KC868-A8 (ESP32 + 8 Relays via PCF8574 + 8 Optokoppler Inputs via PCF8574)
 * 
 * I2C Addresses:
 *   0x24 - PCF8574 for 8x Relay outputs (active LOW via ULN2003A)
 *   0x22 - PCF8574 for 8x Digital Inputs (active LOW, optocoupled)
 * 
 * I2C Bus:
 *   GPIO4 - SDA
 *   GPIO5 - SCL
 * 
 * 1-Wire:
 *   GPIO13 - Water temperature (DS18B20)
 *   GPIO14 - Air temperature (DS18B20)
 * 
 * Virtual Pin Mapping (100+):
 *   100-107 - Relay outputs via PCF8574@0x24
 *   110-117 - Digital inputs via PCF8574@0x22
 * 
 * Note: GPIO227 errors in serial log are harmless WiFi/BT stack artifacts.
 */

#ifndef KC868A8_PINS_H
#define KC868A8_PINS_H

// ============================================================
// KC868-A8 I2C Addresses
// ============================================================
#define KC868_RELAY_I2C_ADDR    0x24    // PCF8574 for relay outputs
#define KC868_INPUT_I2C_ADDR    0x22    // PCF8574 for digital inputs

// ============================================================
// KC868-A8 I2C Bus Pins
// ============================================================
#define KC868_I2C_SDA           4       // GPIO4
#define KC868_I2C_SCL           5       // GPIO5

// ============================================================
// KC868-A8 1-Wire Pins (direct GPIO)
// ============================================================
#define KC868_ONEWIRE_WATER     13      // GPIO13 - Water temperature
#define KC868_ONEWIRE_AIR       14      // GPIO14 - Air temperature

// ============================================================
// KC868-A8 Buzzer
// ============================================================
#define KC868_BUZZER            27      // GPIO27 - Buzzer output

// ============================================================
// Virtual Pin Mapping - Relay Outputs (active LOW via ULN2003A)
// ============================================================
// Mapping: VirtualPin -> PCF8574 bit -> Relay
//   100 -> bit 0 -> Relay 1 (FILTRATION)
//   101 -> bit 1 -> Relay 2 (ROBOT)
//   102 -> bit 2 -> Relay 3 (PH_PUMP)
//   103 -> bit 3 -> Relay 4 (CHL_PUMP)
//   104 -> bit 4 -> Relay 5 (PROJ/Projector)
//   105 -> bit 5 -> Relay 6 (SPARE)
//   106 -> bit 6 -> Relay 7 (SWG_PUMP)
//   107 -> bit 7 -> Relay 8 (FILL_PUMP)

#define VPIN_FILTRATION         100     // Relay 1
#define VPIN_ROBOT              101     // Relay 2
#define VPIN_PH_PUMP            102     // Relay 3
#define VPIN_CHL_PUMP           103     // Relay 4
#define VPIN_PROJ               104     // Relay 5 (Projector)
#define VPIN_SPARE              105     // Relay 6
#define VPIN_SWG_PUMP           106     // Relay 7
#define VPIN_FILL_PUMP          107     // Relay 8

// ============================================================
// Virtual Pin Mapping - Digital Inputs (active LOW, optocoupled)
// ============================================================
// Mapping: VirtualPin -> PCF8574 bit -> Input
//   110 -> bit 0 -> Input 1
//   111 -> bit 1 -> Input 2
//   112 -> bit 2 -> Input 3
//   113 -> bit 3 -> Input 4
//   114 -> bit 4 -> Input 5
//   115 -> bit 5 -> Input 6
//   116 -> bit 6 -> Input 7
//   117 -> bit 7 -> Input 8

#define VPIN_CHL_LEVEL          110     // Input 1 (Chlorine tank level)
#define VPIN_PH_LEVEL           111     // Input 2 (Acid tank level)
#define VPIN_POOL_LEVEL         112     // Input 3 (Pool water level)
#define VPIN_INPUT4             113     // Input 4 (not connected)
#define VPIN_INPUT5             114     // Input 5 (not connected)
#define VPIN_INPUT6             115     // Input 6 (not connected)
#define VPIN_INPUT7             116     // Input 7 (not connected)
#define VPIN_INPUT8             117     // Input 8 (not connected)

// ============================================================
// Pin Range Check Macros
// ============================================================
#define IS_KC868_RELAY_PIN(pin)  ((pin) >= 100 && (pin) <= 107)
#define IS_KC868_INPUT_PIN(pin)  ((pin) >= 110 && (pin) <= 117)
#define IS_KC868_VIRTUAL_PIN(pin) (IS_KC868_RELAY_PIN(pin) || IS_KC868_INPUT_PIN(pin))

#endif // KC868A8_PINS_H
