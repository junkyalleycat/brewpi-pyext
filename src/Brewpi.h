#pragma once

#include "config.h"
#include <stdint.h>
#include <algorithm>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/*
   Arduino.h
   */
#define PSTR(x) x  

#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))  

#define memcpy_P memcpy
#define strcpy_P strcpy
#define vsnprintf_P vsnprintf

#define PROGMEM

using std::min;

/*
   Evertying that follows is arduino functions
   */
unsigned long millis();
unsigned long micros();
void delayMicroseconds(unsigned int);
void delay(unsigned long);

class Serial_ {
    public:
        void print(char);
};

extern Serial_ Serial;

/*
   eeprom.h
   */
void    eeprom_update_block (const void *__src, void *__dst, size_t __n);
void    eeprom_update_byte (uint8_t *__p, uint8_t __value);
void    eeprom_read_block (void *__dst, const void *__src, size_t __n);
uint8_t     eeprom_read_byte (const uint8_t *__p); // __ATTR_PURE__;

/*
   Print.h
   */
class Print {
};
