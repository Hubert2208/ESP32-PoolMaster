#include <Arduino.h>
#include "Config.h"
#include "PoolMaster.h"

static void printUptime(unsigned long ms, const char* prefix) {
  unsigned long sec = ms / 1000;
  unsigned long min = sec / 60;
  unsigned long hrs = min / 60;
  sec %= 60;
  min %= 60;
  Debug.print(DBG_INFO, "[LOGIC] %s Board uptime: %luh %02lum %02lus", prefix, hrs, min, sec);
}

#ifdef KC868_A8