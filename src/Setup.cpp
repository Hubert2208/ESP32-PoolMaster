#undef __STRICT_ANSI__              // work-around for Time Zone definition
#include <stdint.h>                 // std lib (types definitions)
#include <Arduino.h>                // Arduino framework
#include <esp_sntp.h>

#include "Config.h"
#include "PoolMaster.h"

#ifdef SIMU
bool init_simu = true;
double pHLastValue = 7.;
unsigned long pHLastTime = 0;
double OrpLastValue = 730.;
unsigned long OrpLastTime = 0;
double pHTab [3] {0.,0.,0.};
double ChlTab [3] {0.,0.,0.};
uint8_t iw = 0;
uint8_t jw = 0;
bool newpHOutput = false;
bool newChlOutput = false;
double pHCumul = 0.;
double ChlCumul = 0.;
#endif

// Firmware revision
String Firmw = FIRMW;

//Settings structure and its default values
// si pH+ : Kp=2250000.
// si pH- : Kp=2700000.
#ifdef EXT_ADS1115
StoreStruct storage =
{ 
  CONFIG_VERSION,
  0, 0, 1, 0,
  8, 11, 19, 8, 22, 20,
  2700, 2700, 30000,
  1800000, 1800000, 0, 0,
  7.3, 720.0, 1.8, 0.3, 16.0, 27.0, 3.0, -2.49, 6.87, 431.03, 0, 0.377923399, -0.17634473,
  2700000.0, 0.0, 0.0, 18000.0, 0.0, 0.0, 0.0, 0.0, 28.0, 7.3, 720., 1.3,
  100.0, 100.0, 20.0, 20.0, 1.5, 1.5,
  15, 2,  //ajout
  0, 1, 0
};
#else
StoreStruct storage =
{ 
  CONFIG_VERSION,
  0, 0, 1, 0,
  13, 8, 21, 8, 22, 20,
  2700, 2700, 30000,
  1800000, 1800000, 0, 0,
  7.3, 720.0, 1.8, 0.7, 10.0, 18.0, 3.0, 3.61078313, -3.88020422, -966.946396, 2526.88809, 1.31, -0.1,
  2700000.0, 0.0, 0.0, 18000.0, 0.0, 0.0, 0.0, 0.0, 28.0, 7.3, 720., 1.3,
  25.0, 60.0, 20.0, 20.0, 1.5, 1.5,
  15, 2,  //ajout
  0, 1, 0
};
#endif
tm timeinfo;

// Various global flags
volatile bool startTasks = false;               // Signal to start loop tasks

bool AntiFreezeFiltering = false;               // Filtration anti freeze mode
//bool EmergencyStopFiltPump = false;             // flag will be (re)set by double-tapp button
bool PSIError = false;                          // Water pressure OK
bool cleaning_done = false;                     // daily cleaning done   

// Queue object to store incoming JSON commands (up to 10)
QueueHandle_t queueIn;

// NVS Non Volatile SRAM (eqv. EEPROM)
Preferences nvs;      

// Instanciations of Pump and PID objects to make them global. But the constructors are then called 
// before loading of the storage struct. At run time, the attributes take the default
// values of the storage struct as they are compiled, just a few lines above, and not those which will 
// be read from NVS later. This means that the correct objects attributes must be set later in
// the setup function (fortunatelly, init methods exist).

// The five pumps of the system (instanciate the Pump class)
// In this case, all pumps start/Stop are managed by relays. pH, ORP, Robot and Electrolyse SWG pumps are interlocked with 
// filtration pump
// Pump class takes the following parameters:
//    1/ The MCU relay output pin number to be switched by this pump
//    2/ The MCU relay input pin number to monitor for the actual state of the pump (can be the same as first argument if directly controlled)
//       This option is especially useful in the case where the filtration pump is not managed by the Arduino. 
//    3/ The MCU digital input pin number connected to the tank low level switch (can be NO_TANK or NO_LEVEL)
//    4/ The MCU digital input pin number to monitor for interlock. If this input is Inactive, pump is stopped and/or cannot start. This is used for instance to stop
//       the Orp, pH and Electrolyse pumps in case filtration pump is not running
//       Interlock argument can also be a reference to a Relay class which manages the interlocked object.
//    5/ The relay level used for the pump. RELAY_ACTIVE_HIGH if relay ON when digital output is HIGH and RELAY_ACTIVE_LOW if relay ON when digital output is LOW.
//    6/ The relay level used for the interlock. Same as above for the interlock pin. Which level HIGH or LOW is considered ON.
//    7/ The flow rate of the pump in Liters/Hour, typically 1.5 or 3.0 L/hour for peristaltic pumps for pools.
//       This is used to estimate how much of the tank we have emptied out.
//    8/ Tankvolume is used to compute the percentage of tank used/remaining

