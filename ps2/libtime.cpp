#include <kernel.h>
#include <tamtypes.h>
#include <sifrpc.h>
#include <stdio.h>
#include <debug.h>
#include <libcdvd.h>

#include "libtime.h"

using namespace std;

const int MonthsTable[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

ps2time::ps2time() {
   CdvdClock_t clock;
   
   if ( cdReadClock(&clock) ) {
      clock.year = BCD2DEC(clock.year);
      clock.year += 2000;
      
      setAmtOfLeapYears(UNIX_START_YEAR, clock.year); 
   }
}

ps2time::~ps2time() {
}

bool ps2time::isLeapYear(int Year) {
   if ( ( Year % 4 == 0 && Year % 100 != 0 ) || Year % 400 == 0) {
      return true;
   } else {
      return false;
   }
}

bool ps2time::setAmtOfLeapYears(int startYear, int endYear) {
   int i = 0;
   if ( startYear > endYear ) {
      return false;
   }
   
   for (i = startYear; i <= endYear; i++) {
      if ( isLeapYear(i) ) {
         m_AmtOfLeapYears++;
      }
   }
   
   return true;
}

int ps2time::getSecFromData(time_data* date) {
   int seconds = 0, i;
   
   date->month--;
   
   for (i = 0; i < date->month; ++i) {
      seconds += MonthsTable[i] * 86400;
   }
   
   if ( isLeapYear(date->year) && date->month >= 1 ) {
      seconds += 86400;
   }
   
   seconds += (date->day - 1) * 24 * 60 * 60;
   seconds += date->hour * 60 * 60;
   seconds += date->minute * 60;
   seconds += date->second;
   
   return seconds;
}

int ps2time::getTime() {
   time_data date;
   CdvdClock_t clock;
   
   s32 GMTOffset;
   
   if ( !cdReadClock(&clock) ) {
      return -1;
   }
   
   GMTOffset = configGetTimezone() / 60;
   
   date.year = BCD2DEC(clock.year);
   date.month = BCD2DEC(clock.month);
   date.day = BCD2DEC(clock.day);
   date.hour = BCD2DEC(clock.hour);     // Hour in Japan (GMT + 9)
   date.minute = BCD2DEC(clock.minute);
   date.second = BCD2DEC(clock.second);
   
   // fixing
   date.year += 2000;
   date.hour -= 9 + GMTOffset + configIsDaylightSavingEnabled();
   
   if ( date.hour < 0 ) {
      date.hour += 24;
      if ( --date.day == 0 ) {
         if( --date.month == 0 ) {
            date.month = 11;
            date.hour--;
         }
         
         date.day = MonthsTable[date.month];
         
         if ( date.month == 2 && IS_LEAP_YEAR(date.year) )
            date.day++;
      }
   }
   return ((( date.year - UNIX_START_YEAR) * 365) + getLeapYears() ) * 24 * 60 * 60 + getSecFromData(&date);
}
