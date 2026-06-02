/*
 * KC868-A8 I/O Abstraction Layer - Implementation
 * 
 * Hardware:
 *   PCF8574 @ 0x24: 8-bit I/O expander for relay outputs
 *     - ULN2003A driver: LOW on PCF8574 = relay ON, HIGH = relay OFF
 *     - Active LOW logic
 * 
 *   PCF8574 @ 0x22: 8-bit I/O expander for digital inputs
 *     - Optocoupled inputs: LOW when triggered (circuit closed)
 *     - Active LOW logic
 * 
 * Virtual Pin Mapping:
 *   100-107: Relay outputs (PCF8574 @ 0x24, bits 0-7)
 *   110-117: Digital inputs (PCF8574 @ 0x22, bits 0-7)
 */

#include "KC868A8_IO.h"

// Global instance
KC868A8_IO KC868;

KC868A8_IO::KC868A8_IO() 
    : relayState(0xFF),  // All relays OFF initially (active LOW, so HIGH = OFF)
      inputState(0x00),
      lastInputState(0x00),
      initialized(false) {
}

void KC868A8_IO::begin() {
    // I2C should already be initialized (Wire.begin in Setup.cpp)
    // Initialize relay outputs - all OFF
    relayState = 0xFF;  // All HIGH = all relays OFF
    Wire.beginTransmission(KC868_RELAY_I2C_ADDR);
    Wire.write(relayState);
    Wire.endTransmission();
    
    // Initialize inputs - read current state
    readInputs();
    lastInputState = inputState;
    
    initialized = true;
    
    Serial.println("[KC868-A8] I/O initialized: relays OFF, inputs read");
}

void KC868A8_IO::readInputs() {
    Wire.requestFrom(KC868_INPUT_I2C_ADDR, (uint8_t)1);
    if (Wire.available()) {
        uint8_t raw = Wire.read();
        // Optocoupled inputs are active LOW
        // We store the inverted value so HIGH means "active"
        inputState = ~raw;
    }
}

void KC868A8_IO::writeOutputs() {
    Wire.beginTransmission(KC868_RELAY_I2C_ADDR);
    Wire.write(relayState);
    Wire.endTransmission();
}

void KC868A8_IO::digitalWrite(uint8_t pin, uint8_t value) {
    if (!initialized) return;
    
    if (IS_KC868_RELAY_PIN(pin)) {
        uint8_t bit = pin - 100;
        if (value) {
            // Relay ON → PCF8574 bit LOW (active LOW)
            relayState &= ~(1 << bit);
        } else {
            // Relay OFF → PCF8574 bit HIGH
            relayState |= (1 << bit);
        }
        writeOutputs();
    }
    // Input pins: digitalWrite has no effect (read-only)
}

uint8_t KC868A8_IO::digitalRead(uint8_t pin) {
    if (!initialized) return 0;
    
    if (IS_KC868_INPUT_PIN(pin)) {
        readInputs();
        uint8_t bit = pin - 110;
        return (inputState >> bit) & 1;
    }
    
    if (IS_KC868_RELAY_PIN(pin)) {
        // Read current relay state
        uint8_t bit = pin - 100;
        return !((relayState >> bit) & 1);  // Invert: HIGH = relay ON
    }
    
    return 0;
}

void KC868A8_IO::pinMode(uint8_t pin, uint8_t mode) {
    // Virtual pins don't need pinMode - managed by KC868 I/O layer
    // This is intentionally empty
}

void KC868A8_IO::setRelay(uint8_t relayIndex, bool on) {
    if (relayIndex > 7) return;
    
    if (on) {
        relayState &= ~(1 << relayIndex);   // LOW = relay ON
    } else {
        relayState |= (1 << relayIndex);    // HIGH = relay OFF
    }
    writeOutputs();
}

bool KC868A8_IO::getRelay(uint8_t relayIndex) {
    if (relayIndex > 7) return false;
    return !((relayState >> relayIndex) & 1);  // Invert: true = ON
}

bool KC868A8_IO::getInput(uint8_t inputIndex) {
    if (inputIndex > 7) return false;
    readInputs();
    return (inputState >> inputIndex) & 1;  // true = active
}

void KC868A8_IO::printStatus() {
    Serial.printf("[KC868-A8] Relays: 0x%02X, Inputs: 0x%02X\n", relayState, inputState);
}
