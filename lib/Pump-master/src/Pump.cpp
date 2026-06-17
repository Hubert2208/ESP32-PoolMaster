#include "Arduino.h"
#include "Pump.h"

#ifdef KC868_A8
  #include "../../../include/SensorSimulation.h"
#endif

//Call this in the main loop, for every loop, as often as possible
void Pump::loop()
{
  u_int8_t bitMaskErrors = 0;

  // DEBUG: log pump state for all pumps every ~30s
  {
      static unsigned long lastStatePin[32] = {0};
      uint8_t pn = GetPinNumber();
      unsigned long nowMs = millis();
      if (pn >= 100 && pn <= 131 && (lastStatePin[pn-100] == 0 || nowMs - lastStatePin[pn-100] >= 30000)) {
          lastStatePin[pn-100] = nowMs;
          Serial.printf("[PumpStates] pin=%d enables=%s upTime=%lu\r\n",
              pn, IsRunning() ? "ON" : "OFF", UpTime);
      }
  }

  // Call the loop handler if it exists, if the pump is not running
  // and only if the interlock pump is running (if it exists)
  if (interlock_pump_ == nullptr || (interlock_pump_ != nullptr && interlock_pump_->IsEnabled())) {
    if (shouldStartHandler && shouldStartHandler() && !IsRunning()) {
        // If started successfully, call the onStart handler
        bitMaskErrors = Start(); // Start the pump and reset UpTime
        if(bitMaskErrors == 0) { // No error at startup
            if (onStartHandler) onStartHandler(); // Appel du handler
        }
    }
  }

  if (shouldStopHandler && shouldStopHandler() && IsRunning()) {
      if (Stop() == true) {
          // If stopped successfully, call the onStop handler
          if (onStopHandler) onStopHandler(); // Appel du handler
      }
  }

  if(IsRunning())
  {
    if (loopHandler) loopHandler(); // Appel du handler à chaque boucle

    UpTime += millis() - LastLoopMillis;
    LastLoopMillis = millis();

    // Debug: log IsRunning state every ~5s per pump instance
    {
        static unsigned long lastLogByPin[32] = {0};
        uint8_t pn = GetPinNumber();
        unsigned long now = millis();
        // Use pin number as index into a fixed array (safe: pin 100-131)
        uint8_t idx = pn - 100;
        if (pn >= 100 && pn <= 131 && (lastLogByPin[idx] == 0 || now - lastLogByPin[idx] >= 5000)) {
            lastLogByPin[idx] = now;
            Serial.printf("[Pump::loop] pin=%d IsRunning=true activeLvl=%d digRead=%d UpTime=%lu\r\n",
                pn, GetActiveLevel(), digitalRead(pn), UpTime);
        }
    }

    if((CurrMaxUpTime > 0) && (UpTime >= CurrMaxUpTime))
    {
        Stop();
        UpTimeError = true;
    }

    if(!TankLevel())
    {
      Stop();
    } 

    // If there is an interlock pump and it stopped. Stop this pump as well
    if ((interlock_pump_!=nullptr) && (interlock_pump_->IsEnabled() == false))
    {
      Stop();
    }
  }
}

//Switch pump ON
u_int8_t Pump::Start(bool _resetUpTime)
{
  u_int8_t bitMaskErrors = 0;

  if (_resetUpTime) {
    ResetUpTime(); // Reset UpTime if requested
  }

  // Check why a Pump would not start
  bitMaskErrors |= (UpTimeError & 1) << 0; // Bit 0: UpTime error
  bitMaskErrors |= (!TankLevel() & 1) << 1; // Bit 1: Tank level error
  bitMaskErrors |= (!CheckInterlock() & 1) << 2; // Bit 2: Interlock error

  if((!IsRunning()) && !UpTimeError && TankLevel() && CheckInterlock())
  {
    if (!this->Relay::Enable()) {
      bitMaskErrors |= (1 << 3); // Bit 3: Relay error
#ifdef KC868_A8
      Serial.printf("[Pump::Start] pin=%d Relay Enable FAILED! bits=0x%02X\r\n",
        GetPinNumber(), bitMaskErrors);
#endif
      return bitMaskErrors;
    } else {
      LastLoopMillis = StartTime = millis(); 
#ifdef KC868_A8
      Serial.printf("[Pump::Start] pin=%d Pump STARTED OK\r\n", GetPinNumber());
#endif
    }
  }
  return bitMaskErrors;
}

//Switch pump OFF
bool Pump::Stop()
{
  if(IsRunning())
  {
    if (!this->Relay::Disable())
    {
      return false;
    }
    
    UpTime += millis() - LastLoopMillis; 

    
    return true;
  } else return false;
}

//Pump status
bool Pump::IsRunning()
{
  return (this->Relay::IsEnabled());
}

//tank level status (true = full, false = empty)
bool Pump::TankLevel()
{
  if(tank_level_pin == NO_TANK)
  {
    return true;
  }
  else if (tank_level_pin == NO_LEVEL)
  {
    return (GetTankFill() > 5.); //alert below 5% 
  }
  else
  {
#ifdef KC868_A8
    // Check simulation first: if simulation is active for this sensor,
    // use the simulated value instead of reading the physical pin.
    // This prevents false "tank full" readings when no sensor is connected.
    int8_t simVal = SimSensor.getSimulatedInput(tank_level_pin);
    if (simVal >= 0) {
        return (simVal == TANK_FULL);
    }
#endif
    return (digitalRead(tank_level_pin) == TANK_FULL);
  } 
}

//Set tank fill (percentage of tank volume)
void Pump::SetTankFill(double _tankfill)
{
  tankfill = _tankfill;
}

