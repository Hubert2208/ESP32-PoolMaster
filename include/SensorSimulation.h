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
 * 
 * Feedback Loop:
 *   When PID controllers compute outputs, call setPhPumpActive()/setOrpPumpActive()
 *   to update the simulation. The loop() method will then adjust simulated values
 *   based on pump runtime (SIM_KPH for pH, SIM_KORP for ORP).
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
    // PID Feedback Interface (for dynamic pH/ORP simulation)
    // ============================================================
    // Call these from pHRegulation() and OrpRegulation() after PID.Compute()
    // to inform the simulation about pump activity.
    
    // Update pH pump state (called from pHRegulation)
    // active = true when pump is ON (PhPIDOutput > threshold)
    void setPhPumpActive(bool active);
    
    // Update ORP pump state (called from OrpRegulation)
    // active = true when pump is ON (OrpPIDOutput > threshold)
    void setOrpPumpActive(bool active);
    
    // Combined setter (alternative to individual calls)
    void setPIDOutputs(double phOutput, double orpOutput, double threshold = 30000.0);
    
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
    
    // ============================================================
    // Statistics (for debugging)
    // ============================================================
    double getPhPumpTotalSeconds() const { return ph_pump_total_seconds; }
    double getOrpPumpTotalSeconds() const { return orp_pump_total_seconds; }

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
    
    // ============================================================
    // PID Feedback State
    // ============================================================
    bool ph_pump_active;          // pH pump currently running?
    bool orp_pump_active;         // ORP pump currently running?
    unsigned long last_feedback_update;  // Last feedback calculation time
    
    // For tracking pump runtime
    unsigned long ph_pump_start_time;    // When pump started
    unsigned long orp_pump_start_time;   // When pump started
    double ph_pump_total_seconds;        // Total runtime (for statistics)
    double orp_pump_total_seconds;
    
    // Internal feedback calculation
    void updateSimulationFeedback();
};

// Global instance
extern SensorSimulation SimSensor;

#endif // SENSOR_SIMULATION_H
