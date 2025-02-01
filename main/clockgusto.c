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
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"

#include "clockgusto.h"
#include "clockgusto_wifi.h"
#include "led_strip_encoder.h"
#include "rtc_ds3231.h"

#define BOARD "esp32"
#define TAG "clock gusto"
#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 
#define RMT_LED_STRIP_GPIO_NUM      4
#define CLOCKGUSTO_CHASE_SPEED_MS   10

typedef struct _clockgusto_state_t
{
    clock_board_t clock_board;
    uint8_t led_strip_pixels[CLOCKGUSTO_NUM_LEDS * CLOCKGUSTO_BYTES_PER_LED];

    rmt_transmit_config_t tx_config;
    rmt_channel_handle_t led_chan;
    rmt_encoder_handle_t led_encoder;
} clockgusto_state_t;

static clockgusto_state_t state;

static void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t* r, uint32_t* g, uint32_t* b);
static void clockgusto_set_board_time_mask();

void app_main(void)
{
    ESP_LOGI(TAG, "Create RMT TX channel");
    state.led_chan = NULL;
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT, // select source clock
        .gpio_num = RMT_LED_STRIP_GPIO_NUM,
        .mem_block_symbols = 64, // increase the block size can make the LED less flickering
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
        .trans_queue_depth = 4, // set the number of transactions that can be pending in the background
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &state.led_chan));

    ESP_LOGI(TAG, "Install led strip encoder");
    state.led_encoder = NULL;
    led_strip_encoder_config_t encoder_config = {
        .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
    };
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &state.led_encoder));

    ESP_LOGI(TAG, "Enable RMT TX channel");
    ESP_ERROR_CHECK(rmt_enable(state.led_chan));

    ESP_LOGI(TAG, "Start LED rainbow chase");
    state.tx_config = (rmt_transmit_config_t){
        .loop_count = 0,
    };

    ESP_LOGI(TAG, "Enable RTC DS3231");
    uint8_t sda_pin = 21;
    uint8_t scl_pin = 22;
    ESP_ERROR_CHECK(rtc_ds3231_init(sda_pin, scl_pin, 100000));
    
    ESP_LOGI(TAG, "Startup clockgusto");
    clockgusto_startup();
   
#if true
    const char* compile_time = __TIME__;
    uint8_t hours = (uint8_t)strtol(compile_time, NULL, 10);
    uint8_t minutes = (uint8_t)strtol(compile_time + 3, NULL, 10);
    uint8_t seconds = (uint8_t)strtol(compile_time + 6, NULL, 10);
    ESP_ERROR_CHECK(rtc_ds3231_set_time(hours, minutes, seconds));
#else
    int8_t hours = 7;
    int8_t minutes = 13;
    int8_t seconds = 00;
    ESP_ERROR_CHECK(rtc_ds3231_set_time(hours, minutes, seconds));
#endif

    state.clock_board.hours = hours;
    state.clock_board.minutes = minutes;
    state.clock_board.seconds = seconds;
    ESP_LOGI(TAG, "Start clock");
    while (true) 
    {
        //ESP_LOGI(TAG, "Update clock");
        clockgusto_update();

        //ESP_LOGI(TAG, "Show clock");
        clockgusto_show();

        //ESP_LOGI(TAG, "Reset clock");
        clockgusto_reset();
    }
}

