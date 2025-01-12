/* ========================================================================
   $Project: Clock Gusto $
   $Date: 12.01.2025 $
  $Revision: $
   $Creator: Matthias Stefan $
   $Version: 2.0.0 $
   ======================================================================== */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#define BOARD "esp32"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"

#include "clock_gusto.h"
#include "led_strip_encoder.h"
#include "rtc_ds3231.h"

#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 
#define RMT_LED_STRIP_GPIO_NUM      4

#define CLOCKGUSTO_LED_NUMBERS      114
#define CLOCKGUSTO_CHASE_SPEED_MS   10 

static const char *TAG = "clock gusto";

static uint8_t led_strip_pixels[CLOCKGUSTO_LED_NUMBERS * 3];

void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b)
{
    h %= 360; // h -> [0,360]
    uint32_t rgb_max = v * 2.55f;
    uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

    uint32_t i = h / 60;
    uint32_t diff = h % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i) 
    {
    case 0:
        *r = rgb_max;
        *g = rgb_min + rgb_adj;
        *b = rgb_min;
        break;
    case 1:
        *r = rgb_max - rgb_adj;
        *g = rgb_max;
        *b = rgb_min;
        break;
    case 2:
        *r = rgb_min;
        *g = rgb_max;
        *b = rgb_min + rgb_adj;
        break;
    case 3:
        *r = rgb_min;
        *g = rgb_max - rgb_adj;
        *b = rgb_max;
        break;
    case 4:
        *r = rgb_min + rgb_adj;
        *g = rgb_min;
        *b = rgb_max;
        break;
    default:
        *r = rgb_max;
        *g = rgb_min;
        *b = rgb_max - rgb_adj;
        break;
    }
}

void app_main(void)
{
    uint32_t red = 0;
    uint32_t green = 0;
    uint32_t blue = 0;
    uint16_t hue = 0;
    uint16_t start_rgb = 0;

    ESP_LOGI(TAG, "Create RMT TX channel");
    rmt_channel_handle_t led_chan = NULL;
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT, // select source clock
        .gpio_num = RMT_LED_STRIP_GPIO_NUM,
        .mem_block_symbols = 64, // increase the block size can make the LED less flickering
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
        .trans_queue_depth = 4, // set the number of transactions that can be pending in the background
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));

    ESP_LOGI(TAG, "Install led strip encoder");
    rmt_encoder_handle_t led_encoder = NULL;
    led_strip_encoder_config_t encoder_config = {
        .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
    };
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &led_encoder));

    ESP_LOGI(TAG, "Enable RMT TX channel");
    ESP_ERROR_CHECK(rmt_enable(led_chan));

    ESP_LOGI(TAG, "Start LED rainbow chase");
    rmt_transmit_config_t tx_config = {
        .loop_count = 0, // no transfer loop
    };

    ESP_LOGI(TAG, "Enable RTC DS3231");
    uint8_t sda_pin = 21;
    uint8_t scl_pin = 22;
    ESP_ERROR_CHECK(rtc_ds3231_init(sda_pin, scl_pin, 100000));
    

    const char* compile_time = __TIME__;
    uint8_t hours = (uint8_t)strtol(compile_time, NULL, 10);
    uint8_t minutes = (uint8_t)strtol(compile_time + 3, NULL, 10);
    uint8_t seconds = (uint8_t)strtol(compile_time + 6, NULL, 10);
    ESP_ERROR_CHECK(rtc_ds3231_set_time(hours, minutes, seconds));

    while (true) 
    {
        rtc_ds3231_get_time(&hours, &minutes, &seconds);
        ESP_LOGI(TAG, "%i, %i, %i", hours, minutes, seconds);
        
        for (int i = 0; i < 3; i++)
        {
            for (int j = i; j < CLOCKGUSTO_LED_NUMBERS; j += 3) 
            {
                // Build RGB pixels
                hue = j * 360 / CLOCKGUSTO_LED_NUMBERS + start_rgb;
                led_strip_hsv2rgb(hue, 100, 100, &red, &green, &blue);
                led_strip_pixels[j * 3 + 0] = green;
                led_strip_pixels[j * 3 + 1] = blue;
                led_strip_pixels[j * 3 + 2] = red;
            }
            // Flush RGB values to LEDs
            ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
            ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
            vTaskDelay(pdMS_TO_TICKS(CLOCKGUSTO_CHASE_SPEED_MS));
            memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
            ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
            ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
            vTaskDelay(pdMS_TO_TICKS(CLOCKGUSTO_CHASE_SPEED_MS));
        }
        start_rgb += 60;
    }
}



#if false
#include "esp_err.h"
#include "esp_event.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "esp_wifi.h"
#include "lwip/inet.h"
#include "nvs_flash.h"

#include <ArduinoJson.h>
#include <cstdint>
#include <FastLED.h>
#include <RTClib.h>
#include <TimeLib.h>
#include <WebServer.h>
#include <WiFi.h>

#define internal static
#define global static
#define local static

#define CLOCKGUSTO_DEBUG 0
#define CLOCKGUSTO_INTERNAL 1

#define ArraySize(Array) (sizeof(Array) / sizeof(Array[0]))
#define NUM_LEDS 114
#define CLOCK_BOARD_SETUP_SIZE 10
#define STACK_SIZE 1000

