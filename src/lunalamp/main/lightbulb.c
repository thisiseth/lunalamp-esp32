/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS products only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/* HomeKit Lightbulb Example Dummy Implementation
 * Refer ESP IDF docs for LED driver implementation:
 * https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/ledc.html
 * https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/rmt.html
*/

#include <stdio.h>
#include <stdbool.h>
#include "esp_log.h"

#include "lightbulb.h"

#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include <math.h>

static const char *TAG = "lightbulb";

#define NVS_TURN_ON "on"
#define NVS_HUE "hue"
#define NVS_SATURATION "saturation"
#define NVS_BRIGHTNESS "brightness"

static nvs_handle_t light_nvs_handle;

static void nvs_set_light_value(const char *key, uint32_t value)
{
    if (nvs_set_u32(light_nvs_handle, key, value) == ESP_OK)
        nvs_commit(light_nvs_handle);
}

#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_11_BIT
#define LEDC_FREQ 39060

#define LEDC_CHANNEL_R LEDC_CHANNEL_0
#define LEDC_CHANNEL_G LEDC_CHANNEL_1
#define LEDC_CHANNEL_B LEDC_CHANNEL_2
#define LEDC_CHANNEL_WW LEDC_CHANNEL_3
#define LEDC_CHANNEL_WC LEDC_CHANNEL_4

#define LEDC_DUTY_MAX (1 << LEDC_DUTY_RES)
#define LEDC_DUTY_MIN (LEDC_DUTY_MAX / 512)

static bool current_on;
static float current_hue;
static float current_saturation;
static int current_brightness;

static void update_leds();
static void set_led_duty(float r, float g, float b, float ww, float wc);
/**
 * @brief initialize the lightbulb lowlevel module
 */
void lightbulb_init(lightbulb_init_values* values)
{
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    err = nvs_open("light_chars", NVS_READWRITE, &light_nvs_handle);

    values->turn_on = RESET_TURN_ON;
    values->hue = RESET_HUE;
    values->saturation = RESET_SATURATION;
    values->brightness = RESET_BRIGHTNESS;

    uint32_t nvs_value;

    if (nvs_get_u32(light_nvs_handle, NVS_TURN_ON, &nvs_value) == ESP_OK)
        values->turn_on = !!nvs_value;

    if (nvs_get_u32(light_nvs_handle, NVS_HUE, &nvs_value) == ESP_OK)
        values->hue = nvs_value/100000.0F;

    if (nvs_get_u32(light_nvs_handle, NVS_SATURATION, &nvs_value) == ESP_OK)
        values->saturation = nvs_value/100000.0F;

    if (nvs_get_u32(light_nvs_handle, NVS_BRIGHTNESS, &nvs_value) == ESP_OK)
        values->brightness = nvs_value;

    if (values->hue > 360.0F)
        values->hue = 360.0F;
    if (values->hue < 0.0F)
        values->hue = 0.0F;

    if (values->saturation > 100.0F)
        values->saturation = 100.0F;
    if (values->saturation < 0.0F)
        values->saturation = 0.0F;

    if (values->brightness > 100)
        values->brightness = 100;
    if (values->brightness < 0)
        values->brightness = 0;

    current_on = values->turn_on;
    current_hue = values->hue;
    current_saturation = values->saturation;
    current_brightness = values->brightness;

    gpio_set_drive_capability(LEDC_CHANNEL_R, GPIO_DRIVE_CAP_3);
    gpio_set_drive_capability(LEDC_CHANNEL_G, GPIO_DRIVE_CAP_3);
    gpio_set_drive_capability(LEDC_CHANNEL_B, GPIO_DRIVE_CAP_3);
    gpio_set_drive_capability(LEDC_CHANNEL_WW, GPIO_DRIVE_CAP_3);
    gpio_set_drive_capability(LEDC_CHANNEL_WC, GPIO_DRIVE_CAP_3);
    
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQ,  // Set output frequency at 5 kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ESP_LOGI(TAG, "pwm timer freq: %lu", ledc_get_freq(LEDC_MODE, LEDC_TIMER));

    ledc_channel_config_t ledc_channel_r = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL_R,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PIN_LED_R,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_r));
    
    ledc_channel_config_t ledc_channel_g = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL_G,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PIN_LED_G,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_g));
    
    ledc_channel_config_t ledc_channel_b = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL_B,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PIN_LED_B,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_b));
    
    ledc_channel_config_t ledc_channel_ww = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL_WW,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PIN_LED_WW,
        .duty           = 0,
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_ww));
    
    ledc_channel_config_t ledc_channel_wc = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL_WC,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PIN_LED_WC,
        .duty           = 0, 
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_wc));
    
    update_leds();

    ESP_LOGI(TAG, "on: %d", values->turn_on);
    ESP_LOGI(TAG, "hue: %f", values->hue);
    ESP_LOGI(TAG, "saturation: %f", values->saturation);
    ESP_LOGI(TAG, "brightness: %d", values->turn_on);
}

