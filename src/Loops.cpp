#include <Arduino.h>                // Arduino framework
#include "Config.h"
#include "PoolMaster.h"

// Helper to format uptime in human-readable format
static void printUptime(unsigned long ms, const char* prefix) {
  unsigned long sec = ms / 1000;
  unsigned long min = sec / 60;
  unsigned long hrs = min / 60;
  sec %= 60;
  min %= 60;
  Debug.print(DBG_INFO, "[LOGIC] %s Board uptime: %luh %02lum %02lus", prefix, hrs, min, sec);
}

#ifdef KC868_A8
  #include "SensorSimulation.h"
#endif

// Setup oneWire instances to communicate with temperature sensors (one bus per sensor)
static OneWire oneWire_W(ONE_WIRE_BUS_W);
static OneWire oneWire_A(ONE_WIRE_BUS_A);
// Pass our OneWire reference to Dallas Temperature library instance
static DallasTemperature sensors_W(&oneWire_W);
static DallasTemperature sensors_A(&oneWire_A);
// MAC Addresses of DS18b20 water & Air temperature sensor
static DeviceAddress DS18B20_W = { 0x28, 0x9F, 0x24, 0x24, 0x0C, 0x00, 0x00, 0xA9 };
static DeviceAddress DS18B20_A = { 0x28, 0xB0, 0x70, 0x75, 0xD0, 0x01, 0x3C, 0x9D };

// Setup an ADS1115 instance for analog measurements
static ADS1115Scanner adc_int(ADS1115ADDRESS);  // Address 0x48 is the default
#ifdef EXT_ADS1115
static ADS1115Scanner adc_ext(EXT_ADS1115_ADDR);
#endif

static float ph_sensor_value;     // pH sensor current value
static float orp_sensor_value;    // ORP sensor current value
static float psi_sensor_value;    // PSI sensor current value

// Signal filtering library sample buffers
static RunningMedian samples_WTemp = RunningMedian(11);
static RunningMedian samples_ATemp = RunningMedian(11);
static RunningMedian samples_Ph    = RunningMedian(11);
static RunningMedian samples_Orp   = RunningMedian(11);
static RunningMedian samples_PSI   = RunningMedian(11);

void stack_mon(UBaseType_t&);
void lockI2C();
void unlockI2C();

// Helper: log pump start errors as decoded bitmask
static void logPumpStartErrors(const char* pumpName, uint8_t errors) {
    if (errors == 0) return; // no errors, pump started successfully
    Debug.print(DBG_WARNING, "[%s] Start() errors: 0x%02X | UpTime:%d Tank:%d Interlock:%d Relay:%d",
        pumpName, errors,
        (errors >> 0) & 1,  // Bit 0: UpTimeError
        (errors >> 1) & 1,  // Bit 1: TankLevel error
        (errors >> 2) & 1,  // Bit 2: Interlock error
        (errors >> 3) & 1); // Bit 3: Relay error
}

#ifdef EXT_ADS1115
//----------------------------
void AnalogInit()
{
  adc_int.setSpeed(ADS1115_SPEED_16SPS);
  adc_int.addChannel(ADS1115_CHANNEL2, ADS1115_RANGE_6144);
  adc_int.setSamples(8);

  adc_ext.setSpeed(ADS1115_SPEED_16SPS);
  adc_ext.addChannel(ADS1115_CHANNEL01, ADS1115_RANGE_6144);
  adc_ext.addChannel(ADS1115_CHANNEL23, ADS1115_RANGE_6144);
  adc_ext.setSamples(4);
}