////////////////////////////////////////////////////////////////////////////////
// ENUMS

enum clock_word
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
};

enum log_type
{
    LOG_TYPE_INFO,
    LOG_TYPE_SUCCESS,
    LOG_TYPE_WARNING,
    LOG_TYPE_ERROR,
    LOG_TYPE_CRITICAL,

    LOG_TYPE_COUNT
};

enum log_component
{
    LOG_COMPONENT_CLOCK,
    LOG_COMPONENT_WIFI,
    LOG_COMPONENT_SERVER,
    LOG_COMPONENT_GAME,
    LOG_COMPONENT_ANDROID,
    LOG_COMPONENT_SYSTEM,

    LOG_COMPONENT_COUNT
};

////////////////////////////////////////////////////////////////////////////////
// STRING-TABLES

global char *ClockWordStrings[] = {
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

global char *LogComponentStrings[] = { 
    "CLOCK ", 
    "WIFI  ", 
    "SERVER", 
    "GAME ",
    "ANDROID",
    "SYSTEM" 
    };

global char *LogTypeStrings[] = { 
    "INFO    ", 
    "SUCCESS ", 
    "WARNING ", 
    "ERROR   ", 
    "CRITICAL" 
    };

////////////////////////////////////////////////////////////////////////////////
// STRUCTS

struct simple_time
{
    uint8_t Hour;
    uint8_t Minute;
    uint8_t Second;
};

struct clock_word_boundary
{
    uint16_t Index;
    uint16_t Size;  
};

struct led
{
    CRGB* Color;
    bool On;
    bool IsGuarded;
};

struct clock_board
{
    int32_t Color = CRGB::White;
    clock_word_boundary ClockWordBoundaryTable[CLOCK_WORD_COUNT];
    
    DateTime Time;
    uint32_t TimeMask;

    led Leds[NUM_LEDS];
    CRGB LedColors[NUM_LEDS];
};

// 192.168.178.200 /24
struct home_network
{
    const char *Hostname = "ClockGusto";
    bool IsConnected;
    bool ConnectionLost;

    // Fritzbox 192.168.178.200
    uint8_t IP[4] = { 192, 168, 1, 200 }; 
    uint8_t Gateway[4] = { 192, 168, 1, 1 };
    uint8_t SubnetMask[4] = { 255, 255, 255, 0 };
};

////////////////////////////////////////////////////////////////////////////////
// GLOBALS

global clock_board ClockBoard = {};

global RTC_DS3231 RealTimeClock;

global home_network HomeNetwork = {};
global StaticJsonDocument<250> ServerDocument;
global char ServerBuffer[250];
global WebServer Server(80);

global TaskHandle_t Core0TaskHnd;
global TaskHandle_t Core1TaskHnd;

global bool SnakeIsRunning;

////////////////////////////////////////////////////////////////////////////////
// JSON

internal void CreateJson(const char *Tag, const char *Type, const char *Value, const char *Unit)
{
    ServerDocument.clear();
    ServerDocument["tag"] = Tag;
    ServerDocument["type"] = Type;
    ServerDocument["value"] = Value;
    ServerDocument["unit"] = Unit;
    serializeJson(ServerDocument, ServerBuffer);
} 

////////////////////////////////////////

internal void AddJsonObject(const char *Tag, const char *Type, const char *Value, const char *Unit)
{
    JsonObject Object = ServerDocument.createNestedObject();
    Object["tag"] = Tag;
    Object["type"] = Type;
    Object["value"] = Value; 
    Object["unit"] = Unit; 
}

////////////////////////////////////////////////////////////////////////////////
// UTIL

#define LOG_BEGIN(LogType, LogComponent) LogBegin_(LogType, LogComponent)
#define LOG_END() LogEnd_()

internal void LogBegin_(uint32_t LogType, uint32_t LogComponent)
{
    Serial.printf("[%s][%s][%d:%d:%d] ", LogTypeStrings[LogType], LogComponentStrings[LogComponent], ClockBoard.Time.hour(), ClockBoard.Time.minute(), ClockBoard.Time.second());
}

internal void LogEnd_()
{
    Serial.print("\n");
}

#define LOG_CLOCK_INFO(message)                    \ 
{                                                  \
    LogBegin_(LOG_TYPE_INFO, LOG_COMPONENT_CLOCK); \
    Serial.print(message);                         \
    LogEnd_();                                     \
}

#define LOG_CLOCK_SUCCESS(message)                    \ 
{                                                     \
    LogBegin_(LOG_TYPE_SUCCESS, LOG_COMPONENT_CLOCK); \
    Serial.print(message);                            \
    LogEnd_();                                        \
}

#define LOG_CLOCK_WARNING(message)                    \ 
{                                                     \
    LogBegin_(LOG_TYPE_WARNING, LOG_COMPONENT_CLOCK); \
    Serial.print(message);                            \
    LogEnd_();                                        \
}

#define LOG_CLOCK_ERROR(message)                    \ 
{                                                   \
    LogBegin_(LOG_TYPE_ERROR, LOG_COMPONENT_CLOCK); \
    Serial.print(message);                          \
    LogEnd_();                                      \
}

#define LOG_CLOCK_CRITICAL(message)                    \  
{                                                      \
    LogBegin_(LOG_TYPE_CRITICAL, LOG_COMPONENT_CLOCK); \
    Serial.print(message);                             \
    LogEnd_();                                         \
}

#define LOG_WIFI_INFO(message)                    \ 
{                                                 \
    LogBegin_(LOG_TYPE_INFO, LOG_COMPONENT_WIFI); \
    Serial.print(message);                        \
    LogEnd_();                                    \
}

#define LOG_WIFI_SUCCESS(message)                    \ 
{                                                    \
    LogBegin_(LOG_TYPE_SUCCESS, LOG_COMPONENT_WIFI); \
    Serial.print(message);                           \
    LogEnd_();                                       \
}

#define LOG_WIFI_WARNING(message)                    \ 
{                                                    \
    LogBegin_(LOG_TYPE_WARNING, LOG_COMPONENT_WIFI); \
    Serial.print(message);                           \
    LogEnd_();                                       \
}

#define LOG_WIFI_ERROR(message)                    \ 
{                                                  \
    LogBegin_(LOG_TYPE_ERROR, LOG_COMPONENT_WIFI); \
    Serial.print(message);                         \
    LogEnd_();                                     \
}

#define LOG_WIFI_CRITICAL(message)                    \  
{                                                     \
    LogBegin_(LOG_TYPE_CRITICAL, LOG_COMPONENT_WIFI); \
    Serial.print(message);                            \
    LogEnd_();                                        \
}

// Received Signal Strength Indicator
#define LOG_WIFI_RSSI()                                                   \
{                                                                         \
    wifi_ap_record_t APInfo = {};                                         \ 
    esp_wifi_sta_get_ap_info(&APInfo);                                    \
    LogBegin_(LOG_TYPE_INFO, LOG_COMPONENT_WIFI);                         \
    Serial.printf("Received Signal Strength Indicator: %d", APInfo.rssi); \
    LogEnd_();                                                            \
}

    // ---LOG_COMPONENT_CLOCK,
    // ---LOG_COMPONENT_WIFI
    //LOG_COMPONENT_SERVER,
    //LOG_COMPONENT_GAME,
    //LOG_COMPONENT_ANDROID,
    //LOG_COMPONENT_SYSTEM,

////////////////////////////////////////////////////////////////////////////////
// CLOCK

#if CLOCKGUSTO_INTERNAL
internal void PrintTime()
{
    LOG_BEGIN(LOG_TYPE_INFO, LOG_COMPONENT_CLOCK);
    
    for (uint8_t MaskIndex = 0; MaskIndex < CLOCK_WORD_COUNT; ++MaskIndex)
    {
        uint32_t Mask = 0x0 | 1 << MaskIndex;
        uint32_t MaskResult = Mask & ClockBoard.TimeMask;

        if (MaskResult > 0)
        {
            Serial.printf("%s ", ClockWordStrings[(uint32_t)MaskIndex]);
        }
    }
    
    LOG_END();
}
#else 
internal void PrintTime() {}
#endif

////////////////////////////////////////

internal void SetClockWordTable()
{
    ClockBoard.ClockWordBoundaryTable[CLOCK_WORD_ES]           = { 0, 2 };      // index, size  
    ClockBoard.ClockWordBoundaryTable[CLOCK_WORD_IST]          = { 3, 3 };      
    ClockBoard.ClockWordBoundaryTable[CLOCK_WORD_FUENF_1]      = { 7, 4 };
    ClockBoard.ClockWordBoundaryTable[CLOCK_WORD_ZEHN_1]       = { 18, 4 };
    ClockBoard.ClockWordBoundaryTable[CLOCK_WORD_ZWANZIG]      = { 11, 7 };
    ClockBoard.ClockWordBoundaryTable[CLOCK_WORD_DREIVIERTEL]  = { 22, 11 };
    ClockBoard.ClockWordBoundaryTable[CLOCK_WORD_VIERTEL]      = { 26, 7 };
    ClockBoard.ClockWordBoundaryTable[CLOCK_WORD_VOR]          = { 41, 3 };
    ClockBoard.ClockWordBoundaryTable[CLOCK_WORD_NACH]         = { 33, 4 };
    ClockBoard.ClockWordBoundaryTable[CLOCK_WORD_HALB]         = { 44, 4 };
    ClockBoard.ClockWordBoundaryTable[CLOCK_WORD_EIN]          = { 63, 3 };
    ClockBoard.ClockWordBoundaryTable[CLOCK_WORD_EINS]         = { 62, 4 };
    ClockBoard.ClockWordBoundaryTable[CLOCK_WORD_ZWEI]         = { 55, 4 };
    ClockBoard.ClockWordBoundaryTable[CLOCK_WORD_DREI]         = { 66, 4 };
    ClockBoard.ClockWordBoundaryTable[CLOCK_WORD_VIER]         = { 73, 4 };
    ClockBoard.ClockWordBoundaryTable[CLOCK_WORD_FUENF_2]      = { 51, 4};
    ClockBoard.ClockWordBoundaryTable[CLOCK_WORD_SECHS]        = { 83, 5 };
    ClockBoard.ClockWordBoundaryTable[CLOCK_WORD_SIEBEN]       = { 88, 6 };
    ClockBoard.ClockWordBoundaryTable[CLOCK_WORD_ACHT]         = { 77, 4 };
    ClockBoard.ClockWordBoundaryTable[CLOCK_WORD_NEUN]         = { 103, 4 };
    ClockBoard.ClockWordBoundaryTable[CLOCK_WORD_ZEHN_2]       = { 106, 4 };
    ClockBoard.ClockWordBoundaryTable[CLOCK_WORD_ELF]          = { 49, 3 };
    ClockBoard.ClockWordBoundaryTable[CLOCK_WORD_ZWOELF]       = { 94, 5 };
    ClockBoard.ClockWordBoundaryTable[CLOCK_WORD_UHR]          = { 99, 3 };

    ClockBoard.ClockWordBoundaryTable[CLOCK_MINUTE_1]          = { 113, 1 };
    ClockBoard.ClockWordBoundaryTable[CLOCK_MINUTE_2]          = { 112, 1 };
    ClockBoard.ClockWordBoundaryTable[CLOCK_MINUTE_3]          = { 111, 1 };
    ClockBoard.ClockWordBoundaryTable[CLOCK_MINUTE_4]          = { 110, 1 };
}

////////////////////////////////////////

internal void SetGuard(clock_word ClockWord, bool Value)
{
    clock_word_boundary Boundary = ClockBoard.ClockWordBoundaryTable[ClockWord];
    for (uint16_t LedIndex = Boundary.Index; LedIndex < (Boundary.Index + Boundary.Size); ++LedIndex)
    {
        led *Led = &ClockBoard.Leds[LedIndex];
        Led->IsGuarded = Value;
    }
}

////////////////////////////////////////

internal void SetGuard(uint16_t Index, bool Value)
{
    led *Led = &ClockBoard.Leds[Index];
    Led->IsGuarded = Value;
}

////////////////////////////////////////

internal void SetAllGuards(bool Value)
{
    for (uint16_t LedIndex = 0; LedIndex < NUM_LEDS; ++LedIndex)
    {
        led *Led = &ClockBoard.Leds[LedIndex];
        Led->IsGuarded = Value;
    }
}

////////////////////////////////////////

internal void TurnOn(clock_word ClockWord, int Color = CRGB::White)
{
    clock_word_boundary Boundary = ClockBoard.ClockWordBoundaryTable[ClockWord];
    for (uint16_t LedIndex = Boundary.Index; LedIndex < (Boundary.Index + Boundary.Size); ++LedIndex)
    {
        // NOTE: Check overlapping letter 51 & 106
        led *Led = &ClockBoard.Leds[LedIndex];
        if (!Led->IsGuarded)
        {
            *Led->Color = Color;
            Led->On = true;
        }
    }
    FastLED.show();
}
////////////////////////////////////////

internal void TurnOn(uint16_t Index, int Color = CRGB::White)
{
    led *Led = &ClockBoard.Leds[Index];
    if (!Led->IsGuarded)
    {
        *Led->Color = Color;
        Led->On = true;
    }
    FastLED.show();
}

////////////////////////////////////////

internal void TurnOff(clock_word ClockWord)
{
    clock_word_boundary Boundary = ClockBoard.ClockWordBoundaryTable[ClockWord];
    for (uint16_t LedIndex = Boundary.Index; LedIndex < (Boundary.Index + Boundary.Size); ++LedIndex)
    {
        led *Led = &ClockBoard.Leds[LedIndex];
        if (!Led->IsGuarded)
        {
            *Led->Color = CRGB::Black;
            Led->On = false;
        }
    }
    FastLED.show();
}

////////////////////////////////////////

internal void TurnOff(uint16_t Index)
{
    led *Led = &ClockBoard.Leds[Index];
    if (!Led->IsGuarded)
    {
        *Led->Color = CRGB::Black;
        Led->On = false;
    }
    FastLED.show();
}

////////////////////////////////////////

internal void TurnAllOff()
{
    for (uint16_t LedIndex = 0; LedIndex < NUM_LEDS; ++LedIndex)
    {
        led *Led = &ClockBoard.Leds[LedIndex];
        if (!Led->IsGuarded)
        {
            *Led->Color = CRGB::Black;
            Led->On = false;
        }
    }
    FastLED.show();
}

////////////////////////////////////////

internal void SetLedGuard(uint16_t LedIndex, bool Value)
{
    led *Led = &ClockBoard.Leds[LedIndex];
    Led->IsGuarded = Value;
}

////////////////////////////////////////

internal void SetClockBoardTimeMask()
{
    uint8_t Hour = ClockBoard.Time.hour();
    uint8_t Minute = ClockBoard.Time.minute();
    uint8_t Second = ClockBoard.Time.second();
    
    if (!(Minute < 15 || (20 <= Minute && Minute < 25))) 
    {
        Hour = (Hour + 1) % 24; 
    }

    uint32_t TimeMask = ClockBoard.TimeMask;
    ClockBoard.TimeMask = 0x0;

    switch (Hour)
    {
        case 0:
        case 12: 
        {
            ClockBoard.TimeMask |= 1 << CLOCK_WORD_ZWOELF;
        } break;
        case 1:
        case 13:
        {
            if (Minute < 5)
            {
                ClockBoard.TimeMask |= 1 << CLOCK_WORD_EIN;
            }
            else
            {
                ClockBoard.TimeMask |= 1 << CLOCK_WORD_EINS;
            }
        } break;
        case 2:
        case 14:
        {
            ClockBoard.TimeMask |= 1 << CLOCK_WORD_ZWEI;
        } break;
        case 3:
        case 15:
        {
            ClockBoard.TimeMask |= 1 << CLOCK_WORD_DREI;
        } break;
        case 4:
        case 16:
        {
            ClockBoard.TimeMask |= 1 << CLOCK_WORD_VIER;
        } break;
        case 5:
        case 17:
        {
            ClockBoard.TimeMask |= 1 << CLOCK_WORD_FUENF_2;
        } break;
        case 6:
        case 18:
        {
            ClockBoard.TimeMask |= 1 << CLOCK_WORD_SECHS;
        } break;
        case 7:
        case 19:
        {
            ClockBoard.TimeMask |= 1 << CLOCK_WORD_SIEBEN;
        } break;
        case 8:
        case 20:
        {
            ClockBoard.TimeMask |= 1 << CLOCK_WORD_ACHT;
        } break;
        case 9:
        case 21:
        {
            ClockBoard.TimeMask |= 1 << CLOCK_WORD_NEUN;
        } break;
        case 10:
        case 22:
        {
            ClockBoard.TimeMask |= 1 << CLOCK_WORD_ZEHN_2;
        } break;
        case 11:
        case 23:
        {
            ClockBoard.TimeMask |= 1 << CLOCK_WORD_ELF;
        } break;
    }

    if (0 <= Minute && Minute < 5) 
    {
        ClockBoard.TimeMask |= 1 << CLOCK_WORD_ES;
        ClockBoard.TimeMask |= 1 << CLOCK_WORD_IST;
        ClockBoard.TimeMask |= 1 << CLOCK_WORD_UHR;
    }
    else if (5 <= Minute && Minute < 10)
    {
        ClockBoard.TimeMask |= 1 << CLOCK_WORD_FUENF_1;
        ClockBoard.TimeMask |= 1 << CLOCK_WORD_NACH;
    }
    else if (10 <= Minute && Minute < 15)
    {
        ClockBoard.TimeMask |= 1 << CLOCK_WORD_ZEHN_1;
        ClockBoard.TimeMask |= 1 << CLOCK_WORD_NACH;
    }
    else if (15 <= Minute && Minute < 20)
    {
        ClockBoard.TimeMask |= 1 << CLOCK_WORD_VIERTEL;
    }
    else if (20 <= Minute && Minute < 25)
    {
        ClockBoard.TimeMask |= 1 << CLOCK_WORD_ZWANZIG;
        ClockBoard.TimeMask |= 1 << CLOCK_WORD_NACH;
    }
    else if (25 <= Minute && Minute < 30)
    {
        ClockBoard.TimeMask |= 1 << CLOCK_WORD_FUENF_1;
        ClockBoard.TimeMask |= 1 << CLOCK_WORD_VOR;
        ClockBoard.TimeMask |= 1 << CLOCK_WORD_HALB;
    }
    else if (30 <= Minute && Minute < 35)
    {
        ClockBoard.TimeMask |= 1 << CLOCK_WORD_ES;
        ClockBoard.TimeMask |= 1 << CLOCK_WORD_IST;
        ClockBoard.TimeMask |= 1 << CLOCK_WORD_HALB;
    }
    else if (35 <= Minute && Minute < 40)
    {
        ClockBoard.TimeMask |= 1 << CLOCK_WORD_FUENF_1;
        ClockBoard.TimeMask |= 1 << CLOCK_WORD_NACH;
        ClockBoard.TimeMask |= 1 << CLOCK_WORD_HALB;
    }
    else if (40 <= Minute && Minute < 45)
    {
        ClockBoard.TimeMask |= 1 << CLOCK_WORD_ZWANZIG;
        ClockBoard.TimeMask |= 1 << CLOCK_WORD_VOR;
    }
    else if (45 <= Minute && Minute < 50)
    {
        ClockBoard.TimeMask |= 1 << CLOCK_WORD_VIERTEL;
        ClockBoard.TimeMask |= 1 << CLOCK_WORD_DREIVIERTEL;
    }
    else if (50 <= Minute && Minute < 55)
    {
        ClockBoard.TimeMask |= 1 << CLOCK_WORD_ZEHN_1;
        ClockBoard.TimeMask |= 1 << CLOCK_WORD_VOR;
    }
    else // 55 <= Minute && Minute 59
    {
        ClockBoard.TimeMask |= 1 << CLOCK_WORD_FUENF_1;
        ClockBoard.TimeMask |= 1 << CLOCK_WORD_VOR;
    }    

    if (Minute % 5 == 1)
    {
        ClockBoard.TimeMask |= 1 << CLOCK_MINUTE_1;
    }
    else if (Minute % 5 == 2)
    {
        ClockBoard.TimeMask |= 1 << CLOCK_MINUTE_1;
        ClockBoard.TimeMask |= 1 << CLOCK_MINUTE_2;
    }
    else if (Minute % 5 == 3)
    {
        ClockBoard.TimeMask |= 1 << CLOCK_MINUTE_1;
        ClockBoard.TimeMask |= 1 << CLOCK_MINUTE_2;
        ClockBoard.TimeMask |= 1 << CLOCK_MINUTE_3;
    }
    else if (Minute % 5 == 4)
    {
        ClockBoard.TimeMask |= 1 << CLOCK_MINUTE_1;
        ClockBoard.TimeMask |= 1 << CLOCK_MINUTE_2;
        ClockBoard.TimeMask |= 1 << CLOCK_MINUTE_3;
        ClockBoard.TimeMask |= 1 << CLOCK_MINUTE_4;
    }
    else
    {
        ClockBoard.TimeMask |= 0 << CLOCK_MINUTE_1;
        ClockBoard.TimeMask |= 0 << CLOCK_MINUTE_2;
        ClockBoard.TimeMask |= 0 << CLOCK_MINUTE_3;
        ClockBoard.TimeMask |= 0 << CLOCK_MINUTE_4;
    }

    if (TimeMask != ClockBoard.TimeMask)
    {
        PrintTime();
    }
}

////////////////////////////////////////

internal void SetClockBoardTime() 
{
    if (HomeNetwork.IsConnected && HomeNetwork.ConnectionLost)
    {
        ClockBoard.Color = 0xFF0000;
    }

    SetAllGuards(false);
    for (uint8_t MaskIndex = 0; MaskIndex < CLOCK_WORD_COUNT; ++MaskIndex)
    {
        uint32_t Mask = 0x0 | 1 << MaskIndex;
        uint32_t MaskResult = Mask & ClockBoard.TimeMask;

        if (MaskResult > 0)
        {
            TurnOn((clock_word)MaskIndex, ClockBoard.Color);
            SetGuard((clock_word)MaskIndex, true);
        }
        else
        {
            TurnOff((clock_word)MaskIndex);
        }
    }
    SetAllGuards(false);
}

////////////////////////////////////////

internal bool IsTimeWithinInterval(simple_time TimeToCheck, simple_time IntervalStart, simple_time IntervalEnd)
{
    bool SpansMidnight = (IntervalEnd.Hour < IntervalStart.Hour) || 
                         (IntervalEnd.Hour == IntervalStart.Hour && IntervalEnd.Minute < IntervalStart.Minute);

    if (SpansMidnight)
    {
        // Time interval spans midnight
        return (TimeToCheck.Hour > IntervalStart.Hour || 
                (TimeToCheck.Hour == IntervalStart.Hour && TimeToCheck.Minute >= IntervalStart.Minute)) ||
               (TimeToCheck.Hour < IntervalEnd.Hour || 
                (TimeToCheck.Hour == IntervalEnd.Hour && TimeToCheck.Minute <= IntervalEnd.Minute));
    } 
    else 
    {
        // Time interval does not span midnight
        return (TimeToCheck.Hour > IntervalStart.Hour || 
                (TimeToCheck.Hour == IntervalStart.Hour && TimeToCheck.Minute >= IntervalStart.Minute)) &&
               (TimeToCheck.Hour < IntervalEnd.Hour || 
                (TimeToCheck.Hour == IntervalEnd.Hour && TimeToCheck.Minute <= IntervalEnd.Minute));
    }
}

////////////////////////////////////////////////////////////////////////////////
// WIFI

internal void WiFiEventHandler(void* Arg, 
                               esp_event_base_t EventBase,
                               int32_t EventId, 
                               void* EventData)
{
    if (EventBase == WIFI_EVENT && 
        EventId == WIFI_EVENT_STA_CONNECTED) 
    {
        esp_netif_ip_info_t IPInfo;
        esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &IPInfo);
        char Message[150];
        snprintf(Message, sizeof(Message), "Connected to AP successfully. IP is: " IPSTR, IP2STR(&IPInfo.ip));
        LOG_WIFI_INFO(Message);
        HomeNetwork.IsConnected = true;
        HomeNetwork.ConnectionLost = false;
    } 
    else if (EventBase == WIFI_EVENT && 
             EventId == WIFI_EVENT_STA_DISCONNECTED) 
    {
        LOG_WIFI_WARNING("Disconnected. Reconnecting...");
        esp_wifi_connect();
        HomeNetwork.IsConnected = false;
        HomeNetwork.ConnectionLost = true;
    }
}

