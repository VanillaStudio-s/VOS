#include "kernel.h"
#include "cpu/rtc.h"

#define CMOS_ADDR  0x70
#define CMOS_DATA  0x71

static uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_ADDR, (uint8_t)(reg & 0x7F));   /* keep NMI enabled */
    return inb(CMOS_DATA);
}

static uint8_t bcd2bin(uint8_t v) {
    return (uint8_t)((v & 0x0F) + (v >> 4) * 10);
}

static int rtc_busy(void) {
    return cmos_read(0x0A) & 0x80;
}

void rtc_read(RTCTime* t) {
    /* Wait until no update in progress */
    while (rtc_busy());

    uint8_t sec, min, hr, day, mon, yr, sec2;

    /* Read twice to detect mid-update corruption */
    do {
        sec = cmos_read(0x00);
        min = cmos_read(0x02);
        hr  = cmos_read(0x04);
        day = cmos_read(0x07);
        mon = cmos_read(0x08);
        yr  = cmos_read(0x09);
        while (rtc_busy());
        sec2 = cmos_read(0x00);
    } while (sec != sec2);

    uint8_t regB = cmos_read(0x0B);

    /* Convert BCD → binary unless status reg B says binary mode */
    if (!(regB & 0x04)) {
        sec = bcd2bin(sec);
        min = bcd2bin(min);
        hr  = (uint8_t)(bcd2bin((uint8_t)(hr & 0x7F)) | (hr & 0x80));
        day = bcd2bin(day);
        mon = bcd2bin(mon);
        yr  = bcd2bin(yr);
    }

    /* Convert 12-hour PM to 24-hour if needed */
    if (!(regB & 0x02) && (hr & 0x80))
        hr = (uint8_t)(((hr & 0x7F) % 12) + 12);

    t->seconds = sec;
    t->minutes = min;
    t->hours   = hr;
    t->day     = day;
    t->month   = mon;
    t->year    = (uint16_t)(2000 + yr);
}

static void put2(char* buf, uint8_t v) {
    buf[0] = (char)('0' + v / 10);
    buf[1] = (char)('0' + v % 10);
}

void rtc_get_time_str(char* buf) {
    RTCTime t;
    rtc_read(&t);
    put2(buf,     t.hours);   buf[2] = ':';
    put2(buf + 3, t.minutes); buf[5] = ':';
    put2(buf + 6, t.seconds); buf[8] = '\0';
}

void rtc_get_date_str(char* buf) {
    RTCTime t;
    rtc_read(&t);
    put2(buf,     t.day);   buf[2] = '.';
    put2(buf + 3, t.month); buf[5] = '.';
    uint16_t y = t.year;
    buf[6]  = (char)('0' + y / 1000);
    buf[7]  = (char)('0' + y / 100 % 10);
    buf[8]  = (char)('0' + y / 10  % 10);
    buf[9]  = (char)('0' + y       % 10);
    buf[10] = '\0';
}
