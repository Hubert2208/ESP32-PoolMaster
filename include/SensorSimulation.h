/*
 * Sensor Simulation System for ESP32-PoolMaster
 * 
 * Provides individual simulation for each sensor type.
 * Each sensor can be independently enabled/disabled via Config.h flags.
 * 
 * Supported sensors:
 *   - CHL_LEVEL: Chlorine tank level (digital input)
 *   - PH_LEVEL: Acid tank level (digital input)
 *   - POOL_LEVEL: Pool water level (digital input)
 *   - pH: pH sensor (analog via ADS1115)
 *   - ORP: ORP sensor (analog via ADS1115)
 *   - PSI: Pressure sensor (analog via ADS1115)
 * 
 * Usage:
 *   1. Set SIMU_* flags in Config.h to enable simulation for specific sensors
 *   2. Optionally set SIMU_*_VALUE for custom initial values
 *   3. If a sensor is not physically connected, simulation auto-engages
 *   4. Simulation does not affect real sensor logic when disabled
 */

#ifndef SENSOR_SIMULATION_H
#define SENSOR_SIMULATION_H

#include <Arduino.h>

class SensorSimulation {
public:
    // Sensor type constants (public for use in AnalogPoll and other modules)
    static const uint8_t SENSOR_PH = 0;
    static const uint8_t SENSOR_ORP = 1;
    static const uint8_t SENSOR_PSI = 2;
    static const uint8_t SENSOR_CHL_LEVEL = 3;
    static const uint8_t SENSOR_PH_LEVEL = 4;
    static const uint8_t SENSOR_POOL_LEVEL = 5;

    SensorSimulation();
    
    // Initialize simulation system
    void begin();
    
    // Update simulation values (called periodically)
    void loop();
    
    // ============================================================
    // Digital Input Simulation (CHL_LEVEL, PH_LEVEL, POOL_LEVEL)
    // ============================================================
    // Returns simulated state for digital inputs
    // Returns -1 if simulation is not active (use real sensor)
    int8_t getSimulatedInput(uint8_t pin);
    
    // ============================================================
    // Analog Sensor Simulation (pH, ORP, PSI)
    // ============================================================
    // Returns simulated value for analog sensors
    // Returns NaN if simulation is not active (use real sensor)
    double getSimulatedValue(uint8_t sensorType);
    
    // ============================================================
    // Manual Value Setters (for runtime adjustment)
    // ============================================================
    void setSimPH(double value);
    void setSimORP(double value);
    void setSimPSI(double value);
    void setSimChlLevel(bool active);
    void setSimPHLevel(bool active);
    void setSimPoolLevel(bool active);
    
    // ============================================================
    // Analog Simulation Update (called every 60s by AnalogSimLoop)
    // ============================================================
    // Checks pump states and adjusts pH/ORP values accordingly.
    void updateAnalogSimulation();
    
    // ============================================================
    // Status
    // ============================================================
    bool isSimulating(uint8_t sensorType);
    void printStatus();

private:
    // Simulation state
    bool simulating_chl_level;
    bool simulating_ph_level;
    bool simulating_pool_level;
    bool simulating_ph;
    bool simulating_orp;
    bool simulating_psi;
    
    // Simulated values
    double sim_ph_value;
    double sim_orp_value;
    double sim_psi_value;
    bool sim_chl_level;
    bool sim_ph_level;
    bool sim_pool_level;
    
    // Simulation parameters
    unsigned long last_update;
    unsigned long update_interval;
};

// Global instance
extern SensorSimulation SimSensor;

#endif // SENSOR_SIMULATION_H