////////////////////////////////////////

internal void TryConnectToWiFi()
{
    uint8_t Attempts = 3;
    LOG_WIFI_INFO("Starting connection to the router...");
    while (esp_wifi_connect() != ESP_OK && 
           Attempts > 0) 
    {
        LOG_WIFI_INFO("Retrying connection...");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        Attempts--;
    }

    if (Attempts == 0) 
    {
        LOG_WIFI_ERROR("Failed to connect to AP");
        HomeNetwork.IsConnected = false;
    } 
    else
    {
        LOG_WIFI_SUCCESS("Connected to AP successfully.");
        HomeNetwork.IsConnected = true;
    }
}

////////////////////////////////////////

internal void InitializeWiFi()
{
    LOG_WIFI_INFO("Connecting with WiFi...");

    esp_err_t Error = nvs_flash_init();
    if (Error == ESP_ERR_NVS_NO_FREE_PAGES || 
        Error == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        Error = nvs_flash_init();
    }
    ESP_ERROR_CHECK(Error);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t *WifiSta = esp_netif_create_default_wifi_sta();
    esp_netif_set_hostname(WifiSta, HomeNetwork.Hostname);

    esp_netif_ip_info_t IPInfo;

    esp_netif_set_ip4_addr(&IPInfo.ip, HomeNetwork.IP[0], HomeNetwork.IP[1], HomeNetwork.IP[2], HomeNetwork.IP[3]);
    esp_netif_set_ip4_addr(&IPInfo.gw, HomeNetwork.Gateway[0], HomeNetwork.Gateway[1], HomeNetwork.Gateway[2], HomeNetwork.Gateway[3]);
    esp_netif_set_ip4_addr(&IPInfo.netmask, HomeNetwork.SubnetMask[0], HomeNetwork.SubnetMask[1], HomeNetwork.SubnetMask[2], HomeNetwork.SubnetMask[3]);

    esp_netif_dhcpc_stop(WifiSta); // DHCP Client stoppen
    esp_netif_set_ip_info(WifiSta, &IPInfo);

    wifi_init_config_t Config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&Config));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WiFiEventHandler, NULL));

    wifi_config_t WifiConfigs[2] = {
        {
            .sta = {
                .ssid = "FRITZ!Box 7590 HG",
                .password = "57345786411858633822",
            },
        },
        {
            .sta = {
                .ssid = "M.M Homespot 2.4G",
                .password = "OWTi87eJ.",
            },
        }
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &WifiConfigs[1]));
    ESP_ERROR_CHECK(esp_wifi_start());

    TryConnectToWiFi();
}

