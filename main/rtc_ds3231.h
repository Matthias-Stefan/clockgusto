#ifndef RTC_DS3231_H
#define RTC_DS3231_H

#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#define DS3231_ADDR 0x68  // I2C-Address of DS3231

// Register
#define DS3231_REG_SECONDS  0x00
#define DS3231_REG_MINUTES  0x01
#define DS3231_REG_HOURS    0x02
#define DS3231_REG_DAY      0x03
#define DS3231_REG_DATE     0x04
#define DS3231_REG_MONTH    0x05
#define DS3231_REG_YEAR     0x06
#define DS3231_REG_ALARM1   0x07
#define DS3231_REG_ALARM2   0x0B
#define DS3231_REG_CONTROL  0x0E
#define DS3231_REG_STATUS   0x0F
#define DS3231_REG_TEMP_MSB 0x11
#define DS3231_REG_TEMP_LSB 0x12

/** */
esp_err_t rtc_ds3231_init(uint8_t sda_pin, uint8_t scl_pin, uint32_t freq_hz);

/** */
esp_err_t rtc_ds3231_set_time(uint8_t hours, uint8_t minutes, uint8_t seconds);

/** */
esp_err_t rtc_ds3231_get_time(uint8_t* hours, uint8_t* minutes, uint8_t* seconds);

/** */
esp_err_t rtc_ds3231_set_date(uint8_t day, uint8_t date, uint8_t month, uint8_t year);

/** */
esp_err_t rtc_ds3231_get_date(uint8_t* day, uint8_t* date, uint8_t* month, uint8_t* year);

/** */
esp_err_t rtc_ds3231_get_temperature(float* temperature);

#endif
