#pragma once

#include <stdint.h>
#include <stdbool.h>

#define CLOCKGUSTO_NUM_LEDS 114

typedef enum _clock_word_t
{
    CLOCK_WORD_ES,                  // 0b 00000000 00000000 00000000 00000001   
    CLOCK_WORD_IST,                 // 0b 00000000 00000000 00000000 00000010
    CLOCK_WORD_FUENF_1,             // 0b 00000000 00000000 00000000 00000100
    CLOCK_WORD_ZEHN_1,              // 0b 00000000 00000000 00000000 00001000
    CLOCK_WORD_ZWANZIG,             // 0b 00000000 00000000 00000000 00010000
    CLOCK_WORD_DREIVIERTEL,         // 0b 00000000 00000000 00000000 00100000
    CLOCK_WORD_VIERTEL,             // 0b 00000000 00000000 00000000 01000000
    CLOCK_WORD_VOR,                 // 0b 00000000 00000000 00000000 10000000
    CLOCK_WORD_NACH,                // 0b 00000000 00000000 00000001 00000000
    CLOCK_WORD_HALB,                // 0b 00000000 00000000 00000010 00000000
    CLOCK_WORD_EIN,                 // 0b 00000000 00000000 00000100 00000000
    CLOCK_WORD_EINS,                // 0b 00000000 00000000 00001000 00000000
    CLOCK_WORD_ZWEI,                // 0b 00000000 00000000 00010000 00000000
    CLOCK_WORD_DREI,                // 0b 00000000 00000000 00100000 00000000
    CLOCK_WORD_VIER,                // 0b 00000000 00000000 01000000 00000000
    CLOCK_WORD_FUENF_2,             // 0b 00000000 00000000 10000000 00000000
    CLOCK_WORD_SECHS,               // 0b 00000000 00000001 00000000 00000000
    CLOCK_WORD_SIEBEN,              // 0b 00000000 00000010 00000000 00000000
    CLOCK_WORD_ACHT,                // 0b 00000000 00000100 00000000 00000000
    CLOCK_WORD_NEUN,                // 0b 00000000 00001000 00000000 00000000
    CLOCK_WORD_ZEHN_2,              // 0b 00000000 00010000 00000000 00000000
    CLOCK_WORD_ELF,                 // 0b 00000000 00100000 00000000 00000000
    CLOCK_WORD_ZWOELF,              // 0b 00000000 01000000 00000000 00000000
    CLOCK_WORD_UHR,                 // 0b 00000000 10000000 00000000 00000000

    CLOCK_MINUTE_1,                 // 0b 00000001 00000000 00000000 00000000
    CLOCK_MINUTE_2,                 // 0b 00000010 00000000 00000000 00000000
    CLOCK_MINUTE_3,                 // 0b 00000100 00000000 00000000 00000000
    CLOCK_MINUTE_4,                 // 0b 00001000 00000000 00000000 00000000
    
    CLOCK_WORD_COUNT
} clock_word_t;

static char* clock_word_str[] = {
    "ES", 
    "IST", 
    "FÜNF1", 
    "ZEHN1", 
    "ZWANZIG", 
    "DREIVIERTEL", 
    "VIERTEL", 
    "VOR", 
    "NACH", 
    "HALB",
    "EIN", 
    "EINS", 
    "ZWEI", 
    "DREI", 
    "VIER",
    "FÜNF2",
    "SECHS",
    "SIEBEN",
    "ACHT",
    "NEUN",
    "ZEHN2",
    "ELF",  
    "ZWOELF", 
    "UHR", 
    "min1", 
    "min2", 
    "min3", 
    "min4" 
};

typedef struct _clock_word_boundary_t
{
    uint16_t index;
    uint16_t size;  
} clock_word_boundary_t;

typedef struct _led_t
{
    bool on;
    bool locked;
} led_t;

typedef struct _clock_board_t
{
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint32_t time_mask;
    
    clock_word_boundary_t clock_word_boundary_table[CLOCK_WORD_COUNT];
    led_t leds[CLOCKGUSTO_NUM_LEDS];
} clock_board_t;

/** */
void clockgusto_startup();

/** */
void clockgusto_update();

/** */
void clockgusto_show();

/** */
void clockgusto_reset();





