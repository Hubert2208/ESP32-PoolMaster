#ifndef SUPERVISOR_H
#define SUPERVISOR_H

#define SUPERVISOR_I2C_Address 0x07
//#define BUFFER_SIZE 1536 // 1024 Warning Upgrade hangs if not 1536
#define BUFFER_SIZE 2048 // 1024 Warning Upgrade hangs if not 1536
#define LOG_BUFFER_SIZE BUFFER_SIZE
#define PAYLOAD_BUFFER_LENGTH BUFFER_SIZE
#define BUFFER_LENGTH BUFFER_SIZE
//#define PAYLOAD_BUFFER_LENGTH 1200
//#define BUFFER_LENGTH 1536

#define _LHOSTNAME_   16
#define _LSSID_       32
#define _LURL_        128
#define _LVERSION_    12
#define _LTOPIC_      128
#define _LMAC_        20

#define IR_DETECTOR_PIN 36  // GPIO36 for IR detector , no pullup, input only
#define TFT_PowerSaving 45  // TFT will poweroff after xx cycles

const char _DELIMITER_[] = {0xEA,0xEA,0xEA}; // ΩΩΩ

typedef void (*functionvoid) (void*);
typedef void (*functionchar) (char*);

struct ExtensionStruct {
    char*       name;
    int         frequency;
    bool        detected;
    functionvoid Task;
    functionvoid LoadSettings;
    functionvoid SaveSettings;
    functionvoid LoadMeasures;
    functionvoid SaveMeasures;
    functionchar Values;
    functionvoid HistoryStats;
};

typedef ExtensionStruct (*initfunction) (char*, int);
struct ListExtensions
{
    char name[16];          // name of the extension
    initfunction init;      // function to init the extension
    int port;               // Port (GPIO, I2C, etc...)
    UBaseType_t  settingsStackHighWaterMark;
    UBaseType_t  taskStackHighWaterMark;
};

#endif