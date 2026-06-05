// ============================================================
// ESP32-PoolMaster Configuration
// ============================================================

// ============================================================
// HARDWARE SELECTION
// ============================================================
// Uncomment ONE of the following to select your hardware:
// - ESP32_DEVKITV1: Original board (direct GPIO, no I2C relays)
// - KC868_A8: Kincony KC868-A8 (PCF8574 I2C relays + inputs)
// ============================================================
// Hardware is selected via platformio.ini build_flags (-D KC868_A8 or -D ESP32_DEVKITV1)
// Do NOT define here.

#ifdef KC868_A8
  #include "KC868A8_Pins.h"  // Virtual pin definitions for KC868-A8
#endif

// Firmware revisions
#define FIRMW "ESP-4.62"
#define TFT_FIRMW "TFT-4.0" // For compatibility

// Choose Nextion version to Compile (only one choice possible)
// NEXTION_V1 (default) for initial interface or blue theme interface
// NEXTION_V2 for Nextion v5.0 them which redefines full interface and pages
//#define NEXTION_V1
#define NEXTION_V2

#define DEBUG_LEVEL DBG_INFO     // Possible levels : NONE/ERROR/WARNING/INFO/DEBUG/VERBOSE

//Version of config stored in EEPROM
//Random value. Change this value (to any other value) to revert the config to default values
#define CONFIG_VERSION 50

// ============================================================
// TFT DISPLAY CONFIGURATION
// ============================================================
// Uncomment to enable TFT/Nextion display support
// When not defined, all Nextion code is compiled out
// ============================================================
// #define TFT_CONNECTED

// ============================================================
// SENSOR SIMULATION CONFIGURATION
// ============================================================
// Set to 1 to enable simulation for individual sensors.
// When enabled, the simulated value replaces the real sensor value.
// When disabled, the real sensor value is used.
// ============================================================
#define SIMU_CHL_LEVEL      1       // Chlorine tank level simulation
#define SIMU_PH_LEVEL       1       // Acid tank level simulation
#define SIMU_POOL_LEVEL     1       // Pool water level simulation
#define SIMU_PH             1       // pH sensor simulation
#define SIMU_ORP            1       // ORP sensor simulation
#define SIMU_PSI            1       // Pressure sensor simulation

// Simulation default values (used when simulation is enabled)
#define SIMU_PH_VALUE       7.2     // Default simulated pH
#define SIMU_ORP_VALUE      720.0   // Default simulated ORP (mV)
#define SIMU_PSI_VALUE      0.1    // Default simulated pressure (bar)
#define SIMU_CHL_LEVEL_VALUE  0     // Default: Low (tank not empty)
#define SIMU_PH_LEVEL_VALUE   0     // Default: Low (tank not empty)
#define SIMU_POOL_LEVEL_VALUE 1     // Default: HIGH (pool level OK)

// ============================================================
// ANALOG SIMULATION DYNAMICS (pH and ORP)
// ============================================================
// When a pump is active, the sensor value moves in the direction
// the chemical causes. When the pump is off, the value drifts
// naturally in the opposite direction.
//
// Update interval: Every 60 seconds (hardcoded in AnalogSimLoop)
// ============================================================

// pH simulation
#define SIM_PH_ACTIVE_RATE    0.02  // pH decrease per minute when acid pump is ON
#define SIM_PH_DRIFT_RATE     0.005 // pH increase per minute when acid pump is OFF (natural)
#define SIM_PH_MIN            6.0   // Minimum simulated pH
#define SIM_PH_MAX            8.5   // Maximum simulated pH

// ORP simulation
#define SIM_ORP_ACTIVE_RATE   5.0   // ORP increase per minute when chlorine pump is ON (mV)
#define SIM_ORP_DRIFT_RATE    2.0   // ORP decrease per minute when chlorine pump is OFF (mV)
#define SIM_ORP_MIN           400.0 // Minimum simulated ORP (mV)
#define SIM_ORP_MAX           900.0 // Maximum simulated ORP (mV)

