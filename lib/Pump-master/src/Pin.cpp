#include "Arduino.h"
#include "Pin.h"

#ifdef KC868_A8
  #include "KC868A8_IO.h"
  #include "../../../include/SensorSimulation.h"
#endif

//Constructor
PIN::PIN(uint8_t _pin_number, uint8_t _pin_direction, bool _active_level)
{
  Initialize(_pin_number, _pin_direction,_active_level);
}

void PIN::Initialize(uint8_t _pin_number, uint8_t _pin_direction, bool _active_level)
{
  // Set variables
  if((_pin_number != pin_number) && (_pin_direction != INPUT_DIGITAL) && ((pin_direction==OUTPUT_DIGITAL)||(pin_direction==OUTPUT_PWM))) { // If we changed port
    if(_pin_number != 0) {
#ifdef KC868_A8
      // KC868: virtual pins (100+) — managed by I/O layer, skip raw GPIO
      if (pin_number < 100) {
#endif
      digitalWrite(pin_number,!active_level); // Inactivate the previous port
#ifdef KC868_A8
      }
#endif
      // IMPORTANT NOTE: Always change pin number in following order
      //  ->SetPinNumber
      //  ->SetActiveLevel
      //  ->Begin
    }
  }

  pin_number = _pin_number;
  pin_direction = _pin_direction;
  active_level = _active_level;
}

//Open the port
void PIN::Begin()
{
  //("Call Begin %d (%d)\r\n",pin_number,pin_direction);
  if(pin_number!=0) {
    // Open the port for OUTPUT and set it to down state
    if(pin_direction==OUTPUT_DIGITAL) {
#ifdef KC868_A8
      // KC868: virtual pins (100+) are managed by the I/O layer via setRelay()
      // Skip raw pinMode/digitalWrite — they hit unconnected GPIOs, not the I2C expander
      if (pin_number >= 100) {
        return;
      }
#endif
      pinMode(pin_number, OUTPUT);
      digitalWrite(pin_number,!active_level); // Initialize the port to inactive
    } else if(pin_direction==INPUT_DIGITAL) {
      pinMode(pin_number, INPUT);
    }
  }
}

//Switch the PIN ON
void PIN::Enable()
{
  if (pin_number != 0) {
#ifdef KC868_A8
    // KC868: virtual relay pins (100-107) — control via I2C expander
    if (pin_number >= 100 && pin_number <= 107) {
      KC868.setRelay(pin_number - 100, true);
      return;
    }
#endif
    digitalWrite(pin_number, active_level);
  }
}

//Switch the PIN OFF
void PIN::Disable()
{
  if (pin_number != 0) {
#ifdef KC868_A8
    // KC868: virtual relay pins (100-107) — control via I2C expander
    if (pin_number >= 100 && pin_number <= 107) {
      KC868.setRelay(pin_number - 100, false);
      return;
    }
#endif
    digitalWrite(pin_number, !active_level);
  }
}

//Toggle switch state
void PIN::Toggle()
{
  (IsActive()) ? Disable() : Enable();  // Invert the position of the relay
}

//Set pump relay to be active on HIGH or LOW state of the corresponding pin
void PIN::SetActiveLevel(bool _active_level)
{
  active_level = _active_level;
}

//Set pump relay to be active on HIGH or LOW state of the corresponding pin
bool PIN::GetActiveLevel(void)
{
  return active_level;
}

// Return pin id related to this relay
/*uint8_t PIN::GetPinId()
{
  return pin_id;
}*/

// Return pin number related to this relay
uint8_t PIN::GetPinNumber()
{
  return pin_number;
}

// Return pin direction related to this relay
uint8_t PIN::GetPinDirection()
{
  return pin_direction;
}

// Set the pin number for this relay
void PIN::SetPinNumber(uint8_t _pin_number, uint8_t _pin_direction, bool _active_level)
{
  // Open the port
  Initialize(_pin_number,_pin_direction,_active_level);
}

//relay status
bool PIN::IsActive()
{
  if (pin_number != 0) {
#ifdef KC868_A8
    // KC868: virtual relay pins (100-107) — read from KC868 I/O layer, not raw GPIO
    // Raw GPIO is unconnected to the I2C expander; KC868.digitalRead() returns 0
    // when not initialized, causing IsActive() to always return true for ACTIVE_LOW
    if (pin_number >= 100 && pin_number <= 107) {
      return KC868.getRelay(pin_number - 100);
    }
    // KC868: virtual input pins (110-117) — read via KC868 I/O layer
    if (pin_number >= 110 && pin_number <= 117) {
      return KC868.getInput(pin_number - 110);
    }
#endif
    return (digitalRead(pin_number) == active_level);
  } else {
    return false;
  }
}

void PIN::SavePreferences(Preferences& prefs, uint8_t pin_id) {
    char key[15]; 
    
    snprintf(key, sizeof(key), "d%d_pn", pin_id);  // "device_X_pin_number"
    prefs.putUChar(key, pin_number);

    snprintf(key, sizeof(key), "d%d_pd", pin_id);  // "device_X_pin_direction"
    prefs.putUChar(key, pin_direction);

    snprintf(key, sizeof(key), "d%d_id", pin_id);  // "device_X_pin_id"
    prefs.putUChar(key, pin_id);

    snprintf(key, sizeof(key), "d%d_al", pin_id);  // "device_X_active_level"
    prefs.putBool(key, active_level);
}

void PIN::LoadPreferences(Preferences& prefs, uint8_t pin_id) {
  char key[15];

  // pin_number
  snprintf(key, sizeof(key), "d%d_pn", pin_id);
  pin_number = prefs.getUChar(key, pin_number);

  // pin_direction
  snprintf(key, sizeof(key), "d%d_pd", pin_id);
  pin_direction = prefs.getUChar(key, pin_direction);

  // pin_active_level
  snprintf(key, sizeof(key), "d%d_al", pin_id);
  active_level = prefs.getBool(key, active_level);
}