void clockgusto_startup()
{
    clock_board_t* clock_board = &state.clock_board;

    clock_board->clock_word_boundary_table[CLOCK_WORD_ES] = (clock_word_boundary_t){  
        .index = 0,
        .size = 2
    };
    clock_board->clock_word_boundary_table[CLOCK_WORD_IST] = (clock_word_boundary_t){ 
        .index = 3, 
        .size = 3 
    };      
    clock_board->clock_word_boundary_table[CLOCK_WORD_FUENF_1] = (clock_word_boundary_t){ 
        .index = 7, 
        .size = 4 
    };
    clock_board->clock_word_boundary_table[CLOCK_WORD_ZEHN_1] = (clock_word_boundary_t){ 
        .index = 18, 
        .size = 4 
    };
    clock_board->clock_word_boundary_table[CLOCK_WORD_ZWANZIG] = (clock_word_boundary_t){ 
        .index = 11, 
        .size = 7 
    };
    clock_board->clock_word_boundary_table[CLOCK_WORD_DREIVIERTEL] = (clock_word_boundary_t){ 
        .index = 22, 
        .size = 11 
    };
    clock_board->clock_word_boundary_table[CLOCK_WORD_VIERTEL] = (clock_word_boundary_t){ 
        .index = 26, 
        .size = 7 
    };
    clock_board->clock_word_boundary_table[CLOCK_WORD_VOR] = (clock_word_boundary_t){ 
        .index = 41, 
        .size = 3 
    };
    clock_board->clock_word_boundary_table[CLOCK_WORD_NACH] = (clock_word_boundary_t){ 
        .index = 33, 
        .size = 4 
    };
    clock_board->clock_word_boundary_table[CLOCK_WORD_HALB] = (clock_word_boundary_t){ 
        .index = 44, 
        .size = 4 
    };
    clock_board->clock_word_boundary_table[CLOCK_WORD_EIN] = (clock_word_boundary_t){ 
        .index = 63, 
        .size = 3 
    };
    clock_board->clock_word_boundary_table[CLOCK_WORD_EINS] = (clock_word_boundary_t){ 
        .index = 62, 
        .size = 4 
    };
    clock_board->clock_word_boundary_table[CLOCK_WORD_ZWEI] = (clock_word_boundary_t){ 
        .index = 55, 
        .size = 4 
    };
    clock_board->clock_word_boundary_table[CLOCK_WORD_DREI] = (clock_word_boundary_t){ 
        .index = 66, 
        .size = 4 
    };
    clock_board->clock_word_boundary_table[CLOCK_WORD_VIER] = (clock_word_boundary_t){ 
        .index = 73, 
        .size = 4 
    };
    clock_board->clock_word_boundary_table[CLOCK_WORD_FUENF_2] = (clock_word_boundary_t){ 
        .index = 51, 
        .size = 4
    };
    clock_board->clock_word_boundary_table[CLOCK_WORD_SECHS] = (clock_word_boundary_t){ 
        .index = 83, 
        .size = 5 
    };
    clock_board->clock_word_boundary_table[CLOCK_WORD_SIEBEN] = (clock_word_boundary_t){ 
        .index = 88, 
        .size = 6 
    };
    clock_board->clock_word_boundary_table[CLOCK_WORD_ACHT] = (clock_word_boundary_t){ 
        .index = 77, 
        .size = 4 
    };
    clock_board->clock_word_boundary_table[CLOCK_WORD_NEUN] = (clock_word_boundary_t){ 
        .index = 103, 
        .size = 4 
    };
    clock_board->clock_word_boundary_table[CLOCK_WORD_ZEHN_2] = (clock_word_boundary_t){ 
        .index = 106, 
        .size = 4 
    };
    clock_board->clock_word_boundary_table[CLOCK_WORD_ELF] = (clock_word_boundary_t){ 
        .index = 49, 
        .size = 3 
    };
    clock_board->clock_word_boundary_table[CLOCK_WORD_ZWOELF] = (clock_word_boundary_t){ 
        .index = 94, 
        .size = 5 
    };
    clock_board->clock_word_boundary_table[CLOCK_WORD_UHR] = (clock_word_boundary_t){ 
        .index = 99, 
        .size = 3 
    };
    clock_board->clock_word_boundary_table[CLOCK_MINUTE_1] = (clock_word_boundary_t){ 
        .index = 113, 
        .size = 1 
    };
    clock_board->clock_word_boundary_table[CLOCK_MINUTE_2] = (clock_word_boundary_t){ 
        .index = 112, 
        .size = 1 
    };
    clock_board->clock_word_boundary_table[CLOCK_MINUTE_3] = (clock_word_boundary_t){ 
        .index = 111, 
        .size = 1 
    };
    clock_board->clock_word_boundary_table[CLOCK_MINUTE_4] = (clock_word_boundary_t){ 
        .index = 110, 
        .size = 1 
    };
}

void clockgusto_update()
{
    clock_board_t* clock_board = &state.clock_board;
    uint8_t hours, minutes, seconds; 
    rtc_ds3231_get_time(&hours, &minutes, &seconds); 
   
    if (clock_board->hours != hours || clock_board->minutes != minutes)
    {
        clock_board->flip = true;
        clock_board->hours = hours;
        clock_board->minutes = minutes;
        clock_board->seconds = seconds;
    }

    //ESP_LOGI(__FUNCTION__, "Set board time mask");
    clockgusto_set_board_time_mask();

    for (uint16_t led_idx = 0; led_idx < CLOCKGUSTO_NUM_LEDS; ++led_idx)
    {
        clock_board->leds[led_idx].on = false;
    }

    for (uint8_t mask_idx = 0; mask_idx < CLOCKGUSTO_NUM_LEDS; ++mask_idx)
    {
        uint32_t mask = 0x0 | 1 << mask_idx;
        uint32_t mask_result = mask & clock_board->time_mask;
        clock_word_boundary_t boundary = clock_board->clock_word_boundary_table[(clock_word_t)mask_idx];

        if (mask_result > 0)
        {
            for (uint16_t led_idx = boundary.index; 
                 led_idx < (boundary.index + boundary.size);
                 ++led_idx)
            {
                clock_board->leds[led_idx].on = true;
            }
        }
    }
}