// If you need to force network parameters (configuration with no screen)
#define FORCE_NETWORK_PARAMS
#ifdef FORCE_NETWORK_PARAMS
  #define FWIFI_NETWORK "Mayer2"
  #define FWIFI_PASSWORD "Moritz26tOR"
  #define FMQTT_SERVER "192.168.178.223"
  #define FMQTT_PORT 1883
  #define FMQTT_LOGIN "<MQTT_LOGIN>"
  #define FMQTT_PASS "<MQTT_PWD>"
#endif

// Compile on development environment or production (if not defined)
//#define DEVT // Value defined in platformio.ini

// WiFi credentials
#define WIFI_SCAN_INTERVAL  10000

#define OTA_PWDHASH   "<OTA_PASS>"
#ifdef DEVT
  #define HOSTNAME "PoolMaster_Dev"
#else
  #define HOSTNAME "PoolMaster"
#endif 

// Mail parameters and credentials
//#define SMTP  // define to activate SMTP email notifications

// PID Directions (either DIRECT or REVERSE depending on Ph/Orp correction vs water properties)
#define PhPID_DIRECTION REVERSE
#define OrpPID_DIRECTION DIRECT

// ============================================================
// PIN DEFINITIONS (Hardware-dependent)
// ============================================================

#ifdef KC868_A8
  // KC868-A8: Relays and inputs via PCF8574 I2C expanders
  // Virtual pins 100-107 (relays), 110-117 (inputs)
  #define FILTRATION      VPIN_FILTRATION     // 100
  #define ROBOT           VPIN_ROBOT          // 101
  #define PH_PUMP         VPIN_PH_PUMP        // 102
  #define CHL_PUMP        VPIN_CHL_PUMP       // 103
  #define PROJ            VPIN_PROJ           // 104
  #define SPARE           VPIN_SPARE          // 105
  #define SWG_PUMP        VPIN_SWG_PUMP       // 106
  #define FILL_PUMP       VPIN_FILL_PUMP      // 107
  
  #define CHL_LEVEL       VPIN_CHL_LEVEL      // 110
  #define PH_LEVEL        VPIN_PH_LEVEL       // 111
  #define POOL_LEVEL      VPIN_POOL_LEVEL     // 112
  
  #define ONE_WIRE_BUS_A  KC868_ONEWIRE_AIR   // GPIO14
  #define ONE_WIRE_BUS_W  KC868_ONEWIRE_WATER // GPIO13
  
  #define I2C_SDA         KC868_I2C_SDA       // GPIO4
  #define I2C_SCL         KC868_I2C_SCL       // GPIO5
  #define PCF8574ADDRESS  0x20                // Status LEDs (not used on KC868-A8)
  
  #define BUZZER          KC868_BUZZER        // GPIO27
  
  #define ALL_PINS        "100|101|102|103|104|105|106|107"
  
  // ADS1115 not used on KC868-A8 (pH/ORP/PSI via simulation or external)
  #undef EXT_ADS1115
  #define INT_ADS1115_ADDR ADS1115ADDRESS
  #undef EXT_ADS1115_ADDR

#else
  // Original ESP32 DevKit: Direct GPIO connections
  #define FILTRATION      32
  #define ROBOT           33
  #define PH_PUMP         25
  #define CHL_PUMP        26
  #define PROJ            27  // Projecteur
  #define SPARE           4
  #define SWG_PUMP        13
  #define FILL_PUMP       23
  
  #define CHL_LEVEL       39   // Chlorine tank empty switch
  #define PH_LEVEL        36   // Acid tank empty switch
  #define POOL_LEVEL      34   // Pool level switch
  
  #define ONE_WIRE_BUS_A  18
  #define ONE_WIRE_BUS_W  19
  
  #define I2C_SDA         21
  #define I2C_SCL         22
  #define PCF8574ADDRESS  0x20 // 0x20 for PCF8574(N) or 0x38 for PCF8574A(N)
  
  #define BUZZER          2
  
  #define ALL_PINS        "4|13|23|25|26|27|32|33"
  
  // Type of pH and Orp sensors acquisition
  #define EXT_ADS1115
  #define INT_ADS1115_ADDR ADS1115ADDRESS
  #define EXT_ADS1115_ADDR ADS1115ADDRESS+1