//Return the remaining quantity in tank in %. When resetting UpTime, SetTankFill must be called accordingly
double Pump::GetTankFill()
{
  return (tankfill - GetTankUsage());
}

//Set Tank volume
//Typically call this function when changing tank and set it to the full volume
void Pump::SetTankVolume(double _tankvolume)
{
  tankvolume = _tankvolume;
}

//Return the percentage used since last reset of UpTime
double Pump::GetTankUsage() 
{
  float PercentageUsed = -1.0;
  if((tankvolume != 0.0) && (flowrate !=0.0))
  {
    double MinutesOfUpTime = (double)UpTime/1000.0/60.0;
    double Consumption = flowrate/60.0*MinutesOfUpTime;
    PercentageUsed = Consumption/tankvolume*100.0;
  }
  return (PercentageUsed);
}

//Set flow rate of the pump in Liters/hour
void Pump::SetFlowRate(double _flowrate)
{
  flowrate = _flowrate;
}

//Set a maximum running time (in millisecs) per day (in case ResetUpTime() is called once per day)
//Once reached, pump is stopped and "UpTimeError" error flag is raised
//Set "Max" to 0 to disable limit
void Pump::SetMaxUpTime(unsigned long _maxuptime)
{
  MaxUpTime = _maxuptime;
  CurrMaxUpTime = _maxuptime;
}

//Set a minimum running time (in millisecs) 
//Pump can't stop before this time is reached
//Set "Min" to 0 to disable limit
void Pump::SetMinUpTime(unsigned long _minuptime)
{
  MinUpTime = _minuptime;
}


//Reset the tracking of running time
//This is typically called every day at midnight
void Pump::ResetUpTime()
{
  StartTime = 0;
  StopTime = 0;
  UpTime = 0;
  CurrMaxUpTime = MaxUpTime;
  LastLoopMillis = millis();  // FIX: preserve millis() reference to avoid jump in UpTime calculation
}

//Clear "UpTimeError" error flag and allow the pump to run for an extra MaxUpTime
void Pump::ClearErrors()
{
  if(UpTimeError)
  {
    CurrMaxUpTime += MaxUpTime;
    UpTimeError = false;
  }
}

// Set Tank Level PIN
void Pump::SetTankLevelPIN(uint8_t _tank_level_pin)
{
  tank_level_pin = _tank_level_pin;
}

// Initialize the Interlock if needed
void Pump::SetInterlock(PIN* _interlock_pump_)
{
  interlock_pump_ = _interlock_pump_;
}

void Pump::SetInterlock(uint8_t _interlock_pin_id)
{
  interlock_pin_id = _interlock_pin_id;
}

uint8_t Pump::GetInterlockId(void) 
{
/*  if(interlock_pump_ != nullptr)
  {
    return interlock_pump_->GetPinId();
  } else {
    return NO_INTERLOCK;
  }*/
  return interlock_pin_id;
}

//Interlock status
bool Pump::CheckInterlock()
{
  if (interlock_pump_ == nullptr) {
    return true;
  } else {
    return  ((Relay*)interlock_pump_->IsEnabled());
  }
}

bool Pump::IsRelay(void)
{
  return false;
}

void Pump::SavePreferences(Preferences& prefs, uint8_t pin_id)  {
    Relay::SavePreferences(prefs,pin_id);

    char key[15];
    //uint8_t tmp_pin_id = GetPinId();

    snprintf(key, sizeof(key), "d%d_fr", pin_id);  // "device_X_flowrate"
    prefs.putDouble(key, flowrate);
  
    snprintf(key, sizeof(key), "d%d_tv", pin_id);  // "device_X_tankvolume"
    prefs.putDouble(key, tankvolume);
  
    snprintf(key, sizeof(key), "d%d_tf", pin_id);  // "device_X_tankfill"
    prefs.putDouble(key, tankfill);
  
    snprintf(key, sizeof(key), "d%d_tl", pin_id);  // "device_X_tanklevelpin"
    prefs.putUChar(key, tank_level_pin);
  
    snprintf(key, sizeof(key), "d%d_il", pin_id);  // "device_X_interlockid"
    prefs.putUChar(key, interlock_pin_id);
  
    snprintf(key, sizeof(key), "d%d_mu", pin_id);  // "device_X_maxutime"
    prefs.putULong(key, MaxUpTime);
  
    snprintf(key, sizeof(key), "d%d_mi", pin_id);  // "device_X_minutime"
    prefs.putULong(key, MinUpTime);
  }

void Pump::LoadPreferences(Preferences& prefs, uint8_t pin_id) {
    Relay::LoadPreferences(prefs,pin_id);

    char key[15];

    snprintf(key, sizeof(key), "d%d_fr", pin_id);
    flowrate = prefs.getDouble(key, flowrate);

    snprintf(key, sizeof(key), "d%d_tv", pin_id );
    tankvolume = prefs.getDouble(key, tankvolume);

    snprintf(key, sizeof(key), "d%d_tf", pin_id);
    tankfill = prefs.getDouble(key, tankfill);

    snprintf(key, sizeof(key), "d%d_tl", pin_id);
    tank_level_pin = prefs.getUChar(key, tank_level_pin);

    snprintf(key, sizeof(key), "d%d_il", pin_id);
    interlock_pin_id = prefs.getUChar(key, interlock_pin_id);

    snprintf(key, sizeof(key), "d%d_mu", pin_id);
    MaxUpTime = prefs.getULong(key, MaxUpTime);

    snprintf(key, sizeof(key), "d%d_mi", pin_id);
    MinUpTime = prefs.getULong(key, MinUpTime);
  }
