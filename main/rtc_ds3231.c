#include "rtc_ds3231.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "hal/gpio_types.h"
#include "hal/i2c_types.h"
#include <stdbool.h>
#include <stdint.h>

#define I2C_MASTER_NUM        I2C_NUM_0
#define I2C_MASTER_TIMEOUT_MS 1000

static const char *TAG = "rtc ds3231";

static esp_err_t rtc_ds3231_write_register(uint8_t reg, uint8_t value)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_WRITE, true); 
    i2c_master_write_byte(cmd, reg, true);                                   
    i2c_master_write_byte(cmd, value, true);                                 
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (ret != ESP_OK)
    {
        return ret;
    }
    i2c_cmd_link_delete(cmd);

    return ESP_OK;
}

static esp_err_t rtc_ds3231_read_register(uint8_t reg, uint8_t* value)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_WRITE, true); 
    i2c_master_write_byte(cmd, reg, true);                                   
    i2c_master_start(cmd);                                                  
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_READ, true); 
    i2c_master_read_byte(cmd, value, I2C_MASTER_NACK);                      
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    if (ret != ESP_OK)
    {
        return ret;
    }
    i2c_cmd_link_delete(cmd);

    return ESP_OK; 
}

static uint8_t decimal_to_bcd(uint8_t decimal) 
{
    return ((decimal / 10) << 4) | (decimal % 10);
}

static uint8_t bcd_to_decimal(uint8_t bcd) 
{
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

esp_err_t rtc_ds3231_init(uint8_t sda_pin, uint8_t scl_pin, uint32_t freq_hz)
{
    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_pin, // serial data line
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = scl_pin, // serial clock
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = freq_hz,
    };

    esp_err_t ret = i2c_param_config(I2C_NUM_0, &i2c_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to init rtc ds3231.\n");         
        return ret;
    }

    ret = i2c_driver_install(I2C_MASTER_NUM, i2c_config.mode, 0, 0, 0);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to install driver.\n");
        return ret;
    }

    return ESP_OK;
}

esp_err_t rtc_ds3231_set_time(uint8_t hours, uint8_t minutes, uint8_t seconds)
{
    esp_err_t ret = rtc_ds3231_write_register(DS3231_REG_SECONDS, (seconds / 10) << 4 | (seconds % 10));
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write seconds\n");
        return ret;
    }
    ret = rtc_ds3231_write_register(DS3231_REG_MINUTES, (minutes / 10) << 4 | (minutes % 10));
    if (ret != ESP_OK)
    {   
        return ret;
    }
    ret = rtc_ds3231_write_register(DS3231_REG_HOURS, (hours / 10) << 4 | (hours % 10));
    if (ret != ESP_OK)
    {
        return ret;
    }

    return ESP_OK;
}

esp_err_t rtc_ds3231_get_time(uint8_t *hours, uint8_t *minutes, uint8_t *seconds)
{
    uint8_t raw_seconds = 0;
    uint8_t raw_minutes = 0;
    uint8_t raw_hours = 0;

    esp_err_t ret = rtc_ds3231_read_register(DS3231_REG_SECONDS, &raw_seconds);
    if (ret != ESP_OK)
    {
        return ret;
    }
    ret = rtc_ds3231_read_register(DS3231_REG_MINUTES, &raw_minutes);
    if (ret != ESP_OK)
    {
        return ret;
    }
    ret = rtc_ds3231_read_register(DS3231_REG_HOURS, &raw_hours);
    if (ret != ESP_OK)
    {
        return ret;
    }

    *seconds = (raw_seconds >> 4) * 10 + (raw_seconds & 0x0F);
    *minutes = (raw_minutes >> 4) * 10 + (raw_minutes & 0x0F);
    *hours = (raw_hours >> 4) * 10 + (raw_hours & 0x0F);
    return ESP_OK;
}

esp_err_t rtc_ds3231_set_date(uint8_t day, uint8_t date, uint8_t month, uint8_t year)
{
    if (day < 1 || day > 7 || date < 1 || date > 31 || month < 1 || month > 12 || year > 99) 
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t bcd_day = decimal_to_bcd(day);
    uint8_t bcd_date = decimal_to_bcd(date);
    uint8_t bcd_month = decimal_to_bcd(month);
    uint8_t bcd_year = decimal_to_bcd(year);

    esp_err_t ret = rtc_ds3231_write_register(DS3231_REG_DAY, bcd_day);
    if (ret != ESP_OK) 
    {
        return ret;
    }
    ret = rtc_ds3231_write_register(DS3231_REG_DATE, bcd_date);
    if (ret != ESP_OK)
    {
        return ret;
    }
    ret = rtc_ds3231_write_register(DS3231_REG_MONTH, bcd_month);
    if (ret != ESP_OK)
    {
        return ret;
    }
    ret = rtc_ds3231_write_register(DS3231_REG_YEAR, bcd_year);
    if (ret != ESP_OK)
    {
        return ret;
    }

    return ESP_OK;
}

esp_err_t rtc_ds3231_get_date(uint8_t* day, uint8_t* date, uint8_t* month, uint8_t* year)
{
    uint8_t raw_day, raw_date, raw_month, raw_year;

    esp_err_t ret = rtc_ds3231_read_register(DS3231_REG_DAY, &raw_day);
    if (ret != ESP_OK)
    {
        return ret;
    }
    ret = rtc_ds3231_read_register(DS3231_REG_DATE, &raw_date);
    if (ret != ESP_OK)
    {
        return ret;
    }
    ret = rtc_ds3231_read_register(DS3231_REG_MONTH, &raw_month);
    if (ret != ESP_OK)
    {
        return ret;
    }
    ret = rtc_ds3231_read_register(DS3231_REG_YEAR, &raw_year);
    if (ret != ESP_OK)
    {
        return ret;
    }

    *day = bcd_to_decimal(raw_day);
    *date = bcd_to_decimal(raw_date);
    *month = bcd_to_decimal(raw_month);
    *year = bcd_to_decimal(raw_year);

    return ESP_OK;
}

esp_err_t rtc_ds3231_get_temperature(float *temperature)
{
    uint8_t msb, lsb;

    esp_err_t ret = rtc_ds3231_read_register(DS3231_REG_TEMP_MSB, &msb);
    if (ret != ESP_OK)
    {
        return ret;
    }
    ret = rtc_ds3231_read_register(DS3231_REG_TEMP_LSB, &lsb);
    if (ret != ESP_OK)
    {
        return ret;
    }

    int8_t temp_integer = (int8_t)msb;

    float temp_fraction = (lsb >> 6) * 0.25f;

    *temperature = temp_integer + temp_fraction;

    return ESP_OK;
}