void AnalogPoll(void *pvParameters)
{
  while (!startTasks) ;

  TickType_t period = PT1;  
  TickType_t ticktime = xTaskGetTickCount(); 
  static UBaseType_t hwm=0;

  lockI2C();
  adc_int.start();
  adc_ext.start();
  unlockI2C();
  vTaskDelayUntil(&ticktime,period);
  
  for(;;)
  {
    lockI2C();
    adc_ext.update();

    if(adc_ext.ready()){
      orp_sensor_value = adc_ext.readFilter(0);
      if(orp_sensor_value >= 32768) orp_sensor_value = orp_sensor_value - 65536;
      ph_sensor_value = adc_ext.readFilter(1);
      if(ph_sensor_value >= 32768) ph_sensor_value= ph_sensor_value - 65536;
      adc_ext.start();  
        
      samples_Ph.add(ph_sensor_value);
      PMData.PhValue = (samples_Ph.getAverage(5)*0.1875/1000.)*PMConfig.get<double>(PHCALIBCOEFFS0) + PMConfig.get<double>(PHCALIBCOEFFS1);

      samples_Orp.add(orp_sensor_value);
      PMData.OrpValue = (samples_Orp.getAverage(5)*0.1875/1000.)*PMConfig.get<double>(ORPCALIBCOEFFS0) + PMConfig.get<double>(ORPCALIBCOEFFS1);

#ifdef KC868_A8
      double simPH = SimSensor.getSimulatedValue(SimSensor.SENSOR_PH);
      if (!isnan(simPH)) PMData.PhValue = simPH;
      double simORP = SimSensor.getSimulatedValue(SimSensor.SENSOR_ORP);
      if (!isnan(simORP)) PMData.OrpValue = simORP;
#endif

      Debug.print(DBG_DEBUG,"pH: %5.0f - %4.2f - ORP: %5.0f - %3.0fmV - PSI: %5.0f - %4.2fBar\r\n",
        ph_sensor_value,PMData.PhValue,orp_sensor_value,PMData.OrpValue,psi_sensor_value,PMData.PSIValue);
    }
    
    adc_int.update();

    if(adc_int.ready()){
      psi_sensor_value = adc_int.readFilter(0) ;
      adc_int.start();

      samples_PSI.add(psi_sensor_value);
      PMData.PSIValue = (samples_PSI.getAverage(5)*0.1875/1000.)*PMConfig.get<double>(PSICALIBCOEFFS0) + PMConfig.get<double>(PSICALIBCOEFFS1);
      PMData.PSIValue = (PMData.PSIValue < 0)? 0 : PMData.PSIValue;

#ifdef KC868_A8
      double simPSI = SimSensor.getSimulatedValue(SimSensor.SENSOR_PSI);
      if (!isnan(simPSI)) PMData.PSIValue = simPSI;
#endif
    }
    unlockI2C();

    stack_mon(hwm);
    vTaskDelayUntil(&ticktime,period);
  }  
}

#else //EXT_ADS1115
//-----------------

void AnalogInit()
{
  adc_int.setSpeed(ADS1115_SPEED_16SPS);
  adc_int.addChannel(ADS1115_CHANNEL0, ADS1115_RANGE_6144);
  adc_int.addChannel(ADS1115_CHANNEL1, ADS1115_RANGE_6144);
  adc_int.addChannel(ADS1115_CHANNEL2, ADS1115_RANGE_6144);
  adc_int.setSamples(3);
}

void AnalogPoll(void *pvParameters)
{
  while (!startTasks) ;

  TickType_t period = PT1;  
  TickType_t ticktime = xTaskGetTickCount(); 
  static UBaseType_t hwm=0;

  #ifdef CHRONO
  unsigned long td;
  int t_act=0,t_min=999,t_max=0;
  float t_mean=0.;
  int n=1;
  #endif

  lockI2C();
  adc_int.start();
  unlockI2C();
  vTaskDelayUntil(&ticktime,period);
  
  for(;;)
  {
    #ifdef CHRONO
    td = millis();
    #endif

    lockI2C();
    adc_int.update();

    if(adc_int.ready()){
        orp_sensor_value = adc_int.readFilter(0) ;
        ph_sensor_value  = adc_int.readFilter(1) ;
        psi_sensor_value = adc_int.readFilter(2) ;
        adc_int.start();  
        
        samples_Ph.add(ph_sensor_value);
        PMData.PhValue = (samples_Ph.getAverage(5)*0.1875/1000.)*PMConfig.get<double>(PHCALIBCOEFFS0) + PMConfig.get<double>(PHCALIBCOEFFS1);

        samples_Orp.add(orp_sensor_value);
        PMData.OrpValue = (samples_Orp.getAverage(5)*0.1875/1000.)*PMConfig.get<double>(ORPCALIBCOEFFS0) + PMConfig.get<double>(ORPCALIBCOEFFS1);

        samples_PSI.add(psi_sensor_value);
        PMData.PSIValue = (samples_PSI.getAverage(5)*0.1875/1000.)*PMConfig.get<double>(PSICALIBCOEFFS0) + PMConfig.get<double>(PSICALIBCOEFFS1);
        PMData.PSIValue = (PMData.PSIValue < 0)? 0 : PMData.PSIValue;

#ifdef KC868_A8
        double simPH = SimSensor.getSimulatedValue(SimSensor.SENSOR_PH);
        if (!isnan(simPH)) PMData.PhValue = simPH;
        double simORP = SimSensor.getSimulatedValue(SimSensor.SENSOR_ORP);
        if (!isnan(simORP)) PMData.OrpValue = simORP;
        double simPSI = SimSensor.getSimulatedValue(SimSensor.SENSOR_PSI);
        if (!isnan(simPSI)) PMData.PSIValue = simPSI;
#endif

        Debug.print(DBG_DEBUG,"pH: %5.0f - %4.2f - ORP: %5.0f - %3.0fmV - PSI: %5.0f - %4.2fBar\r\n",
            ph_sensor_value,PMData.PhValue,orp_sensor_value,PMData.OrpValue,psi_sensor_value,PMData.PSIValue);
    }
    unlockI2C();

    stack_mon(hwm);
    vTaskDelayUntil(&ticktime,period);
  }  
}

