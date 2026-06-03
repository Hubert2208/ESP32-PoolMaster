#ifndef SENSOR_SIMULATION_H
#define SENSOR_SIMULATION_H

#include <Arduino.h>
#include <Preferences.h>

// Forward declarations for KC868-A8 I/O functions
// These are implemented in KC868A8_IO.cpp
void kc868_a8_init();
uint8_t kc868_a8_digitalRead(uint8_t pin);
void kc868_a8_digitalWrite(uint8_t pin, uint8_t value);

// ADS1115 multiplexer channel constants (for analog simulation)
#define ADS1115_MUX_CH0 0x4000  // AIN0
#define ADS1115_MUX_CH1 0x5000  // AIN1
#define ADS1115_MUX_CH2 0x6000  // AIN2
#define ADS1115_MUX_CH3 0x7000  // AIN3

// Default simulated values when no sensor is connected
#define SIMU_PH_DEFAULT    7.0   // Neutral pH
#define SIMU_ORP_DEFAULT  700.0  // Typical ORP value (mV)
#define SIMU_PSI_DEFAULT    1.5  // Typical pool pressure (bar)
#define SIMU_CHL_DEFAULT  700.0  // Typical chlorine ORP (mV)

class SensorSimulation {
public:
    // Sensor index constants for analog simulation (used with getSimulatedValue)
    static const uint8_t SENSOR_PH = 0;
    static const uint8_t SENSOR_ORP = 1;
    static const uint8_t SENSOR_PSI = 2;
    static const uint8_t SENSOR_CHL = 3;  // Chlorine (alias for ORP in some configs)

private:
    // Simulation level per sensor type
    // 0 = disabled (use real sensor), 1 = enabled (use simulated value)
    uint8_t simuPHLevel;
    uint8_t simuORPLevel;
    uint8_t simuPSILevel;
    uint8_t simuCHLLevel;

    // Simulated values (configurable via MQTT or NVS)
    double simuPHValue;
    double simuORPValue;
    double simuPSIValue;
    double simuCHLValue;

    // Digital input simulation states (pin-based)
    // Maps KC868-A8 digital pin to simulated state
    static const uint8_t MAX_SIMU_PINS = 16;
    int8_t simuDigitalPins[MAX_SIMU_PINS];  // -1 = not simulated, 0/1 = simulated state

    // Preferences key prefix
    static const char* NVS_NAMESPACE;

public:
    SensorSimulation();

    // Initialize from NVS (saved settings)
    void begin();

    // Set simulation level for a sensor type
    // level: 0 = disabled, 1 = enabled
    void setSimuLevel(uint8_t sensorType, uint8_t level);

    // Get simulation level for a sensor type
    uint8_t getSimuLevel(uint8_t sensorType) const;

    // Set simulated value for a sensor
    void setSimuValue(uint8_t sensorType, double value);

    // Get simulated value for a sensor
    // Returns the simulated value if simulation is enabled for this sensor,
    // otherwise returns NAN (not a number) to indicate "use real sensor"
    double getSimulatedValue(uint8_t sensorType) const;

    // Simulate a digital input pin
    // pin: KC868-A8 digital pin number
    // state: 0 or 1
    void setSimulatedInput(uint8_t pin, uint8_t state);

    // Get simulated digital input state
    // Returns: 0 or 1 if simulated, -1 if not simulated (use real sensor)
    int8_t getSimulatedInput(uint8_t pin) const;

    // Check if any simulation is active
    bool isAnySimulationActive() const;

    // Save current settings to NVS
    void saveSettings();

    // Load settings from NVS
    void loadSettings();

    // Reset all simulations to defaults
    void resetToDefaults();

    // Get sensor type from string (for MQTT commands)
    static uint8_t sensorTypeFromString(const char* str);

    // Get sensor type as string (for MQTT status)
    static const char* sensorTypeToString(uint8_t sensorType);

    // Debug: print current simulation state
    void printStatus() const;
};

// Global instance
extern SensorSimulation SimSensor;

#endif // SENSOR_SIMULATION_H