void clockgusto_show()
{
    static uint32_t red = 0;
    static uint32_t green = 0;
    static uint32_t blue = 0;
    static uint16_t hue = 0;
    static uint16_t start_rgb = 0;

    if (state.clock_board.flip == true)
    {

        for (uint8_t mask_idx = 0; mask_idx < CLOCK_WORD_COUNT; ++mask_idx)
        {
            uint32_t mask = 0x0 | 1 << mask_idx;
            uint32_t mask_result = mask & state.clock_board.time_mask;

            if (mask_result > 0)
            {    
                const clock_word_str_t word = clock_word_str[(uint32_t)mask_idx];
                ESP_LOGI(__FUNCTION__, "%s", word.data);
            }
        }
    }
   
    if (state.clock_board.flip == true)
    {
        for (int led_idx = 0; 
             led_idx < CLOCKGUSTO_NUM_LEDS * CLOCKGUSTO_BYTES_PER_LED; 
             led_idx += CLOCKGUSTO_BYTES_PER_LED)
        {
            led_strip_hsv2rgb(0, 0, 0, &red, &green, &blue);
            state.led_strip_pixels[led_idx + 0] = green;
            state.led_strip_pixels[led_idx + 1] = blue;
            state.led_strip_pixels[led_idx + 2] = red;
        }
    }

    for (int led_idx = 0; led_idx < CLOCKGUSTO_NUM_LEDS; led_idx += 1) 
    {
        if (state.clock_board.leds[led_idx].on == true)
        {
            hue = led_idx * 360 / CLOCKGUSTO_NUM_LEDS + start_rgb;
            led_strip_hsv2rgb(hue, 100, 100, &red, &green, &blue);
            int color_offset = led_idx * CLOCKGUSTO_BYTES_PER_LED; 
            state.led_strip_pixels[color_offset + 0] = green;
            state.led_strip_pixels[color_offset + 1] = blue;
            state.led_strip_pixels[color_offset + 2] = red;
        }
    }

    ESP_ERROR_CHECK(rmt_transmit(state.led_chan, 
                                 state.led_encoder, 
                                 state.led_strip_pixels, 
                                 sizeof(state.led_strip_pixels), 
                                 &state.tx_config));

    ESP_ERROR_CHECK(rmt_tx_wait_all_done(state.led_chan, portMAX_DELAY));
    vTaskDelay(pdMS_TO_TICKS(20));
    start_rgb += 1;
}

void clockgusto_reset()
{
    for (uint16_t led_idx = 0; led_idx < CLOCKGUSTO_NUM_LEDS; ++led_idx)
    {
        state.clock_board.leds[led_idx].on = false;
    }
    
    state.clock_board.flip = false;
}