////////////////////////////////////////////////////////////////////////////////
// SERVER

// HTTP_POST, HTTP_PUT, HTTP_PATCH, HTTP_DELETE
internal void InitializeServer()
{
    // Time
    Server.on("/time/unix", GET_UnixTime);
    Server.on("/time/millenium", GET_MilleniumTime);
    Server.on("/time/set", HTTP_PATCH, PATCH_Time);
    
    Server.begin(); 
}

////////////////////////////////////////

internal void GET_UnixTime()
{
    LOG_BEGIN(LOG_TYPE_INFO, LOG_COMPONENT_SERVER);
    Serial.print("REST GET: /time/unix");
    LOG_END();

    LOG_BEGIN(LOG_TYPE_INFO, LOG_COMPONENT_SERVER);
    String UnixTime_ = String(ClockBoard.Time.unixtime()); 
    Serial.printf("%s", UnixTime_);
    LOG_END();

    CreateJson("time/unix", "uint32_t", UnixTime_.c_str(), "sec");
    Server.send(200, "/time/json", ServerBuffer);
}

////////////////////////////////////////

internal void GET_MilleniumTime()
{
    LOG_BEGIN(LOG_TYPE_INFO, LOG_COMPONENT_SERVER);
    Serial.print("REST GET: /time/millenium");
    LOG_END();

    String MilleniumTime_ = String(ClockBoard.Time.secondstime()); 

    CreateJson("time/millenium", "uint32_t", MilleniumTime_.c_str(), "sec");
    Server.send(200, "/time/json", ServerBuffer);
}

