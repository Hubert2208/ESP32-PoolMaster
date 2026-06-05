/*
 * Sensor Simulation System - Implementation
 * 
 * Each sensor can be independently simulated via Config.h flags.
 * Simulation values are configurable and can be changed at runtime.
 * 
 * Analog simulation (pH/ORP) is driven by pump states:
 *   - Acid pump ON  -> pH decreases (acid effect)
 *   - Acid pump OFF -> pH increases (natural drift)
 *   - Chlorine pump ON  -> ORP increases (chlorine effect)
 *   - Chlorine pump OFF -> ORP decreases (natural drift)
 */

#include "SensorSimulation.h"
#include "Config.h"

#ifdef KC868_A8
  #include "PoolMaster.h"  // For PhPump, ChlPump extern declarations
#endif

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

// ============================================================
// Analog Simulation Update - Called every 60s by AnalogSimLoop
// ============================================================
// Checks pump states and adjusts pH/ORP values accordingly:
//   - Acid pump ON  -> pH decreases (acid effect)
//   - Acid pump OFF -> pH increases (natural drift)
//   - Chlorine pump ON  -> ORP increases (chlorine effect)
//   - Chlorine pump OFF -> ORP decreases (natural drift)
// Values are clamped to SIM_PH_MIN/MAX and SIM_ORP_MIN/MAX.
// ============================================================
void SensorSimulation::updateAnalogSimulation() {
#ifdef KC868_A8
    // --- pH Simulation ---
    if (simulating_ph) {
        if (PhPump.IsRunning()) {
            // Acid pump active: pH decreases
            sim_ph_value -= SIM_PH_ACTIVE_RATE;
            Debug.print(DBG_INFO, "[Sim] pH decreasing (pump ON): %.3f", sim_ph_value);
        } else {
            // Acid pump off: pH drifts up naturally
            sim_ph_value += SIM_PH_DRIFT_RATE;
            Debug.print(DBG_VERBOSE, "[Sim] pH drifting up (pump OFF): %.3f", sim_ph_value);
        }
        // Clamp to configured bounds
        if (sim_ph_value < SIM_PH_MIN) sim_ph_value = SIM_PH_MIN;
        if (sim_ph_value > SIM_PH_MAX) sim_ph_value = SIM_PH_MAX;
    }

    // --- ORP Simulation ---
    if (simulating_orp) {
        if (ChlPump.IsRunning()) {
            // Chlorine pump active: ORP increases
            sim_orp_value += SIM_ORP_ACTIVE_RATE;
            Debug.print(DBG_INFO, "[Sim] ORP increasing (pump ON): %.1f", sim_orp_value);
        } else {
            // Chlorine pump off: ORP drifts down naturally
            sim_orp_value -= SIM_ORP_DRIFT_RATE;
            Debug.print(DBG_VERBOSE, "[Sim] ORP drifting down (pump OFF): %.1f", sim_orp_value);
        }
        // Clamp to configured bounds
        if (sim_orp_value < SIM_ORP_MIN) sim_orp_value = SIM_ORP_MIN;
        if (sim_orp_value > SIM_ORP_MAX) sim_orp_value = SIM_ORP_MAX;
    }
#endif
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