/**
 * @brief turn on/off the lowlevel lightbulb
 */
int lightbulb_set_on(bool value)
{
    current_on = value;
    return 0;
}

/**
 * @brief set the hue of the lowlevel lightbulb
 */
int lightbulb_set_hue(float value)
{
    if (value > 360.0F || value < 0.0F)
        return 1;

    current_hue = value;
    return 0;
}

/**
 * @brief set the saturation of the lowlevel lightbulb
 */
int lightbulb_set_saturation(float value)
{
    if (value > 100.0F || value < 0.0F)
        return 1;

    current_saturation = value;
    return 0;
}

/**
 * @brief set the brightness of the lowlevel lightbulb
 */
int lightbulb_set_brightness(int value)
{
    if (value > 100 || value < 0)
        return 1;

    current_brightness = value;
    return 0;
}

//int lightbulb_set_color_temperature(uint32_t value)
//{
//    return 0;
//}

void lightbulb_update_chars()
{
    update_leds();

    nvs_set_light_value(NVS_BRIGHTNESS, current_brightness);
    nvs_set_light_value(NVS_HUE, current_hue*100000.0F);
    nvs_set_light_value(NVS_SATURATION, current_saturation*100000.0F);
    nvs_set_light_value(NVS_TURN_ON, !!current_on);
}

//'gamma' correction :|
#define apply_gamma(val) (val*val)

#define vec_length(x, y, z) sqrtf(x*x + y*y + z*z)

//hue 30 sat 67 from iphone app
#define WW_VEC_R 1.0000F
#define WW_VEC_G 0.4422F
#define WW_VEC_B 0.1089F

#define WW_VEC_LENGTH vec_length(WW_VEC_R, WW_VEC_G, WW_VEC_B)
#define WC_VEC_LENGTH vec_length(1, 1, 1)

#define CLAMP_COS(val) (val > 1.0F ? 1.0F : (val < -1.0F ? -1.0F : val))

#define MIN_COLOR_VALUE (1.0F / LEDC_DUTY_MAX)