////////////////////////////////////////

internal void PATCH_Time()
{
    LOG_BEGIN(LOG_TYPE_INFO, LOG_COMPONENT_SERVER);
    Serial.print("REST PATCH: /time/set ");

    String Body = Server.arg("plain");
    deserializeJson(ServerDocument, Body);

    uint32_t Year = ServerDocument["year"];
    uint32_t Month = ServerDocument["month"];
    uint32_t Day = ServerDocument["day"];
    uint32_t Hour = ServerDocument["hour"];
    uint32_t Minute = ServerDocument["minute"];
    uint32_t Second = ServerDocument["second"];

    DateTime NewTime = DateTime(Year, Month, Day, Hour, Minute, Second);
    
    RealTimeClock.adjust(NewTime);

    Serial.printf("Set time: %d-%d-%d %d:%d:%d", NewTime.year(), NewTime.month(), NewTime.day(), NewTime.hour(), NewTime.minute(), NewTime.second());

    LOG_END();
    Server.send(200, "application/json", "{}");
}

////////////////////////////////////////////////////////////////////////////////
// MAIN

#if CLOCKGUSTO_DEBUG
void debug_loop()
{
    for (int i = 0; i < NUM_LEDS; ++i)
    {
        TurnOn(i);
    }
    delay(1000);        
    TurnAllOff();

    for (int i = 0; i < NUM_LEDS; ++i)
    {
        TurnOn(i);
        delay(33);        
    }

    delay(1000);
    TurnAllOff();

    TurnOn(CLOCK_WORD_ES, 0xFF0000);
    TurnOn(CLOCK_WORD_IST, 0x00FF00);
    TurnOn(CLOCK_WORD_FUENF_1, 0x0000FF);        
    TurnOn(CLOCK_WORD_ZEHN_1, 0xFFFF00);
    TurnOn(CLOCK_WORD_ZWANZIG, 0xFF00FF);
    TurnOn(CLOCK_WORD_DREIVIERTEL, 0x123456);
    TurnOn(CLOCK_WORD_VIERTEL, 0xFEDCBA);
    TurnOn(CLOCK_WORD_VOR, 0xCD5C5C);
    TurnOn(CLOCK_WORD_NACH, 0xF08080);
    TurnOn(CLOCK_WORD_HALB, 0xDC143C);
    TurnOn(CLOCK_WORD_FUENF_2, 0xB22222);
    TurnOn(CLOCK_WORD_EINS, 0xBD2478);
    TurnOn(CLOCK_WORD_ZWEI, 0x5C0775);
    TurnOn(CLOCK_WORD_DREI, 0x614624);
    TurnOn(CLOCK_WORD_VIER, 0x71FF52);
    TurnOn(CLOCK_WORD_SECHS, 0x6A32D1);
    TurnOn(CLOCK_WORD_ACHT, 0xF757F7);
    TurnOn(CLOCK_WORD_SIEBEN, 0xFCBA03);
    TurnOn(CLOCK_WORD_ZWOELF, 0x72C959);
    TurnOn(CLOCK_WORD_ZEHN_2, 0x174BA0);
    TurnOn(CLOCK_WORD_UHR, 0x7BD7C1);
    delay(1000);
    
    TurnAllOff();
} 

