/*
 * Sensor Simulation System - Implementation
 * 
 * Each sensor can be independently simulated via Config.h flags.
 * Simulation values are configurable and can be changed at runtime.
 * 
 * When simulation is active, the simulated value replaces the real sensor value.
 * When simulation is not active, the real sensor value is used.
 * 
 * Feedback Loop:
 * - Call setPhPumpActive()/setOrpPumpActive() from PID controllers
 * - loop() adjusts simulated values based on pump runtime
 * - SIM_KPH controls pH change rate (negative = pH drops when pump runs)
 * - SIM_KORP controls ORP change rate (positive = ORP rises when pump runs)
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
      update_interval(1000),  // Update every second
      ph_pump_active(false),
      orp_pump_active(false),
      last_feedback_update(0),
      ph_pump_start_time(0),
      orp_pump_start_time(0),
      ph_pump_total_seconds(0),
      orp_pump_total_seconds(0)
{
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
    last_feedback_update = millis();
    
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
    Serial.printf("  Feedback: SIM_KPH=%.4f, SIM_KORP=%.2f\n",
        (double)SIM_KPH, (double)SIM_KORP);
}

void SensorSimulation::loop() {
    unsigned long now = millis();
    if (now - last_update < update_interval) return;
    last_update = now;
    
    // Update simulation feedback (adjust values based on pump activity)
    updateSimulationFeedback();
    
    // Periodic status output (every 30 seconds)
    static unsigned long last_status = 0;
    if (now - last_status > 30000) {
        last_status = now;
        if (simulating_ph || simulating_orp) {
            printStatus();
        }
    }
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
// PID Feedback Interface
// ============================================================

void SensorSimulation::setPhPumpActive(bool active) {
    unsigned long now = millis();
    
    if (active && !ph_pump_active) {
        // Pump just started
        ph_pump_start_time = now;
        Serial.printf("[SensorSim] pH pump STARTED at pH=%.4f\n", sim_ph_value);
    }
    else if (!active && ph_pump_active) {
        // Pump just stopped - accumulate runtime
        double runtime = (now - ph_pump_start_time) / 1000.0;
        ph_pump_total_seconds += runtime;
        Serial.printf("[SensorSim] pH pump STOPPED (ran %.1f sec, total=%.1f sec)\n", 
                      runtime, ph_pump_total_seconds);
    }
    
    ph_pump_active = active;
}

void SensorSimulation::setOrpPumpActive(bool active) {
    unsigned long now = millis();
    
    if (active && !orp_pump_active) {
        // Pump just started
        orp_pump_start_time = now;
        Serial.printf("[SensorSim] ORP pump STARTED at ORP=%.1f\n", sim_orp_value);
    }
    else if (!active && orp_pump_active) {
        // Pump just stopped - accumulate runtime
        double runtime = (now - orp_pump_start_time) / 1000.0;
        orp_pump_total_seconds += runtime;
        Serial.printf("[SensorSim] ORP pump STOPPED (ran %.1f sec, total=%.1f sec)\n", 
                      runtime, orp_pump_total_seconds);
    }
    
    orp_pump_active = active;
}

void SensorSimulation::setPIDOutputs(double phOutput, double orpOutput, double threshold) {
    // Update pump states based on PID outputs
    setPhPumpActive(phOutput > threshold);
    setOrpPumpActive(orpOutput > threshold);
}

// ============================================================
// Feedback Calculation
// ============================================================

void SensorSimulation::updateSimulationFeedback() {
    unsigned long now = millis();
    double dt = (now - last_feedback_update) / 1000.0;  // Convert to seconds
    last_feedback_update = now;
    
    // Only update if simulation is active
    if (!simulating_ph && !simulating_orp) return;
    
    // Limit dt to prevent huge jumps (e.g., after sleep)
    if (dt > 5.0) dt = 1.0;
    
    // ============================================================
    // pH Simulation Feedback
    // ============================================================
    if (simulating_ph && ph_pump_active) {
        // Acid pump is running -> pH decreases
        // SIM_KPH is negative (e.g., -0.001), so we add it directly
        double delta = SIM_KPH * dt;
        sim_ph_value += delta;
        
        // Clamp to realistic limits
        if (sim_ph_value < SIM_PH_MIN) {
            sim_ph_value = SIM_PH_MIN;
            Serial.println("[SensorSim] pH hit MIN limit");
        }
        if (sim_ph_value > SIM_PH_MAX) {
            sim_ph_value = SIM_PH_MAX;
            Serial.println("[SensorSim] pH hit MAX limit");
        }
        
        // Debug output (every 10 seconds when pump is active)
        static unsigned long last_ph_debug = 0;
        if (now - last_ph_debug > 10000) {
            last_ph_debug = now;
            Serial.printf("[SensorSim] pH: %.4f (Δ%.4f over %.1fs)\n", 
                          sim_ph_value, delta, dt);
        }
    }
    
    // ============================================================
    // ORP Simulation Feedback
    // ============================================================
    if (simulating_orp && orp_pump_active) {
        // Chlorine pump is running -> ORP increases
        // SIM_KORP is positive (e.g., 0.5), so we add it directly
        double delta = SIM_KORP * dt;
        sim_orp_value += delta;
        
        // Clamp to realistic limits
        if (sim_orp_value < SIM_ORP_MIN) {
            sim_orp_value = SIM_ORP_MIN;
            Serial.println("[SensorSim] ORP hit MIN limit");
        }
        if (sim_orp_value > SIM_ORP_MAX) {
            sim_orp_value = SIM_ORP_MAX;
            Serial.println("[SensorSim] ORP hit MAX limit");
        }
        
        // Debug output (every 10 seconds when pump is active)
        static unsigned long last_orp_debug = 0;
        if (now - last_orp_debug > 10000) {
            last_orp_debug = now;
            Serial.printf("[SensorSim] ORP: %.2f (Δ%.2f over %.1fs)\n", 
                          sim_orp_value, delta, dt);
        }
    }
}

// ============================================================
// Manual Value Setters
// ============================================================

void SensorSimulation::setSimPH(double value) {
    sim_ph_value = value;
    Serial.printf("[SensorSim] pH manually set to %.4f\n", value);
}

void SensorSimulation::setSimORP(double value) {
    sim_orp_value = value;
    Serial.printf("[SensorSim] ORP manually set to %.1f\n", value);
}

void SensorSimulation::setSimPSI(double value) {
    sim_psi_value = value;
    Serial.printf("[SensorSim] PSI manually set to %.3f\n", value);
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
    Serial.printf("  pH: %s (value: %.4f)\n",
        simulating_ph ? "SIM" : "REAL", sim_ph_value);
    Serial.printf("  ORP: %s (value: %.2f)\n",
        simulating_orp ? "SIM" : "REAL", sim_orp_value);
    Serial.printf("  PSI: %s (value: %.3f)\n",
        simulating_psi ? "SIM" : "REAL", sim_psi_value);
    Serial.printf("  Pump State: pH=%s, ORP=%s\n",
        ph_pump_active ? "RUNNING" : "STOPPED",
        orp_pump_active ? "RUNNING" : "STOPPED");
    Serial.printf("  Pump Runtime: pH=%.1fs, ORP=%.1fs\n",
        ph_pump_total_seconds, orp_pump_total_seconds);
}