#endif

// Task TimeOut before reboot
#define WDT_TIMEOUT     10

// Delay when instructed to reboot
#define REBOOT_DELAY  10000

// Server port
//#define SERVER_PORT 8060

//OTA port
#define OTA_PORT    8063

//12bits (0,06°C) temperature sensors resolution
#define TEMPERATURE_RESOLUTION 12

// Time to wait for Wifi to connect. If not connected resume startup without network connection
#define WIFI_TIMEOUT  10000

//MQTT stuff including local broker/server IP address, login and pwd
//------------------------------------------------------------------

//interval (in millisec) between MQTT publishement of measurement data
// can be configured at runtime
#define PUBLISHINTERVAL 30000

#define CONFIG_NVS_NAME "MasterConfig" // NVS namespace for configuration storage

// Default values if nothing better is recorded at runtime
#define POOLTOPIC "Home/Pool/"
#define MQTTID "PoolMaster"
// ElegantOTA Config
//#define ELEGANT_OTA

#ifdef ELEGANT_OTA
  //#define ELEGANT_OTA_AUTH
  //#define ELEGANT_OTA_USERNAME  "username"
  //#define ELEGANT_OTA_PASSWORD  "password"
#endif
// Robot pump timing
#define ROBOT_DELAY 60     // Robot start delay after filtration in mn
#define ROBOT_DURATION 90  // Robot cleaning duration

#define SWG_MODE_ADJUST 0   // Adjust SWG production time according to the pool ORP
#define SWG_MODE_FIXED 1    // Fixed time for SWG production (in hours)

//Display timeout before blanking
//-------------------------------
//#define TFT_SLEEP 60000L  // Moved to Nextion Library

// Loop tasks scheduling parameters
//---------------------------------
// T1: AnalogPoll
// T2: PoolServer
// T3: PoolMaster
// T4: getTemp
// T5: OrpRegulation
// T6: pHRegulation
// T7: StatusLights
// T8: PublishMeasures
// T9: PublishSettings 
// T10: Nextion Screen Refresh On (/2 when screen on, /4 if menu page for faster refresh)
// T11: Every 2 minutes, statistics recording, MQTT and NTP reconnects

//Periods 
// Task9 period is initialized with PUBLISHINTERVAL and can be changed dynamically
#define PT1 125
#define PT2 500
#define PT3 500
#define PT4 1000 / (1 << (12 - TEMPERATURE_RESOLUTION))
#define PT5 1000
#define PT6 1000
#define PT7 3000
#define PT8 30000 //Unused, stored as config parameter and can be modified at runtime
#define PT9 1000
#define PT10 1000
#define PT11 120000 // Run once every 2 minutes


//Start offsets to spread tasks along time
// Task1 has no delay
#define DT2 190/portTICK_PERIOD_MS
#define DT3 310/portTICK_PERIOD_MS
#define DT4 440/portTICK_PERIOD_MS
#define DT5 560/portTICK_PERIOD_MS
#define DT6 920/portTICK_PERIOD_MS
#define DT7 100/portTICK_PERIOD_MS
#define DT8 570/portTICK_PERIOD_MS
#define DT9 940/portTICK_PERIOD_MS
#define DT10 50/portTICK_PERIOD_MS
#define DT11 2000/portTICK_PERIOD_MS

//#define CHRONO                    // Activate tasks timings traces for profiling
