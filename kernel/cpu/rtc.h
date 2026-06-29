#ifndef RTC_H
#define RTC_H

#include "kernel.h"

typedef struct {
    uint8_t  seconds;
    uint8_t  minutes;
    uint8_t  hours;
    uint8_t  day;
    uint8_t  month;
    uint16_t year;
} RTCTime;

void rtc_read(RTCTime* t);
void rtc_get_time_str(char* buf);   /* writes "HH:MM:SS\0" */
void rtc_get_date_str(char* buf);   /* writes "DD.MM.YYYY\0" */

#endif
