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
 * PID Feedback:
 *   When simulation is active, the PID output drives the simulated
 *   sensor values. The acid pump decreases pH, the chlorine pump
 *   increases ORP. Rates are configurable via SIM_KPH and SIM_KORP.
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

    // Default simulation rate constants
    // KPH:  pH change per ms of PID output per second
    //   At 100% output over 1h (3,600,000 ms): 0.0000028 * 3,600,000 = ~0.10 pH
    //   Suitable for a ~50m3 pool with 1.5 L/h acid pump
    static constexpr double SIM_KPH  = 0.0000028;
    // KORP: ORP change per ms of PID output per second
    //   At 100% output over 1h (3,600,000 ms): 0.00139 * 3,600,000 = ~50 mV
    //   Suitable for a ~50m3 pool with 1.5 L/h chlorine pump
    static constexpr double SIM_KORP = 0.00139;

    SensorSimulation();
    
    // Initialize simulation system
    void begin();
    
    // Update simulation values (called periodically in AnalogPoll)
    // Computes new pH/ORP from PID outputs and returns simulated values
    void loop();
    
    // ============================================================
    // PID Feedback Interface
    // ============================================================
    // Called by AnalogPoll to pass current PID outputs to the simulation.
    // The simulation uses these to drive the simulated sensor values.
    void setPIDOutputs(double phOutput, double orpOutput);
    
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
    
    // PID feedback state
    double last_ph_output;
    double last_orp_output;
    
    // Simulation parameters
    unsigned long last_update;
    unsigned long update_interval;
};

// Global instance
extern SensorSimulation SimSensor;

#endif // SENSOR_SIMULATION_H