// FiltrationPump: This Pump controls the filtration, no tank attached and not interlocked to any element. SSD relay attached works with HIGH level.
Pump FiltrationPump(FILTRATION_PUMP, FILTRATION_PUMP, (uint8_t)NO_TANK, (uint8_t)NO_INTERLOCK, (uint8_t)RELAY_ACTIVE_HIGH, (uint8_t)RELAY_ACTIVE_HIGH);
// pHPump: This Pump has no low-level switch so remaining volume is estimated. It is interlocked with the relay of the FilrationPump
Pump PhPump(PH_PUMP, PH_PUMP, NO_LEVEL, FiltrationPump.GetRelayReference(), storage.pHPumpFR, storage.pHTankVol, storage.AcidFill);
// ChlPump: This Pump has no low-level switch so remaining volume is estimated. It is interlocked with the relay of the FilrationPump
Pump ChlPump(CHL_PUMP, CHL_PUMP, NO_LEVEL, FiltrationPump.GetRelayReference(), storage.ChlPumpFR, storage.ChlTankVol, storage.ChlFill);
// RobotPump: This Pump is not injecting liquid so tank is associated to it. It is interlocked with the relay of the FilrationPump
Pump RobotPump(ROBOT_PUMP, ROBOT_PUMP, NO_TANK, FiltrationPump.GetRelayReference());
// SWG: This Pump is associated with a Salt Water Chlorine Generator. It turns on and off the equipment to produce chlorine.
// It has no tank associated. It is interlocked with the relay of the FilrationPump
Pump SWG(ORP_PROD, ORP_PROD, NO_TANK, FiltrationPump.GetRelayReference()); // SWG is interlocked with the Pump Relay

// The Relays class to activate and deactivate digital pins
Relay RELAYR0(RELAY_R0);
Relay RELAYR1(RELAY_R1);

// PIDs instances
//Specify the direction and initial tuning parameters
PID PhPID(&storage.PhValue, &storage.PhPIDOutput, &storage.Ph_SetPoint, storage.Ph_Kp, storage.Ph_Ki, storage.Ph_Kd, PhPID_DIRECTION);
PID OrpPID(&storage.OrpValue, &storage.OrpPIDOutput, &storage.Orp_SetPoint, storage.Orp_Kp, storage.Orp_Ki, storage.Orp_Kd, OrpPID_DIRECTION);

// Publishing tasks handles to notify them
static TaskHandle_t pubSetTaskHandle;
static TaskHandle_t pubMeasTaskHandle;

// Mutex to share access to I2C bus among two tasks: AnalogPoll and StatusLights
static SemaphoreHandle_t mutex;

// Functions prototypes
void StartTime(void);
void readLocalTime(void);
bool loadConfig(void);
bool saveConfig(void);
void WiFiEvent(WiFiEvent_t);
void initTimers(void);
void connectToWiFi(void);
void mqttInit(void);                     
void InitTFT(void);
void ResetTFT(void);
void PublishSettings(void);
void SetPhPID(bool);
void SetOrpPID(bool);
int  freeRam (void);
void AnalogInit(void);
void TempInit(void);
bool saveParam(const char*,uint8_t );
unsigned stack_hwm();
void stack_mon(UBaseType_t&);
void info();

// Functions to inform Nextion of bootup process
void SetBoardReady(void);
void SetWifiReady(void);
void SetNTPReady(void);
void UpdateTFT(void);

// Functions used as Tasks
void PoolMaster(void*);
void AnalogPoll(void*);
void pHRegulation(void*);
void OrpRegulation(void*);
void getTemp(void*);
void ProcessCommand(void*);
void SettingsPublish(void*);
void MeasuresPublish(void*);
void StatusLights(void*);

