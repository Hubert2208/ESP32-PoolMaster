
// Credits : 
//  https://github.com/hugokernel/esphome-water-meter/blob/master/README.md

#include <Arduino.h>
#include "Config.h"
#include "PoolMaster.h"

#if defined(_EXTENSIONS_)

#include "Extension_WaterMeter_Pulse.h"
extern void SuperVisor_Message(const char *, char*);

ExtensionStruct myWaterMeterPulse = {0};
static double   myWaterMeterCounter = 0;
static double   myWMLiterPerPulse   = 1.0;  // K=1 liter/pulse
static uint32_t myWMDebounce        = 25;   // Debounce time in ms
static int      myWMGPIO            = 0;    // disabled by default, suggest GPIO 15
ConfigManager   WMConfig;

enum WMParamID {
    WATERMETERLITER,
    WATERMETERPULSE,
    WATERMETERDEBOUNCE,
    WATERMETERGPIO,
};

extern void PublishTopic(const char*, JsonDocument&);
void WaterMeterPulsePubMQTT(void)
{
    static double oldvalues = -1;
    double n = myWaterMeterCounter+myWMLiterPerPulse+myWMDebounce+myWMGPIO;
    if (n != oldvalues) oldvalues=n;
    else return; // no change to publish

    DynamicJsonDocument root(1024);
    root["L"]    = (int)myWaterMeterCounter;
    root["K"]    = myWMLiterPerPulse;
    root["D"]    = myWMDebounce;
    root["GPIO"] = myWMGPIO;
    char topic[50];
    const char *roottopic = PMConfig.get<const char*>(MQTT_TOPIC);
    if (strcmp(roottopic, "") == 0) return;
    if (strcmp(roottopic, "none") == 0) return;
    sprintf(topic, "%s/%s", roottopic, myWaterMeterPulse.name);
    PublishTopic(topic, root);
}

void WaterMeterPulseValues(char* buffer)
{
    if (myWMGPIO>0)
        sprintf(buffer, "L=%.0f K=%.2f D=%d GPIO=%d", myWaterMeterCounter, myWMLiterPerPulse, myWMDebounce, myWMGPIO);
    else strcpy(buffer, "none");
}

void WaterMeterPulseLoadSettings(void *pvParameters)
{
    static bool initvalues = true;
    if (initvalues) {
        initvalues = false;
        myWaterMeterCounter = WMConfig.get<double>(WATERMETERLITER);
        myWMLiterPerPulse   = WMConfig.get<double>(WATERMETERPULSE);
        myWMDebounce        = WMConfig.get<uint32_t>(WATERMETERDEBOUNCE);
        myWMGPIO            = WMConfig.get<uint8_t>(WATERMETERGPIO);
        pinMode(myWMGPIO, INPUT_PULLUP);
    }

    // Get Watermeter settings from SuperVisor, if any
    char buffer[I2C_MAXMESSAGE+5] = {0};
    SuperVisor_Message("GET_WATERMETER_COUNTER", buffer);
    if (strcmp(buffer, "none")!=0) {
        sscanf(buffer, "%lf", &myWaterMeterCounter);
        WMConfig.put<double>(WATERMETERLITER, myWaterMeterCounter);
    }
    SuperVisor_Message("GET_WATERMETER_K", buffer);
    if (strcmp(buffer, "none")!=0) {
        sscanf(buffer, "%lf", &myWMLiterPerPulse);
        WMConfig.put<double>(WATERMETERPULSE, myWMLiterPerPulse);
    }
    SuperVisor_Message("GET_WATERMETER_D", buffer);
    if (strcmp(buffer, "none")!=0) {
        sscanf(buffer, "%d", &myWMDebounce);
        WMConfig.put<uint32_t>(WATERMETERDEBOUNCE, myWMDebounce);
    }
    SuperVisor_Message("GET_WATERMETER_GPIO", buffer);
    if (strcmp(buffer, "none")!=0) {
        sscanf(buffer, "%d", &myWMGPIO);
        WMConfig.put<uint8_t>(WATERMETERGPIO, myWMGPIO);
        pinMode(myWMGPIO, INPUT_PULLUP);
    }

    WaterMeterPulsePubMQTT();
    //Debug.print(DBG_INFO, "[WaterMeterPulseLoadSettings] with myWMGPIO=%d\n", myWMGPIO);
}

void WaterMeterPulseTask(void *pvParameters)
{
    if (myWMGPIO<1) return;

    // Debounce is managed by the loop
    static uint8_t LastReading = HIGH;
    static bool meterblocked = false;   // meter can stop when GPIO is at LOW level

    uint8_t Reading = digitalRead(myWMGPIO);

    if (Reading != LastReading) meterblocked = false; // state has changed
    LastReading = Reading;

    if ((Reading == LOW) && (!meterblocked)) {
        meterblocked = true;
        myWaterMeterCounter += myWMLiterPerPulse;
        WMConfig.put<double>(WATERMETERLITER, myWaterMeterCounter);
        WaterMeterPulsePubMQTT();
//      Debug.print(DBG_INFO,"[WaterMeterPulseTask] counter=%d\n", myWaterMeterCounter);
    }
}

ExtensionStruct WaterMeterPulse_Init(char *name, int defaultIO)
{
    WMConfig.SetNamespace("WaterMeter");
    WMConfig.initParam(WATERMETERLITER,    "WMLiter",   (double)myWaterMeterCounter);
    WMConfig.initParam(WATERMETERPULSE,    "WMPulse",   (double)myWMLiterPerPulse);
    WMConfig.initParam(WATERMETERDEBOUNCE, "WMDebounce",(uint32_t)myWMDebounce);
    WMConfig.initParam(WATERMETERGPIO,     "WMGPIO",    (uint8_t)defaultIO);

    // Init structure
    myWaterMeterPulse.name              = name;
    myWaterMeterPulse.Task              = WaterMeterPulseTask;
    myWaterMeterPulse.detected          = true;
    myWaterMeterPulse.frequency         = myWMDebounce;     // check every xxx ms if counter changes (~ debounce time)
    myWaterMeterPulse.LoadSettings      = WaterMeterPulseLoadSettings;
    myWaterMeterPulse.SaveSettings      = 0;
    myWaterMeterPulse.LoadMeasures      = 0;
    myWaterMeterPulse.SaveMeasures      = 0;
    myWaterMeterPulse.Values            = WaterMeterPulseValues;
    myWaterMeterPulse.HistoryStats      = 0;

    return myWaterMeterPulse;
}

#endif