static void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t* r, uint32_t* g, uint32_t* b)
{
    h %= 360;
    uint32_t rgb_max = v * 2.55f;
    uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

    uint32_t i = h / 60;
    uint32_t diff = h % 60;

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

static void clockgusto_set_board_time_mask()
{
    uint8_t hours = state.clock_board.hours;
    uint8_t minutes = state.clock_board.minutes;
    uint8_t seconds = state.clock_board.seconds;
    (void)seconds;

    if (!(minutes < 15 || (20 <= minutes&& minutes < 25))) 
    {
        hours = (hours + 1) % 24; 
    }

    state.clock_board.time_mask = 0x0;
    switch (hours)
    {
        case 0:
        case 12: 
        {
            state.clock_board.time_mask |= 1 << CLOCK_WORD_ZWOELF;
        } break;
        case 1:
        case 13:
        {
            if (minutes < 5)
            {
                state.clock_board.time_mask |= 1 << CLOCK_WORD_EIN;
            }
            else
            {
                state.clock_board.time_mask |= 1 << CLOCK_WORD_EINS;
            }
        } break;
        case 2:
        case 14:
        {
            state.clock_board.time_mask |= 1 << CLOCK_WORD_ZWEI;
        } break;
        case 3:
        case 15:
        {
            state.clock_board.time_mask |= 1 << CLOCK_WORD_DREI;
        } break;
        case 4:
        case 16:
        {
            state.clock_board.time_mask |= 1 << CLOCK_WORD_VIER;
        } break;
        case 5:
        case 17:
        {
            state.clock_board.time_mask |= 1 << CLOCK_WORD_FUENF_2;
        } break;
        case 6:
        case 18:
        {
            state.clock_board.time_mask |= 1 << CLOCK_WORD_SECHS;
        } break;
        case 7:
        case 19:
        {
            state.clock_board.time_mask |= 1 << CLOCK_WORD_SIEBEN;
        } break;
        case 8:
        case 20:
        {
            state.clock_board.time_mask |= 1 << CLOCK_WORD_ACHT;
        } break;
        case 9:
        case 21:
        {
            state.clock_board.time_mask |= 1 << CLOCK_WORD_NEUN;
        } break;
        case 10:
        case 22:
        {
            state.clock_board.time_mask |= 1 << CLOCK_WORD_ZEHN_2;
        } break;
        case 11:
        case 23:
        {
            state.clock_board.time_mask |= 1 << CLOCK_WORD_ELF;
        } break;
    }

    if (0 <= minutes && minutes < 5) 
    {
        state.clock_board.time_mask |= 1 << CLOCK_WORD_ES;
        state.clock_board.time_mask |= 1 << CLOCK_WORD_IST;
        state.clock_board.time_mask |= 1 << CLOCK_WORD_UHR;
    }
    else if (5 <= minutes && minutes < 10)
    {
        state.clock_board.time_mask |= 1 << CLOCK_WORD_FUENF_1;
        state.clock_board.time_mask |= 1 << CLOCK_WORD_NACH;
    }
    else if (10 <= minutes && minutes < 15)
    {
        state.clock_board.time_mask |= 1 << CLOCK_WORD_ZEHN_1;
        state.clock_board.time_mask |= 1 << CLOCK_WORD_NACH;
    }
    else if (15 <= minutes && minutes < 20)
    {
        state.clock_board.time_mask |= 1 << CLOCK_WORD_VIERTEL;
    }
    else if (20 <= minutes && minutes < 25)
    {
        state.clock_board.time_mask |= 1 << CLOCK_WORD_ZWANZIG;
        state.clock_board.time_mask |= 1 << CLOCK_WORD_NACH;
    }
    else if (25 <= minutes && minutes < 30)
    {
        state.clock_board.time_mask |= 1 << CLOCK_WORD_FUENF_1;
        state.clock_board.time_mask |= 1 << CLOCK_WORD_VOR;
        state.clock_board.time_mask |= 1 << CLOCK_WORD_HALB;
    }
    else if (30 <= minutes && minutes < 35)
    {
        state.clock_board.time_mask |= 1 << CLOCK_WORD_ES;
        state.clock_board.time_mask |= 1 << CLOCK_WORD_IST;
        state.clock_board.time_mask |= 1 << CLOCK_WORD_HALB;
    }
    else if (35 <= minutes && minutes < 40)
    {
        state.clock_board.time_mask |= 1 << CLOCK_WORD_FUENF_1;
        state.clock_board.time_mask |= 1 << CLOCK_WORD_NACH;
        state.clock_board.time_mask |= 1 << CLOCK_WORD_HALB;
    }
    else if (40 <= minutes && minutes < 45)
    {
        state.clock_board.time_mask |= 1 << CLOCK_WORD_ZWANZIG;
        state.clock_board.time_mask |= 1 << CLOCK_WORD_VOR;
    }
    else if (45 <= minutes && minutes < 50)
    {
        state.clock_board.time_mask |= 1 << CLOCK_WORD_DREIVIERTEL;
    }
    else if (50 <= minutes && minutes < 55)
    {
        state.clock_board.time_mask |= 1 << CLOCK_WORD_ZEHN_1;
        state.clock_board.time_mask |= 1 << CLOCK_WORD_VOR;
    }
    else // 55 <= minutes && minutes 59
    {
        state.clock_board.time_mask |= 1 << CLOCK_WORD_FUENF_1;
        state.clock_board.time_mask |= 1 << CLOCK_WORD_VOR;
    }    

    if (minutes % 5 == 1)
    {
        state.clock_board.time_mask |= 1 << CLOCK_MINUTE_1;
    }
    else if (minutes % 5 == 2)
    {
        state.clock_board.time_mask |= 1 << CLOCK_MINUTE_1;
        state.clock_board.time_mask |= 1 << CLOCK_MINUTE_2;
    }
    else if (minutes % 5 == 3)
    {
        state.clock_board.time_mask |= 1 << CLOCK_MINUTE_1;
        state.clock_board.time_mask |= 1 << CLOCK_MINUTE_2;
        state.clock_board.time_mask |= 1 << CLOCK_MINUTE_3;
    }
    else if (minutes % 5 == 4)
    {
        state.clock_board.time_mask |= 1 << CLOCK_MINUTE_1;
        state.clock_board.time_mask |= 1 << CLOCK_MINUTE_2;
        state.clock_board.time_mask |= 1 << CLOCK_MINUTE_3;
        state.clock_board.time_mask |= 1 << CLOCK_MINUTE_4;
    }
    else
    {
        state.clock_board.time_mask |= 0 << CLOCK_MINUTE_1;
        state.clock_board.time_mask |= 0 << CLOCK_MINUTE_2;
        state.clock_board.time_mask |= 0 << CLOCK_MINUTE_3;
        state.clock_board.time_mask |= 0 << CLOCK_MINUTE_4;
    }
}