DateTime debug_get_time()
{
    static DateTime Result(2024, 8, 16, 0, 0, 0);
    Result = Result + TimeSpan(0, 0, 1, 0);
    return Result;
}
#endif

////////////////////////////////////////

internal void CoreTask0(void *Parameters)
{
    for (;;)
    {
        LOG_BEGIN(LOG_TYPE_INFO, LOG_COMPONENT_SYSTEM);
        Serial.print("CoreTask0 runs on Core: "); 
        Serial.print(xPortGetCoreID()); 
        LOG_END();
    }
}

////////////////////////////////////////

internal void CoreTask1(void *Parameters)
{
    for (;;)
    {
        LOG_BEGIN(LOG_TYPE_INFO, LOG_COMPONENT_SYSTEM);
        Serial.print("CoreTask1 runs on Core: "); 
        Serial.print(xPortGetCoreID()); 
        LOG_END();
    }
}

////////////////////////////////////////

void setup() 
{
    Serial.begin(115200);

    // Setup power management
    esp_pm_config_t CPUConfig;
    CPUConfig.max_freq_mhz = 80;   // Set maximum frequency to 80 MHz to minimize power usage
    CPUConfig.min_freq_mhz = 10;   // Set minimum frequency to 10 MHz for minimal CPU usage

    esp_err_t Error = esp_pm_configure(&CPUConfig);
    if (Error == ESP_OK)
    {
        LOG_BEGIN(LOG_TYPE_INFO, LOG_COMPONENT_SYSTEM);
        Serial.println("CPU adjustment successful!");
        LOG_END();
    }
    else
    {
        LOG_BEGIN(LOG_TYPE_CRITICAL, LOG_COMPONENT_SYSTEM);
        Serial.print("CPU adjustment failed! Error code: ");
        Serial.println(Error);
        LOG_END();
    }

    RealTimeClock.begin();
    RealTimeClock.adjust(DateTime(F(__DATE__), F(__TIME__)));
    SetClockWordTable(); 

    for (uint16_t LedIndex = 0; LedIndex < NUM_LEDS; ++LedIndex)
    {
        ClockBoard.Leds[LedIndex].Color = &ClockBoard.LedColors[LedIndex];
        ClockBoard.Leds[LedIndex].On = false;
        ClockBoard.Leds[LedIndex].IsGuarded = false;
    }
    pinMode(DATA_PIN, OUTPUT);
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(ClockBoard.LedColors, NUM_LEDS);

    InitializeWiFi();
    InitializeServer();

#if false
    xTaskCreatePinnedToCore(CoreTask0, "CPU_0", STACK_SIZE , NULL, 1, &Core0TaskHnd, 0);
    xTaskCreatePinnedToCore(CoreTask1, "CPU_1", STACK_SIZE , NULL, 1, &Core1TaskHnd, 1);
#endif
}

