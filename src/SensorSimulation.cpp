/*
 * Sensor Simulation System - Implementation
 * 
 * Each sensor can be independently simulated via Config.h flags.
 * Simulation values are configurable and can be changed at runtime.
 * 
 * When simulation is active, the simulated value replaces the real sensor value.
 * When simulation is not active, the real sensor value is used.
 */

#include "SensorSimulation.h"
#include "Config.h"

// Global instance
SensorSimulation SimSensor;

SensorSimulation::SensorSimulation()
    : simulating_chl_level(false),
      simulating_ph_level(false),
      simulating_pool_level(false),
      simulating_ph(false),
      simulating_orp(false),
      simulating_psi(false),
      sim_ph_value(SIMU_PH_VALUE),
      sim_orp_value(SIMU_ORP_VALUE),
      sim_psi_value(SIMU_PSI_VALUE),
      sim_chl_level(SIMU_CHL_LEVEL_VALUE),
      sim_ph_level(SIMU_PH_LEVEL_VALUE),
      sim_pool_level(SIMU_POOL_LEVEL_VALUE),
      last_update(0),
      update_interval(1000) {  // Update every second
}

void SensorSimulation::begin() {
    // Set simulation flags from Config.h defines
    simulating_chl_level = SIMU_CHL_LEVEL;
    simulating_ph_level = SIMU_PH_LEVEL;
    simulating_pool_level = SIMU_POOL_LEVEL;
    simulating_ph = SIMU_PH;
    simulating_orp = SIMU_ORP;
    simulating_psi = SIMU_PSI;
    
    last_update = millis();
    
    Serial.println("[SensorSim] Initialized:");
    Serial.printf("  CHL_LEVEL: %s (value: %s)\n", 
        simulating_chl_level ? "SIMULATED" : "REAL",
        sim_chl_level ? "HIGH" : "LOW");
    Serial.printf("  PH_LEVEL: %s (value: %s)\n",
        simulating_ph_level ? "SIMULATED" : "REAL",
        sim_ph_level ? "HIGH" : "LOW");
    Serial.printf("  POOL_LEVEL: %s (value: %s)\n",
        simulating_pool_level ? "SIMULATED" : "REAL",
        sim_pool_level ? "HIGH" : "LOW");
    Serial.printf("  pH: %s (value: %.2f)\n",
        simulating_ph ? "SIMULATED" : "REAL", sim_ph_value);
    Serial.printf("  ORP: %s (value: %.1f)\n",
        simulating_orp ? "SIMULATED" : "REAL", sim_orp_value);
    Serial.printf("  PSI: %s (value: %.3f)\n",
        simulating_psi ? "SIMULATED" : "REAL", sim_psi_value);
}

void SensorSimulation::loop() {
    unsigned long now = millis();
    if (now - last_update < update_interval) return;
    last_update = now;
    
    // Optional: Add dynamic simulation behavior here
    // For example, slowly ramp values or add noise
}

int8_t SensorSimulation::getSimulatedInput(uint8_t pin) {
    // CHL_LEVEL
    if (pin == CHL_LEVEL && simulating_chl_level) {
        return sim_chl_level ? HIGH : LOW;
    }
    // PH_LEVEL
    if (pin == PH_LEVEL && simulating_ph_level) {
        return sim_ph_level ? HIGH : LOW;
    }
    // POOL_LEVEL
    if (pin == POOL_LEVEL && simulating_pool_level) {
        return sim_pool_level ? HIGH : LOW;
    }
    
    // No simulation active for this pin
    return -1;
}

double SensorSimulation::getSimulatedValue(uint8_t sensorType) {
    switch (sensorType) {
        case SENSOR_PH:
            return simulating_ph ? sim_ph_value : NAN;
        case SENSOR_ORP:
            return simulating_orp ? sim_orp_value : NAN;
        case SENSOR_PSI:
            return simulating_psi ? sim_psi_value : NAN;
        default:
            return NAN;
    }
}

// ============================================================
// Manual Value Setters
// ============================================================

void SensorSimulation::setSimPH(double value) {
    sim_ph_value = value;
}

void SensorSimulation::setSimORP(double value) {
    sim_orp_value = value;
}

void SensorSimulation::setSimPSI(double value) {
    sim_psi_value = value;
}

void SensorSimulation::setSimChlLevel(bool active) {
    sim_chl_level = active;
}

void SensorSimulation::setSimPHLevel(bool active) {
    sim_ph_level = active;
}

void SensorSimulation::setSimPoolLevel(bool active) {
    sim_pool_level = active;
}

// ============================================================
// Status
// ============================================================

bool SensorSimulation::isSimulating(uint8_t sensorType) {
    switch (sensorType) {
        case SENSOR_PH:         return simulating_ph;
        case SENSOR_ORP:        return simulating_orp;
        case SENSOR_PSI:        return simulating_psi;
        case SENSOR_CHL_LEVEL:  return simulating_chl_level;
        case SENSOR_PH_LEVEL:   return simulating_ph_level;
        case SENSOR_POOL_LEVEL: return simulating_pool_level;
        default:                return false;
    }
}

void SensorSimulation::printStatus() {
    Serial.println("[SensorSim] Status:");
    Serial.printf("  CHL_LEVEL: %s (value: %s)\n", 
        simulating_chl_level ? "SIM" : "REAL",
        sim_chl_level ? "HIGH" : "LOW");
    Serial.printf("  PH_LEVEL: %s (value: %s)\n",
        simulating_ph_level ? "SIM" : "REAL",
        sim_ph_level ? "HIGH" : "LOW");
    Serial.printf("  POOL_LEVEL: %s (value: %s)\n",
        simulating_pool_level ? "SIM" : "REAL",
        sim_pool_level ? "HIGH" : "LOW");
    Serial.printf("  pH: %s (value: %.2f)\n",
        simulating_ph ? "SIM" : "REAL", sim_ph_value);
    Serial.printf("  ORP: %s (value: %.1f)\n",
        simulating_orp ? "SIM" : "REAL", sim_orp_value);
    Serial.printf("  PSI: %s (value: %.3f)\n",
        simulating_psi ? "SIM" : "REAL", sim_psi_value);
}
