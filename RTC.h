#ifndef _RTC_HEADER_H_
#define _RTC_HEADER_H_

#include "ps2/libtime.h"

typedef struct _RTC_Data {
  int RAMBank;
  int ClockLatch;
  int ClockRegister;
  int Seconds;
  int Minutes;
  int Hours;
  int Days;
  int Control;
  int LSeconds;
  int LMinutes;
  int LHours;
  int LDays;
  int LControl;
  int LastTime;
} RTC_Data;

extern RTC_Data RTC;
extern ps2time timer;

void UpdateClockData(RTC_Data* RTC);

#endif
