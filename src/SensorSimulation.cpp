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
 * - When pump is OFF: pH drifts UP naturally, ORP drifts DOWN naturally
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
      update_interval(1000),
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
    
    // Periodic status output (every 60 seconds)
    static unsigned long last_status = 0;
    if (now - last_status > 60000) {
        last_status = now;
        if (simulating_ph || simulating_orp) {
            printStatus();
        }
    }
}

int8_t SensorSimulation::getSimulatedInput(uint8_t pin) {
    if (pin == CHL_LEVEL && simulating_chl_level) {
        return sim_chl_level ? HIGH : LOW;
    }
    if (pin == PH_LEVEL && simulating_ph_level) {
        return sim_ph_level ? HIGH : LOW;
    }
    if (pin == POOL_LEVEL && simulating_pool_level) {
        return sim_pool_level ? HIGH : LOW;
    }
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
        ph_pump_start_time = now;
        Serial.printf("[SensorSim] pH pump STARTED at pH=%.4f\n", sim_ph_value);
    }
    else if (!active && ph_pump_active) {
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
        orp_pump_start_time = now;
        Serial.printf("[SensorSim] ORP pump STARTED at ORP=%.1f\n", sim_orp_value);
    }
    else if (!active && orp_pump_active) {
        double runtime = (now - orp_pump_start_time) / 1000.0;
        orp_pump_total_seconds += runtime;
        Serial.printf("[SensorSim] ORP pump STOPPED (ran %.1f sec, total=%.1f sec)\n", 
                      runtime, orp_pump_total_seconds);
    }
    
    orp_pump_active = active;
}

void SensorSimulation::setPIDOutputs(double phOutput, double orpOutput, double threshold) {
    setPhPumpActive(phOutput > threshold);
    setOrpPumpActive(orpOutput > threshold);
}

// ============================================================
// Feedback Calculation with Natural Drift
// ============================================================

void SensorSimulation::updateSimulationFeedback() {
    unsigned long now = millis();
    double dt = (now - last_feedback_update) / 1000.0;
    last_feedback_update = now;
    
    if (!simulating_ph && !simulating_orp) return;
    
    // Limit dt to prevent huge jumps
    if (dt > 5.0) dt = 1.0;
    
    // ============================================================
    // pH Simulation Feedback
    // 
    // Real pool behavior:
    // - Pump ON:  pH DECREASES (acid addition) - SIM_KPH is negative
    // - Pump OFF: pH INCREASES naturally (CO2 outgassing) - SIM_KPH_NATURAL
    //
    // This creates a natural oscillation around the setpoint.
    // ============================================================
    if (simulating_ph) {
        if (ph_pump_active) {
            // --------------------------------------------------------
            // PUMP RUNNING: pH decreases (adding acid)
            // --------------------------------------------------------
            double delta = SIM_KPH * dt;  // SIM_KPH is negative
            sim_ph_value += delta;
        } else {
            // --------------------------------------------------------
            // PUMP OFF: pH naturally INCREASES (CO2 outgassing)
            // This is the key: pH always drifts UP when pump is off,
            // regardless of current value. This simulates real pool chemistry.
            // --------------------------------------------------------
            double delta = SIM_KPH_NATURAL * dt;  // SIM_KPH_NATURAL is positive
            sim_ph_value += delta;
        }
        
        // Clamp to realistic limits
        if (sim_ph_value < SIM_PH_MIN) {
            sim_ph_value = SIM_PH_MIN;
        }
        if (sim_ph_value > SIM_PH_MAX) {
            sim_ph_value = SIM_PH_MAX;
        }
        
        // Debug output (every 30 seconds)
        static unsigned long last_ph_debug = 0;
        if (now - last_ph_debug > 30000) {
            last_ph_debug = now;
            Serial.printf("[SensorSim] pH=%.4f [%s]\n", 
                sim_ph_value, ph_pump_active ? "PUMP ON ↓" : "PUMP OFF ↑");
        }
    }
    
    // ============================================================
    // ORP Simulation Feedback
    //
    // Real pool behavior:
    // - Pump ON:  ORP INCREASES (chlorine addition) - SIM_KORP is positive
    // - Pump OFF: ORP DECREASES naturally (chlorine degradation) - SIM_KORP_NATURAL
    //
    // This creates a natural oscillation around the setpoint.
    // ============================================================
    if (simulating_orp) {
        if (orp_pump_active) {
            // --------------------------------------------------------
            // PUMP RUNNING: ORP increases (adding chlorine)
            // --------------------------------------------------------
            double delta = SIM_KORP * dt;  // SIM_KORP is positive
            sim_orp_value += delta;
        } else {
            // --------------------------------------------------------
            // PUMP OFF: ORP naturally DECREASES (chlorine degradation)
            // This is the key: ORP always drifts DOWN when pump is off,
            // regardless of current value. This simulates real pool chemistry.
            // --------------------------------------------------------
            double delta = SIM_KORP_NATURAL * dt;  // SIM_KORP_NATURAL is positive, applied as subtraction
            sim_orp_value -= delta;
        }
        
        // Clamp to realistic limits
        if (sim_orp_value < SIM_ORP_MIN) {
            sim_orp_value = SIM_ORP_MIN;
        }
        if (sim_orp_value > SIM_ORP_MAX) {
            sim_orp_value = SIM_ORP_MAX;
        }
        
        // Debug output (every 30 seconds)
        static unsigned long last_orp_debug = 0;
        if (now - last_orp_debug > 30000) {
            last_orp_debug = now;
            Serial.printf("[SensorSim] ORP=%.2f [%s]\n", 
                sim_orp_value, orp_pump_active ? "PUMP ON ↑" : "PUMP OFF ↓");
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
    Serial.printf("  pH: %.4f [%s] (pump runtime: %.1fs)\n",
        sim_ph_value, ph_pump_active ? "PUMP ON ↓" : "PUMP OFF ↑",
        ph_pump_total_seconds);
    Serial.printf("  ORP: %.2f [%s] (pump runtime: %.1fs)\n",
        sim_orp_value, orp_pump_active ? "PUMP ON ↑" : "PUMP OFF ↓",
        orp_pump_total_seconds);
}