#endif //EXT_ADS1115
//------------------

void StatusLights(void *pvParameters)
{
  static uint8_t line = 0;
  uint8_t status;

  while (!startTasks) ;
  vTaskDelay(DT7);                                // Scheduling offset 

  TickType_t period = PT7;  
  TickType_t ticktime = xTaskGetTickCount(); 
  static UBaseType_t hwm = 0;

  for(;;)
  {
    status = 0;
    status |= (line & 1) << 1;
    if(line == 0)
    {
        line = 1;
        status |= (PMConfig.get<bool>(AUTOMODE) & 1) << 2;
        status |= (AntiFreezeFiltering & 1) << 3;        
        status |= (FillingPump.UpTimeError  & 1) << 6;
        status |= (PSIError & 1) << 7;
    } else
    {
        line = 0;
        status |= (PhPID.GetMode() & 1) << 2;
        status |= (OrpPID.GetMode() & 1) << 3;
        status |= (!PhPump.TankLevel() & 1) << 4;
        status |= (!ChlPump.TankLevel() & 1) << 5;
        status |= (PhPump.UpTimeError & 1) << 6;
        status |= (ChlPump.UpTimeError & 1) << 7;  
    }
#if BUZZER != 255
    if(PMConfig.get<bool>(BUZZERON))
    {
      (status & 0xF0) ? digitalWrite(BUZZER,HIGH) : digitalWrite(BUZZER,LOW) ;
    }else{
      digitalWrite(BUZZER,LOW);
    }
#endif
    if(WiFi.status() == WL_CONNECTED) status |= 0x01;
        else status &= 0xFE;
    Debug.print(DBG_VERBOSE,"Status LED : 0x%02x",status);
    lockI2C();
    Wire.beginTransmission(PCF8574ADDRESS);
    Wire.write(~status);
    Wire.endTransmission();
    unlockI2C();

    stack_mon(hwm);
    vTaskDelayUntil(&ticktime,period);
  }
}

