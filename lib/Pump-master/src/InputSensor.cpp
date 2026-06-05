#include "Arduino.h"
#include "InputSensor.h"

#ifdef KC868_A8
  #include "SensorSimulation.h"
#endif

bool InputSensor::getState() {
    return currentState;
}

//check status of input pins
void InputSensor::loop() {
//    bool currentState = IsActive();
//    if (currentState != lastState) {
//        if (xTimerStart(debounceTimer, 0) != pdPASS) {
//            Serial.println("Failed to start debounce timer");
//        }
//    }
    // Implement the loop functionality here
    // For input pins, you might want to read the state or perform some action
    // For example, you could log the state of the pin
    currentState = IsActive();
    if(currentState != previous_state) {
      previous_state = currentState;
    }
}


bool InputSensor::IsEnabled() {
    return getState();
}

#ifdef KC868_A8
// Override IsActive() to support sensor simulation on KC868-A8
// When simulation is active, use the simulated value instead of the physical pin.
bool InputSensor::IsActive() {
    int8_t simVal = SimSensor.getSimulatedInput(GetPinNumber());
    if (simVal >= 0) {
        return (simVal == HIGH);  // Simulated value: HIGH = active
    }
    return PIN::IsActive();
}
#endif

//Function overiden for derived class hierarchie but they do nothing
void InputSensor::SetOperationMode(bool _operation_mode) {}
bool InputSensor::GetOperationMode(void) {return false;}
void InputSensor::SetTankLevelPIN(uint8_t _tank_level) {}
void InputSensor::SetTankFill(double _tank_fill) {}
void InputSensor::SetTankVolume(double _tank_vol) {}
void InputSensor::SetFlowRate(double _flow_rate) {}
void InputSensor::SetMaxUpTime(unsigned long _max_uptime) {}
double InputSensor::GetTankFill() {return 100.;}
void InputSensor::ResetUpTime() {}
void InputSensor::SetInterlock(PIN* _interlock) {}
void InputSensor::SetInterlock(uint8_t) {}
uint8_t InputSensor::GetInterlockId(void) { return NO_INTERLOCK;}
bool InputSensor::IsRelay(void) {return true;}