static void update_leds() 
{
    ESP_LOGI(TAG, "leds: %d, %.2f, %.2f, %d", current_on, current_hue, current_saturation, current_brightness);

    if (!current_on)
    {
        set_led_duty(0, 0, 0, 0, 0);
        return;
    }

    //naive hsv-rgb conversion
    float r = 0, g = 0, b = 0;
    float c = (current_brightness/100.0F) * (current_saturation/100.0F);
    float x = c * (1.0F - fabsf(fmodf(current_hue/60.0F, 2.0F) - 1.0F));
    float m = (current_brightness/100.0F) - c;

    //hue-based component
    if (current_hue < 60.0F)
    {
        r = c;
        g = x;
    } 
    else if (current_hue < 120.0F)
    {
        r = x;
        g = c;
    } 
    else if (current_hue < 180.0F)
    {
        g = c;
        b = x;
    } 
    else if (current_hue < 240.0F)
    {
        g = x;
        b = c;
    } 
    else if (current_hue < 300.0F)
    {
        r = x;
        b = c;
    } else 
    {
        r = c;
        b = x;
    }

    //'white'-based component
    r += m;
    g += m;
    b += m;

    //switch to linear color space
    r = apply_gamma(r);
    g = apply_gamma(g);
    b = apply_gamma(b);

    ESP_LOGI(TAG, "gamma corrected r: %5f g: %5f b: %5f", r, g, b);

    if (r < MIN_COLOR_VALUE && 
        g < MIN_COLOR_VALUE && 
        b < MIN_COLOR_VALUE)
    {
        ESP_LOGI(TAG, "nothing to calculate, r: %8f g: %8f b: %8f", r, g, b);
        set_led_duty(0, 0, 0, 0, 0);
        return;
    }

    //find distance to both whites
    float cos_ww = (r*WW_VEC_R + g*WW_VEC_G + b*WW_VEC_B) / (vec_length(r, g, b) * WW_VEC_LENGTH);
    float cos_wc = (r + g + b) / (vec_length(r, g, b) * WC_VEC_LENGTH);

    cos_ww = CLAMP_COS(cos_ww);
    cos_wc = CLAMP_COS(cos_wc);

    float sin_ww = sqrtf(1 - cos_ww*cos_ww);
    float sin_wc = sqrtf(1 - cos_wc*cos_wc);

    ESP_LOGI(TAG, "cos_ww: %5f cos_wc: %5f sin_ww: %5f sin_wc: %5f", cos_ww, cos_wc, sin_ww, sin_wc);

    //close enough to use only one led
    if (sin_ww < 0.01F)
        sin_ww = 0;
    else if (sin_wc < 0.01F)
        sin_wc = 0;

    float wc_quantity = sin_ww / (sin_ww + (WC_VEC_LENGTH/WW_VEC_LENGTH)*sin_wc);
    float ww_quantity = 1 - wc_quantity;

    float whites_r = ww_quantity*WW_VEC_R + wc_quantity;
    float whites_g = ww_quantity*WW_VEC_G + wc_quantity;
    float whites_b = ww_quantity*WW_VEC_B + wc_quantity;

    float whites_quantity = fminf(r/whites_r, fminf(g/whites_g, b/whites_b));
    
    //subtract whites
    r -= whites_r*whites_quantity;
    g -= whites_g*whites_quantity;
    b -= whites_b*whites_quantity;

    //apply rgb to white coeffs
    //todo: lower coeffs when saturation is high to allow full green&blue ?
    r *= WC_CAL_R/(float)WC_CAL_W;
    g *= WC_CAL_G/(float)WC_CAL_W;
    b *= WC_CAL_B/(float)WC_CAL_W;

    ESP_LOGI(TAG, "r: %5f g: %5f b: %5f ww: %5f wc: %5f", r, g, b, ww_quantity*whites_quantity, wc_quantity*whites_quantity);

    set_led_duty(r, g, b, ww_quantity*whites_quantity, wc_quantity*whites_quantity);
}

#define CLAMP_DUTY(x) x < LEDC_DUTY_MIN ? (x > 0.0F ? LEDC_DUTY_MIN : 0.0F) : (x > LEDC_DUTY_MAX ? LEDC_DUTY_MAX : x)

static void set_led_duty(float r, float g, float b, float ww, float wc)
{
    uint32_t duty_r = CLAMP_DUTY((int)(r*LEDC_DUTY_MAX));
    uint32_t duty_g = CLAMP_DUTY((int)(g*LEDC_DUTY_MAX));
    uint32_t duty_b = CLAMP_DUTY((int)(b*LEDC_DUTY_MAX));
    uint32_t duty_ww = CLAMP_DUTY((int)(ww*LEDC_DUTY_MAX));
    uint32_t duty_wc = CLAMP_DUTY((int)(wc*LEDC_DUTY_MAX));

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_R, duty_r);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_G, duty_g);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_B, duty_b);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_WW, duty_ww);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_WC, duty_wc);

    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_R);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_G);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_B);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_WW);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_WC);
}