#ifndef _LIB_TIME_H_
#define _LIB_TIME_H_

#include <libcdvd.h>
#include <osd_config.h>

#include "libtime.h"

#define BCD2DEC(bcd)     (( ( ((bcd) >> 4) & 0x0F) * 10) + ( (bcd) & 0x0F) )
#define IS_LEAP_YEAR(Y)  (( (Y) > 0) && !( (Y) % 4 ) && ( ((Y)%100) || !((Y)%400) ) )

#define UNIX_START_YEAR  1970

extern const int MonthsTable[];

typedef struct _time_data {
   int year;
   int month;
   int day;
   int hour;
   int minute;
   int second;
   bool is_leap_year;
} time_data;

class ps2time {
   private:
      int m_AmtOfLeapYears;
      
   public:
      bool isLeapYear(int Year);
      bool setAmtOfLeapYears(int startYear, int endYear);
      int getLeapYears() { 
         return m_AmtOfLeapYears; 
      }
      int getTime();
      int getSecFromData(time_data* date);
      
      // not implemented yet
      time_data* getLocaltime(time_data* date);
      
      ps2time();
     ~ps2time();
};

#endif