void pHRegulation(void *pvParameters)
{
  while (!startTasks) ;
  vTaskDelay(DT6);                                // Scheduling offset 

  TickType_t period = PT6;  
  TickType_t ticktime = xTaskGetTickCount(); 
  static UBaseType_t hwm = 0;

  for(;;)
  {
    //do not compute PID if filtration pump is not running
    if (PhPID.GetMode() == AUTOMATIC)
    {  
      if (FiltrationPump.IsRunning()) {
 
          if(PhPID.Compute()){
            Debug.print(DBG_INFO,"Ph  regulation: %10.2f, %13.9f, %13.9f, %17.9f",PMData.PhPIDOutput,PMData.PhValue,PMData.Ph_SetPoint,PMConfig.get<double>(PH_KP));
            if(PMData.PhPIDOutput < (double)30000.) PMData.PhPIDOutput = 0.;
          }    

          /************************************************
           turn the Acid pump on/off based on pid output
          ************************************************/
          unsigned long now = millis();
          unsigned long wSize = PMConfig.get<unsigned long>(PHPIDWINDOWSIZE);
          if (now - PMData.PhPIDwStart > wSize)
          {
            //time to shift the Relay Window
            PMData.PhPIDwStart += wSize;
          }
          if ((unsigned long)PMData.PhPIDOutput <= now - PMData.PhPIDwStart) {
            PhPump.Stop();
          } else {
            uint8_t err = PhPump.Start();
            logPumpStartErrors("PhPump", err);
          }
      } else {
        PhPID.SetMode(MANUAL);
        PMData.Ph_RegOnOff = false;
        PMData.PhPIDOutput = 0.0;
        PhPump.Stop();
      } 
      // Debug: log pH pump start/stop with runtime and board uptime
      {
        static bool phPumpWasRunning = false;
        bool phPumpRunning = PhPump.IsRunning();
        if (phPumpRunning && !phPumpWasRunning) {
          Debug.print(DBG_INFO, "[LOGIC] pH Pump START");
          printUptime(millis(), "[PhPump START]");
        } else if (!phPumpRunning && phPumpWasRunning) {
          unsigned long runtime_sec = PhPump.GetUpTime();
          Debug.print(DBG_INFO, "[LOGIC] pH Pump STOP -- ran for %lum %02lus", runtime_sec / 60, runtime_sec % 60);
          printUptime(millis(), "[PhPump STOP]");
        }
        phPumpWasRunning = phPumpRunning;
      }
    }

    stack_mon(hwm);
    vTaskDelayUntil(&ticktime,period);
  }
}

//Orp regulation loop
void OrpRegulation(void *pvParameters)
{
  while (!startTasks) ;
  vTaskDelay(DT5);                                // Scheduling offset 

  TickType_t period = PT5;  
  TickType_t ticktime = xTaskGetTickCount(); 
  static UBaseType_t hwm = 0;

  for(;;)
  { 
    //do not compute PID if filtration pump is not running
    if (OrpPID.GetMode() == AUTOMATIC) {
      if (FiltrationPump.IsRunning())
      {
        if(OrpPID.Compute()){
          Debug.print(DBG_INFO,"ORP regulation: %10.2f, %13.9f, %12.9f, %17.9f",PMData.OrpPIDOutput,PMData.OrpValue,PMData.Orp_SetPoint,PMConfig.get<double>(ORP_KP));
          if(PMData.OrpPIDOutput < (double)30000.) PMData.OrpPIDOutput = 0.;    
          }    

        /************************************************
         turn the Chl pump on/off based on pid output
        ************************************************/
        unsigned long now = millis();
        unsigned long wSize = PMConfig.get<unsigned long>(ORPPIDWINDOWSIZE);
        if (now - PMData.OrpPIDwStart > wSize)
        {
          //time to shift the Relay Window
          PMData.OrpPIDwStart += wSize;
        }
        if ((unsigned long)PMData.OrpPIDOutput <= now - PMData.OrpPIDwStart) {
          ChlPump.Stop();
        } else {
          uint8_t err = ChlPump.Start();
          logPumpStartErrors("ChlPump", err);
        }
      } else {
        OrpPID.SetMode(MANUAL);
        PMData.Orp_RegOnOff = false;
        PMData.OrpPIDOutput = 0.0;
        ChlPump.Stop();
      } 
      // Debug: log Chlor pump start/stop with runtime and board uptime
      {
        static bool chlPumpWasRunning = false;
        bool chlPumpRunning = ChlPump.IsRunning();
        if (chlPumpRunning && !chlPumpWasRunning) {
          Debug.print(DBG_INFO, "[LOGIC] Chlor Pump START");
          printUptime(millis(), "[ChlPump START]");
        } else if (!chlPumpRunning && chlPumpWasRunning) {
          unsigned long runtime_sec = ChlPump.GetUpTime();
          Debug.print(DBG_INFO, "[LOGIC] Chlor Pump STOP -- ran for %lum %02lus", runtime_sec / 60, runtime_sec % 60);
          printUptime(millis(), "[ChlPump STOP]");
        }
        chlPumpWasRunning = chlPumpRunning;
      }
    }

    stack_mon(hwm);    
    vTaskDelayUntil(&ticktime,period);
  }
}

