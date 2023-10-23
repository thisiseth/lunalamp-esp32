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

/* HomeKit Lightbulb Example
*/

#ifndef _LIGHTBULB_H_
#define _LIGHTBULB_H_

#define PIN_LED_R 37
#define PIN_LED_G 2
#define PIN_LED_B 35
#define PIN_LED_WW 36
#define PIN_LED_WC 38

#define RESET_HUE 30.0
#define RESET_SATURATION 44.0
#define RESET_BRIGHTNESS 30
#define RESET_TURN_ON true

#define WW_CAL_R 330
#define WW_CAL_G 145
#define WW_CAL_B 18
#define WW_CAL_W 200

#define WC_CAL_R 300
#define WC_CAL_G 240
#define WC_CAL_B 90
#define WC_CAL_W 230

typedef struct {
    float hue;
    float saturation;
    int brightness;
    bool turn_on;
} lightbulb_init_values;

void lightbulb_init(lightbulb_init_values *values);

/**
 * @brief deinitialize the lightbulb's lowlevel module
 *
 * @param none
 *
 * @return none
 */
void lightbulb_deinit(void);

/**
 * @brief turn on/off the lowlevel lightbulb
 *
 * @param value The "On" value
 *
 * @return none
 */
int lightbulb_set_on(bool value);

/**
 * @brief set the saturation of the lowlevel lightbulb
 *
 * @param value The Saturation value
 *
 * @return 
 *     - 0 : OK
 *     - others : fail
 */
int lightbulb_set_saturation(float value);

/**
 * @brief set the hue of the lowlevel lightbulb
 *
 * @param value The Hue value
 *
 * @return 
 *     - 0 : OK
 *     - others : fail
 */
int lightbulb_set_hue(float value);

/**
 * @brief set the brightness of the lowlevel lightbulb
 *
 * @param value The Brightness value
 *
 * @return 
 *     - 0 : OK
 *     - others : fail
 */
int lightbulb_set_brightness(int value);

//int lightbulb_set_color_temperature(uint32_t value);

void lightbulb_update_chars();

#endif /* _LIGHTBULB_H_ */
