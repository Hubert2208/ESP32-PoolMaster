/*
 * KC868-A8 I/O Abstraction Layer
 * 
 * Provides digitalWrite/digitalRead/pinMode redirection for KC868-A8 hardware.
 * Virtual pins (100+) are mapped to PCF8574 I2C expanders.
 * 
 * Relay outputs (100-107): PCF8574 @ 0x24, active HIGH via ULN2003A
 *   - digitalWrite/digitalRead are INVERTED for relay pins to match
 *     the ACTIVE_LOW convention used by the Pump-master library:
 *     digitalWrite(pin, 0) → relay ON  (value LOW = active)
 *     digitalWrite(pin, 1) → relay OFF (value HIGH = inactive)
 *     digitalRead(pin) returns 0 when relay ON, 1 when OFF
 *   - Use setRelay()/getRelay() for direct (non-inverted) hardware control
 * 
 * Digital inputs (110-117): PCF8574 @ 0x22, active LOW
 *   - digitalRead(pin) → HIGH = inactive, LOW = active (optocoupler triggered)
 *   - For compatibility with original code, we invert: digitalRead returns
 *     the ACTUAL state (HIGH when optocoupler active, LOW when inactive)
 */

#ifndef KC868A8_IO_H
#define KC868A8_IO_H

#include <Arduino.h>
#include <Wire.h>
#include "KC868A8_Pins.h"

class KC868A8_IO {
public:
    KC868A8_IO();
    
    // Initialize I2C and read initial states
    void begin();
    
    // Digital I/O for virtual pins
    // Note: relay pins (100-107) use inverted logic (see above)
    void digitalWrite(uint8_t pin, uint8_t value);
    uint8_t digitalRead(uint8_t pin);
    void pinMode(uint8_t pin, uint8_t mode);
    
    // Relay control (direct, non-inverted — for Setup.cpp)
    void setRelay(uint8_t relayIndex, bool on);
    bool getRelay(uint8_t relayIndex);
    
    // Input control (direct, for Setup.cpp)
    bool getInput(uint8_t inputIndex);
    
    // Debug: print current states
    void printStatus();

private:
    void readInputs();
    void writeOutputs();
    
    uint8_t relayState;      // Current relay output state (bitmask)
    uint8_t inputState;      // Current input state (bitmask, inverted from hardware)
    uint8_t lastInputState;  // Previous input state for change detection
    
    bool initialized;
};

// Global instance
extern KC868A8_IO KC868;

// ============================================================
// KC868-A8 Redirection Macros
// ============================================================
// These macros redirect digitalWrite/digitalRead/pinMode calls
// through the KC868 I/O layer for virtual pins.

#ifdef KC868_A8
  // Save originals (they're macros in Arduino.h)
  #define KC868_ORIG_DIGITAL_WRITE  digitalWrite
  #define KC868_ORIG_DIGITAL_READ   digitalRead
  #define KC868_ORIG_PIN_MODE       pinMode
  
  // Redefine for KC868-A8 virtual pin support
  #define digitalWrite(pin, val) \
    (IS_KC868_VIRTUAL_PIN(pin) ? KC868.digitalWrite(pin, val) : KC868_ORIG_DIGITAL_WRITE(pin, val))
  
  #define digitalRead(pin) \
    (IS_KC868_VIRTUAL_PIN(pin) ? KC868.digitalRead(pin) : KC868_ORIG_DIGITAL_READ(pin))
  
  #define pinMode(pin, mode) \
    (IS_KC868_VIRTUAL_PIN(pin) ? KC868.pinMode(pin, mode) : KC868_ORIG_PIN_MODE(pin, mode))
#endif

#endif // KC868A8_IO_H