//Init DS18B20 one-wire library
void TempInit()
{
  bool error = false;
  char buf[64];
  sensors_W.begin();
  sensors_W.begin();
  sensors_A.begin();

  Debug.print(DBG_INFO,"1wire W devices: %d device(s) found",sensors_W.getDeviceCount());
  Debug.print(DBG_INFO,"1wire A devices: %d device(s) found",sensors_A.getDeviceCount());

  if (!sensors_W.getAddress(DS18B20_W, 0)) 
  {
    Debug.print(DBG_ERROR,"Unable to find address for bus W");
    error = true;
  }  
  else {
    sprintf(buf,"DS18B20_W: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
      DS18B20_W[0],DS18B20_W[1],DS18B20_W[2],DS18B20_W[3],
      DS18B20_W[4],DS18B20_W[5],DS18B20_W[6],DS18B20_W[7]);
    Debug.print(DBG_INFO,"%s",buf);
    Serial.printf("DS18B20_W: ");
    for(uint8_t i=0;i<8;i++){
      Serial.printf("%02x",DS18B20_W[i]);
      if(i<7) Serial.print(":");
        else Serial.printf("\r\n");
    }
  }  
  if (!sensors_A.getAddress(DS18B20_A, 0)) 
  {
    Debug.print(DBG_ERROR,"Unable to find address for bus A");
    error = true;
  }  
  else {
    sprintf(buf,"DS18B20_A: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
      DS18B20_A[0],DS18B20_A[1],DS18B20_A[2],DS18B20_A[3],
      DS18B20_A[4],DS18B20_A[5],DS18B20_A[6],DS18B20_A[7]);
    Debug.print(DBG_INFO,"%s",buf);
    Serial.printf("DS18B20_A: ");
    for(uint8_t i=0;i<8;i++){
      Serial.printf("%02x",DS18B20_A[i]);
      if(i<7) Serial.print(":");
        else Serial.printf("\r\n");
    }
  } 

  if(!error) 
  {
    sensors_W.setResolution(DS18B20_W, TEMPERATURE_RESOLUTION);
    sensors_A.setResolution(DS18B20_A, TEMPERATURE_RESOLUTION);
    sensors_W.setWaitForConversion(false);
    sensors_A.setWaitForConversion(false);
  }
}

//Request temperature asynchronously
void getTemp(void *pvParameters)
{
  while (!startTasks) ;
  vTaskDelay(DT4);                                // Scheduling offset 

  TickType_t period = PT4;  
  TickType_t ticktime = xTaskGetTickCount(); 
  static UBaseType_t hwm = 0;

  sensors_W.requestTemperatures();
  sensors_A.requestTemperatures();
  vTaskDelayUntil(&ticktime,period);
  
  for(;;)
  {        
    double temp = sensors_W.getTempC(DS18B20_W);
    if (temp == NAN || temp == -127) {
      Debug.print(DBG_WARNING,"Error getting Water temperature");
    }  else PMData.WaterTemp = temp;
    samples_WTemp.add(PMData.WaterTemp);
    PMData.WaterTemp = samples_WTemp.getAverage(5);
    Debug.print(DBG_VERBOSE,"DS18B20_W: %6.2f C",PMData.WaterTemp);

    temp = sensors_A.getTempC(DS18B20_A);
    if (temp == NAN || temp == -127) {
      Debug.print(DBG_WARNING,"Error getting Air temperature");
    }  else PMData.AirTemp = temp;
    samples_ATemp.add(PMData.AirTemp);
    PMData.AirTemp = samples_ATemp.getAverage(5);
    Debug.print(DBG_VERBOSE,"DS18B20_A: %6.2f C",PMData.AirTemp);

    sensors_W.requestTemperatures();
    sensors_A.requestTemperatures();

    stack_mon(hwm);
    vTaskDelayUntil(&ticktime,period);
  } 
}

// ============================================================
// Analog Sensor Simulation Loop (KC868-A8 only)
// ==========================================================
void AnalogSimLoop(void *pvParameters)
{
  while (!startTasks) ;
  vTaskDelay(DT7);
  
  TickType_t period = pdMS_TO_TICKS(60000);  // 60 seconds
  TickType_t ticktime = xTaskGetTickCount(); 
  static UBaseType_t hwm = 0;

  Debug.print(DBG_INFO, "[AnalogSim] Loop started (period: 60s)");

  for(;;)
  {
    SimSensor.updateAnalogSimulation();
    stack_mon(hwm);
    vTaskDelayUntil(&ticktime,period);
  }
}