////////////////////////////////////////

void loop() 
{
    if (HomeNetwork.IsConnected)
    {
        Server.handleClient();
    }

    DateTime CurrentTime = RealTimeClock.now();
#if CLOCKGUSTO_DEBUG
    
    CurrentTime = debug_get_time();
#endif

    uint8_t CurrentHour = CurrentTime.hour();
    uint8_t CurrentMinute = CurrentTime.minute();
    uint8_t CurrentSecond = CurrentTime.second();
    
    if (IsTimeWithinInterval({ CurrentHour, CurrentMinute, CurrentSecond }, 
                             { 22, 0, 0 },
                             { 7, 0, 0 }))
    {
        ClockBoard.Color = 0x000926;
    }
    else
    {
        ClockBoard.Color = CRGB::White;
    }

    bool IsFlipClockBoardNeeded = (CurrentHour != ClockBoard.Time.hour() || CurrentMinute != ClockBoard.Time.minute());
    // Check if the LED state needs to be changed
    if (IsFlipClockBoardNeeded)
    {
        // Update the Clockboard with the current time
        ClockBoard.Time = CurrentTime;
        SetClockBoardTimeMask();
        SetClockBoardTime();
    }

    if (IsFlipClockBoardNeeded)
    {
        LOG_WIFI_RSSI();
    }
}

#endif