// Setup
void setup()
{
  //Serial port for debug info
  Serial.begin(115200);

  // Set appropriate debug level. The level is defined in PoolMaster.h
  Debug.setDebugLevel(DEBUG_LEVEL);
  Debug.timestampOn();
  Debug.debugLabelOn();
  Debug.formatTimestampOn();

  //get board info
  info();
  
  // Initialize Nextion TFT
  InitTFT();
  ResetTFT();
  SetBoardReady();
  UpdateTFT();
  //Read ConfigVersion. If does not match expected value, restore default values
  if(nvs.begin("PoolMaster",true))
  {
    uint8_t vers = nvs.getUChar("ConfigVersion",0);
    Debug.print(DBG_INFO,"Stored version: %d",vers);
    nvs.end();

    if (vers == CONFIG_VERSION)
    {
      Debug.print(DBG_INFO,"Same version: %d / %d. Loading settings from NVS",vers,CONFIG_VERSION);
      if(loadConfig()) Debug.print(DBG_INFO,"Config loaded"); //Restore stored values from NVS
    }
    else
    {
      Debug.print(DBG_INFO,"New version: %d / %d. Loading new default settings",vers,CONFIG_VERSION);      
      if(saveConfig()) Debug.print(DBG_INFO,"Config saved");  //First time use. Save new default values to NVS
    }

  } else {
    Debug.print(DBG_ERROR,"NVS Error");
    nvs.end();
    Debug.print(DBG_INFO,"New version: %d. First saving of settings",CONFIG_VERSION);      
      if(saveConfig()) Debug.print(DBG_INFO,"Config saved");  //First time use. Save new default values to NVS

  }  

  //Define pins directions
  pinMode(BUZZER, OUTPUT);

  // Warning: pins used here have no pull-ups, provide external ones
  pinMode(CHL_LEVEL, INPUT);
  pinMode(PH_LEVEL, INPUT);

  // Initialize watch-dog
  esp_task_wdt_init(WDT_TIMEOUT, true);

  //Initialize MQTT
  mqttInit();

  // Initialize WiFi events management (on connect/disconnect)
  WiFi.onEvent(WiFiEvent);
  initTimers();
  connectToWiFi();

  delay(500);    // let task start-up and wait for connection
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  SetWifiReady(); // Inform Nextion screen
  UpdateTFT();
  // Config NTP, get time and set system time. This is done here in setup then every day at midnight
  // note: in timeinfo struct, months are from 0 to 11 and years are from 1900. Thus the corrections
  // to pass arguments to setTime which needs months from 1 to 12 and years from 2000...
  // DST (Daylight Saving Time) is managed automatically
  StartTime();
  readLocalTime();
  setTime(timeinfo.tm_hour,timeinfo.tm_min,timeinfo.tm_sec,timeinfo.tm_mday,timeinfo.tm_mon+1,timeinfo.tm_year-100);
  Debug.print(DBG_INFO,"%d/%02d/%02d %02d:%02d:%02d",year(),month(),day(),hour(),minute(),second());
  SetNTPReady();  // Inform Nextion Screen
  UpdateTFT();

  // Initialize the mDNS library.
  while (!MDNS.begin("PoolMaster")) {
    Debug.print(DBG_ERROR,"Error setting up MDNS responder!");
    delay(1000);
  }
  MDNS.addService("http", "tcp", SERVER_PORT);

  // Start I2C for ADS1115 and status lights through PCF8574A
  Wire.begin(I2C_SDA,I2C_SCL);

  // Init pH, ORP and PSI analog measurements
  AnalogInit();

  // Init Water and Air temperatures measurements
  TempInit();

  // Clear status LEDs
  Wire.beginTransmission(PCF8574ADDRESS);
  Wire.write((uint8_t)0xFF);
  Wire.endTransmission();

  // Initialize PIDs
  storage.PhPIDwindowStartTime  = millis();
  storage.OrpPIDwindowStartTime = millis();

  // Limit the PIDs output range in order to limit max. pumps runtime (safety first...)
  PhPID.SetTunings(storage.Ph_Kp, storage.Ph_Ki, storage.Ph_Kd);
  PhPID.SetControllerDirection(PhPID_DIRECTION);
  PhPID.SetSampleTime((int)storage.PhPIDWindowSize);
  PhPID.SetOutputLimits(0, storage.PhPIDWindowSize);    //Whatever happens, don't allow continuous injection of Acid for more than a PID Window

  OrpPID.SetTunings(storage.Orp_Kp, storage.Orp_Ki, storage.Orp_Kd);
  OrpPID.SetControllerDirection(OrpPID_DIRECTION);
  OrpPID.SetSampleTime((int)storage.OrpPIDWindowSize);
  OrpPID.SetOutputLimits(0, storage.OrpPIDWindowSize);  //Whatever happens, don't allow continuous injection of Chl for more than a PID Window

  // PIDs off at start
  SetPhPID (false);
  SetOrpPID(false);

  //Initialize pump instances with stored config data
  FiltrationPump.SetMaxUpTime(0);     //no runtime limit for the filtration pump
  RobotPump.SetMaxUpTime(0);          //no runtime limit for the robot pump

  PhPump.SetFlowRate(storage.pHPumpFR);
  PhPump.SetTankVolume(storage.pHTankVol);
  PhPump.SetTankFill(storage.AcidFill);
  PhPump.SetMaxUpTime(storage.PhPumpUpTimeLimit * 1000);

  ChlPump.SetFlowRate(storage.ChlPumpFR);
  ChlPump.SetTankVolume(storage.ChlTankVol);
  ChlPump.SetTankFill(storage.ChlFill);
  ChlPump.SetMaxUpTime(storage.ChlPumpUpTimeLimit * 1000);

  // Configure SWG relay as bistable (simulate a button press to turn on and off the Salt Water Chlorine generator)
  // default relay type for class Pump
  SWG.SetRelayType(RELAY_BISTABLE); 
  //SWG.GetRelayReference()->SetBistableDelay(500); // Default delay between up and down for bistable relay is 500ms. Can be changed here.

  // Start filtration pump at power-on if within scheduled time slots -- You can choose not to do this and start pump manually
  //if (storage.AutoMode && (hour() >= storage.FiltrationStart) && (hour() < storage.FiltrationStop))
  //  FiltrationPump.Start();
  //else FiltrationPump.Stop();

  // Robot pump off at start
  RobotPump.Stop();

  // Create queue for external commands
  queueIn = xQueueCreate((UBaseType_t)QUEUE_ITEMS_NBR,(UBaseType_t)QUEUE_ITEM_SIZE);

  // Create loop tasks in the scheduler.
  //------------------------------------
  int app_cpu = xPortGetCoreID();

  Debug.print(DBG_DEBUG,"Creating loop Tasks");

  // Create I2C sharing mutex
  mutex = xSemaphoreCreateMutex();

  // Analog measurement polling task
  xTaskCreatePinnedToCore(
    AnalogPoll,
    "AnalogPoll",
    3072,
    NULL,
    1,
    nullptr,
    app_cpu
  );

  // MQTT commands processing
  xTaskCreatePinnedToCore(
    ProcessCommand,
    "ProcessCommand",
    3584,
    NULL,
    1,
    nullptr,
    app_cpu
  );

  // PoolMaster: Supervisory task
  xTaskCreatePinnedToCore(
    PoolMaster,
    "PoolMaster",
    5120,
    NULL,
    1,
    nullptr,
    app_cpu
  );

  // Temperatures measurement
  xTaskCreatePinnedToCore(
    getTemp,
    "GetTemp",
    3072,
    NULL,
    1,
    nullptr,
    app_cpu
  );
  
 // ORP regulation loop
    xTaskCreatePinnedToCore(
    OrpRegulation,
    "ORPRegulation",
    2048,
    NULL,
    1,
    nullptr,
    app_cpu
  );

  // pH regulation loop
    xTaskCreatePinnedToCore(
    pHRegulation,
    "pHRegulation",
    2048,
    NULL,
    1,
    nullptr,
    app_cpu
  );

  // Status lights display
  xTaskCreatePinnedToCore(
    StatusLights,
    "StatusLights",
    2048,
    NULL,
    1,
    nullptr,
    app_cpu
  );  

  // Measures MQTT publish 
  xTaskCreatePinnedToCore(
    MeasuresPublish,
    "MeasuresPublish",
    3072,
    NULL,
    1,
    &pubMeasTaskHandle,               // needed to notify task later
    app_cpu
  );

  // MQTT Settings publish 
  xTaskCreatePinnedToCore(
    SettingsPublish,
    "SettingsPublish",
    3584,
    NULL,
    1,
    &pubSetTaskHandle,                // needed to notify task later
    app_cpu
  );

#ifdef ELEGANT_OTA
// ELEGANTOTA Configuration
  server.on("/", []() {
    server.send(200, "text/plain", "NA");
  });


  ElegantOTA.begin(&server);    // Start ElegantOTA
  server.begin();
  Serial.println("HTTP server started");
  // Set Authentication Credentials
#ifdef ELEGANT_OTA_AUTH
  ElegantOTA.setAuth(ELEGANT_OTA_USERNAME, ELEGANT_OTA_PASSWORD);
#endif
#endif

  // Initialize OTA (On The Air update)
  //-----------------------------------
  ArduinoOTA.setPort(OTA_PORT);
  ArduinoOTA.setHostname("PoolMaster");
  ArduinoOTA.setPasswordHash(OTA_PWDHASH);
  
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
    Debug.print(DBG_INFO,"Start updating %s",type);
  });
  ArduinoOTA.onEnd([]() {
  Debug.print(DBG_INFO,"End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    esp_task_wdt_reset();           // reset Watchdog as upload may last some time...
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Debug.print(DBG_ERROR,"Error[%u]: ", error);
    if      (error == OTA_AUTH_ERROR)    Debug.print(DBG_ERROR,"Auth Failed");
    else if (error == OTA_BEGIN_ERROR)   Debug.print(DBG_ERROR,"Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Debug.print(DBG_ERROR,"Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Debug.print(DBG_ERROR,"Receive Failed");
    else if (error == OTA_END_ERROR)     Debug.print(DBG_ERROR,"End Failed");
  });

  ArduinoOTA.begin();

  //display remaining RAM/Heap space.
  Debug.print(DBG_DEBUG,"[memCheck] Stack: %d bytes - Heap: %d bytes",stack_hwm(),freeRam());

  // Start loops tasks
  Debug.print(DBG_INFO,"Init done, starting loop tasks");
  startTasks = true;

  delay(1000);          // wait for tasks to start

}

bool loadConfig()
{
  nvs.begin("PoolMaster",true);

  storage.ConfigVersion         = nvs.getUChar("ConfigVersion",0);
  storage.Ph_RegulationOnOff    = nvs.getBool("Ph_RegOnOff",false);
  storage.Orp_RegulationOnOff   = nvs.getBool("Orp_RegOnOff",false);  
  storage.AutoMode              = nvs.getBool("AutoMode",true);
  storage.WinterMode            = nvs.getBool("WinterMode",false);
  storage.FiltrationDuration    = nvs.getUChar("FiltrDuration",12);
  storage.FiltrationStart       = nvs.getUChar("FiltrStart",8);
  storage.FiltrationStop        = nvs.getUChar("FiltrStop",20);
  storage.FiltrationStartMin    = nvs.getUChar("FiltrStartMin",8);
  storage.FiltrationStopMax     = nvs.getUChar("FiltrStopMax",22);
  storage.DelayPIDs             = nvs.getUChar("DelayPIDs",0);
  storage.PhPumpUpTimeLimit     = nvs.getULong("PhPumpUTL",900);
  storage.ChlPumpUpTimeLimit    = nvs.getULong("ChlPumpUTL",2500);
  storage.PublishPeriod         = nvs.getULong("PublishPeriod",PUBLISHINTERVAL);
  storage.PhPIDWindowSize       = nvs.getULong("PhPIDWSize",60000);
  storage.OrpPIDWindowSize      = nvs.getULong("OrpPIDWSize",60000);
  storage.PhPIDwindowStartTime  = nvs.getULong("PhPIDwStart",0);
  storage.OrpPIDwindowStartTime = nvs.getULong("OrpPIDwStart",0);
  storage.Ph_SetPoint           = nvs.getDouble("Ph_SetPoint",7.3);
  storage.Orp_SetPoint          = nvs.getDouble("Orp_SetPoint",750);
  storage.PSI_HighThreshold     = nvs.getDouble("PSI_High",0.5);
  storage.PSI_MedThreshold      = nvs.getDouble("PSI_Med",0.25);
  storage.WaterTempLowThreshold = nvs.getDouble("WaterTempLow",10.);
  storage.WaterTemp_SetPoint    = nvs.getDouble("WaterTempSet",27.);
  storage.AirTemp          = nvs.getDouble("TempExternal",3.);
  storage.pHCalibCoeffs0        = nvs.getDouble("pHCalibCoeffs0",-2.49);
  storage.pHCalibCoeffs1        = nvs.getDouble("pHCalibCoeffs1",6.87);
  storage.OrpCalibCoeffs0       = nvs.getDouble("OrpCalibCoeffs0",431.03);
  storage.OrpCalibCoeffs1       = nvs.getDouble("OrpCalibCoeffs1",0);
  storage.PSICalibCoeffs0       = nvs.getDouble("PSICalibCoeffs0",0.377923399);
  storage.PSICalibCoeffs1       = nvs.getDouble("PSICalibCoeffs1",-0.17634473);
  storage.Ph_Kp                 = nvs.getDouble("Ph_Kp",2000000.);
  storage.Ph_Ki                 = nvs.getDouble("Ph_Ki",0.);
  storage.Ph_Kd                 = nvs.getDouble("Ph_Kd",0.);
  storage.Orp_Kp                = nvs.getDouble("Orp_Kp",2500.);
  storage.Orp_Ki                = nvs.getDouble("Orp_Ki",0.);
  storage.Orp_Kd                = nvs.getDouble("Orp_Kd",0.);
  storage.PhPIDOutput           = nvs.getDouble("PhPIDOutput",0.);
  storage.OrpPIDOutput          = nvs.getDouble("OrpPIDOutput",0.);
  storage.WaterTemp             = nvs.getDouble("TempValue",18.);
  storage.PhValue               = nvs.getDouble("PhValue",0.);
  storage.OrpValue              = nvs.getDouble("OrpValue",0.);
  storage.PSIValue              = nvs.getDouble("PSIValue",0.4);
  storage.AcidFill              = nvs.getDouble("AcidFill",100.);
  storage.ChlFill               = nvs.getDouble("ChlFill",100.);
  storage.pHTankVol             = nvs.getDouble("pHTankVol",20.);
  storage.ChlTankVol            = nvs.getDouble("ChlTankVol",20.);
  storage.pHPumpFR              = nvs.getDouble("pHPumpFR",1.5);
  storage.ChlPumpFR             = nvs.getDouble("ChlPumpFR",1.5);
  storage.SecureElectro         = nvs.getUChar("SecureElectro",15);  //ajout
  storage.DelayElectro          = nvs.getUChar("DelayElectro",2);  //ajout
  storage.ElectrolyseMode       = nvs.getBool("ElectrolyseMode",false);  //ajout
  storage.pHAutoMode            = nvs.getBool("pHAutoMode",false);  //ajout
  storage.OrpAutoMode           = nvs.getBool("OrpAutoMode",false);  //ajout


  nvs.end();

  Debug.print(DBG_INFO,"%d",storage.ConfigVersion);
  Debug.print(DBG_INFO,"%d, %d, %d, %d",storage.Ph_RegulationOnOff,storage.Orp_RegulationOnOff,storage.AutoMode,storage.WinterMode);
  Debug.print(DBG_INFO,"%d, %d, %d, %d, %d, %d",storage.FiltrationDuration,storage.FiltrationStart,storage.FiltrationStop,
              storage.FiltrationStartMin,storage.FiltrationStopMax,storage.DelayPIDs);
  Debug.print(DBG_INFO,"%d, %d, %d",storage.PhPumpUpTimeLimit,storage.ChlPumpUpTimeLimit,storage.PublishPeriod);
  Debug.print(DBG_INFO,"%d, %d, %d, %d",storage.PhPIDWindowSize,storage.OrpPIDWindowSize,storage.PhPIDwindowStartTime,storage.OrpPIDwindowStartTime);
  Debug.print(DBG_INFO,"%3.1f, %4.0f, %3.1f, %3.1f, %3.0f, %3.0f, %4.1f, %8.6f, %9.6f, %11.6f, %11.6f, %3.1f, %3.1f",
              storage.Ph_SetPoint,storage.Orp_SetPoint,storage.PSI_HighThreshold,
              storage.PSI_MedThreshold,storage.WaterTempLowThreshold,storage.WaterTemp_SetPoint,storage.AirTemp,
              storage.pHCalibCoeffs0,storage.pHCalibCoeffs1,storage.OrpCalibCoeffs0,storage.OrpCalibCoeffs1,
              storage.PSICalibCoeffs0,storage.PSICalibCoeffs1);
  Debug.print(DBG_INFO,"%8.0f, %3.0f, %3.0f, %6.0f, %3.0f, %3.0f, %7.0f, %7.0f, %4.2f, %4.2f, %4.0f, %4.2f",
              storage.Ph_Kp,storage.Ph_Ki,storage.Ph_Kd,storage.Orp_Kp,storage.Orp_Ki,storage.Orp_Kd,
              storage.PhPIDOutput,storage.OrpPIDOutput,storage.WaterTemp,storage.PhValue,storage.OrpValue,storage.PSIValue);
  Debug.print(DBG_INFO,"%3.0f, %3.0f, %3.0f, %3.0f, %3.1f, %3.1f ",storage.AcidFill,storage.ChlFill,storage.pHTankVol,storage.ChlTankVol,
              storage.pHPumpFR,storage.ChlPumpFR);
  Debug.print(DBG_INFO,"%d, %d, %d, %d, %d",storage.SecureElectro,storage.DelayElectro,storage.ElectrolyseMode,storage.pHAutoMode,
              storage.OrpAutoMode);

  return (storage.ConfigVersion == CONFIG_VERSION);
}

bool saveConfig()
{
  nvs.begin("PoolMaster",false);

  size_t i = nvs.putUChar("ConfigVersion",storage.ConfigVersion);
  i += nvs.putBool("Ph_RegOnOff",storage.Ph_RegulationOnOff);
  i += nvs.putBool("Orp_RegOnOff",storage.Orp_RegulationOnOff);  
  i += nvs.putBool("AutoMode",storage.AutoMode);
  i += nvs.putBool("WinterMode",storage.WinterMode);
  i += nvs.putUChar("FiltrDuration",storage.FiltrationDuration);
  i += nvs.putUChar("FiltrStart",storage.FiltrationStart);
  i += nvs.putUChar("FiltrStop",storage.FiltrationStop);
  i += nvs.putUChar("FiltrStartMin",storage.FiltrationStartMin);
  i += nvs.putUChar("FiltrStopMax",storage.FiltrationStopMax);
  i += nvs.putUChar("DelayPIDs",storage.DelayPIDs);
  i += nvs.putULong("PhPumpUTL",storage.PhPumpUpTimeLimit);
  i += nvs.putULong("ChlPumpUTL",storage.ChlPumpUpTimeLimit);
  i += nvs.putULong("PublishPeriod",storage.PublishPeriod);
  i += nvs.putULong("PhPIDWSize",storage.PhPIDWindowSize);
  i += nvs.putULong("OrpPIDWSize",storage.OrpPIDWindowSize);
  i += nvs.putULong("PhPIDwStart",storage.PhPIDwindowStartTime);
  i += nvs.putULong("OrpPIDwStart",storage.OrpPIDwindowStartTime);
  i += nvs.putDouble("Ph_SetPoint",storage.Ph_SetPoint);
  i += nvs.putDouble("Orp_SetPoint",storage.Orp_SetPoint);
  i += nvs.putDouble("PSI_High",storage.PSI_HighThreshold);
  i += nvs.putDouble("PSI_Med",storage.PSI_MedThreshold);
  i += nvs.putDouble("WaterTempLow",storage.WaterTempLowThreshold);
  i += nvs.putDouble("WaterTempSet",storage.WaterTemp_SetPoint);
  i += nvs.putDouble("TempExternal",storage.AirTemp);
  i += nvs.putDouble("pHCalibCoeffs0",storage.pHCalibCoeffs0);
  i += nvs.putDouble("pHCalibCoeffs1",storage.pHCalibCoeffs1);
  i += nvs.putDouble("OrpCalibCoeffs0",storage.OrpCalibCoeffs0);
  i += nvs.putDouble("OrpCalibCoeffs1",storage.OrpCalibCoeffs1);
  i += nvs.putDouble("PSICalibCoeffs0",storage.PSICalibCoeffs0);
  i += nvs.putDouble("PSICalibCoeffs1",storage.PSICalibCoeffs1);
  i += nvs.putDouble("Ph_Kp",storage.Ph_Kp);
  i += nvs.putDouble("Ph_Ki",storage.Ph_Ki);
  i += nvs.putDouble("Ph_Kd",storage.Ph_Kd);
  i += nvs.putDouble("Orp_Kp",storage.Orp_Kp);
  i += nvs.putDouble("Orp_Ki",storage.Orp_Ki);
  i += nvs.putDouble("Orp_Kd",storage.Orp_Kd);
  i += nvs.putDouble("PhPIDOutput",storage.PhPIDOutput);
  i += nvs.putDouble("OrpPIDOutput",storage.OrpPIDOutput);
  i += nvs.putDouble("TempValue",storage.WaterTemp);
  i += nvs.putDouble("PhValue",storage.PhValue);
  i += nvs.putDouble("OrpValue",storage.OrpValue);
  i += nvs.putDouble("PSIValue",storage.PSIValue);
  i += nvs.putDouble("AcidFill",storage.AcidFill);
  i += nvs.putDouble("ChlFill",storage.ChlFill);
  i += nvs.putDouble("pHTankVol",storage.pHTankVol);
  i += nvs.putDouble("ChlTankVol",storage.ChlTankVol);
  i += nvs.putDouble("pHPumpFR",storage.pHPumpFR);
  i += nvs.putDouble("ChlPumpFR",storage.ChlPumpFR);
  i += nvs.putUChar("SecureElectro",storage.SecureElectro);  //ajout
  i += nvs.putUChar("DelayElectro",storage.DelayElectro);  //ajout
  i += nvs.putBool("ElectrolyseMode",storage.ElectrolyseMode);  //ajout
  i += nvs.putBool("pHAutoMode",storage.pHAutoMode);  //ajout
  i += nvs.putBool("OrpAutoMode",storage.OrpAutoMode);  //ajout
  nvs.end();

  Debug.print(DBG_INFO,"Bytes saved: %d / %d\n",i,sizeof(storage));
  return (i == sizeof(storage)) ;

}

// functions to save any type of parameter (4 overloads with same name but different arguments)

bool saveParam(const char* key, uint8_t val)
{
  nvs.begin("PoolMaster",false);
  size_t i = nvs.putUChar(key,val);
  return(i == sizeof(val));
}

bool saveParam(const char* key, bool val)
{
  nvs.begin("PoolMaster",false);
  size_t i = nvs.putBool(key,val);
  return(i == sizeof(val));
}

bool saveParam(const char* key, unsigned long val)
{
  nvs.begin("PoolMaster",false);
  size_t i = nvs.putULong(key,val);
  return(i == sizeof(val));
}

bool saveParam(const char* key, double val)
{
  nvs.begin("PoolMaster",false);
  size_t i = nvs.putDouble(key,val);
  return(i == sizeof(val));
}

//Compute free RAM
//useful to check if it does not shrink over time
int freeRam () {
  int v = xPortGetFreeHeapSize();
  return v;
}

// Get current free stack 
unsigned stack_hwm(){
  return uxTaskGetStackHighWaterMark(nullptr);
}

// Monitor free stack (display smallest value)
void stack_mon(UBaseType_t &hwm)
{
  UBaseType_t temp = uxTaskGetStackHighWaterMark(nullptr);
  if(!hwm || temp < hwm)
  {
    hwm = temp;
    Debug.print(DBG_DEBUG,"[stack_mon] %s: %d bytes",pcTaskGetTaskName(NULL), hwm);
  }  
}

// Get exclusive access of I2C bus
void lockI2C(){
  xSemaphoreTake(mutex, portMAX_DELAY);
}

// Release I2C bus access
void unlockI2C(){
  xSemaphoreGive(mutex);  
}

// Set time parameters, including DST
void StartTime(){
  configTime(0, 0,"0.pool.ntp.org","1.pool.ntp.org","2.pool.ntp.org"); // 3 possible NTP servers
  setenv("TZ","CET-1CEST,M3.5.0/2,M10.5.0/3",3);                       // configure local time with automatic DST  
  tzset();
  int retry = 0;
  const int retry_count = 15;
  while(sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count){
    Serial.print(".");
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
  Serial.println("");
  Debug.print(DBG_INFO,"NTP configured");
}

void readLocalTime(){
  if(!getLocalTime(&timeinfo,5000U)){
    Debug.print(DBG_WARNING,"Failed to obtain time");
  }
  Serial.println(&timeinfo,"%A, %B %d %Y %H:%M:%S");
}

// Notify PublishSettings task 
void PublishSettings()
{
  xTaskNotifyGive(pubSetTaskHandle);
}

// Notify PublishMeasures task
void PublishMeasures()
{
  xTaskNotifyGive(pubMeasTaskHandle);
}

//board info
void info(){
  esp_chip_info_t out_info;
  esp_chip_info(&out_info);
  Debug.print(DBG_INFO,"CPU frequency       : %dMHz",ESP.getCpuFreqMHz());
  Debug.print(DBG_INFO,"CPU Cores           : %d",out_info.cores);
  Debug.print(DBG_INFO,"Flash size          : %dMB",ESP.getFlashChipSize()/1000000);
  Debug.print(DBG_INFO,"Free RAM            : %d bytes",ESP.getFreeHeap());
  Debug.print(DBG_INFO,"Min heap            : %d bytes",esp_get_free_heap_size());
  Debug.print(DBG_INFO,"tskIDLE_PRIORITY    : %d",tskIDLE_PRIORITY);
  Debug.print(DBG_INFO,"confixMAX_PRIORITIES: %d",configMAX_PRIORITIES);
  Debug.print(DBG_INFO,"configTICK_RATE_HZ  : %d",configTICK_RATE_HZ);
}

// Pseudo loop, which deletes loopTask of the Arduino framework
void loop()
{
  delay(1000);
  vTaskDelete(nullptr);
}