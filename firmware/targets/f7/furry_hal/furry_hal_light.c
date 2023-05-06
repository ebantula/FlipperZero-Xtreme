#include <core/common_defines.h>
#include <furry_hal_resources.h>
#include <furry_hal_light.h>
#include <lp5562.h>
#include <stdint.h>
#include <xtreme.h>
#include <rgb_backlight/rgb_backlight.h>

#define LED_CURRENT_RED 50
#define LED_CURRENT_GREEN 50
#define LED_CURRENT_BLUE 50
#define LED_CURRENT_WHITE 150

void furry_hal_light_init() {
    furry_hal_i2c_acquire(&furry_hal_i2c_handle_power);

    lp5562_reset(&furry_hal_i2c_handle_power);

    lp5562_set_channel_current(&furry_hal_i2c_handle_power, LP5562ChannelRed, LED_CURRENT_RED);
    lp5562_set_channel_current(&furry_hal_i2c_handle_power, LP5562ChannelGreen, LED_CURRENT_GREEN);
    lp5562_set_channel_current(&furry_hal_i2c_handle_power, LP5562ChannelBlue, LED_CURRENT_BLUE);
    lp5562_set_channel_current(&furry_hal_i2c_handle_power, LP5562ChannelWhite, LED_CURRENT_WHITE);

    lp5562_set_channel_value(&furry_hal_i2c_handle_power, LP5562ChannelRed, 0x00);
    lp5562_set_channel_value(&furry_hal_i2c_handle_power, LP5562ChannelGreen, 0x00);
    lp5562_set_channel_value(&furry_hal_i2c_handle_power, LP5562ChannelBlue, 0x00);
    lp5562_set_channel_value(&furry_hal_i2c_handle_power, LP5562ChannelWhite, 0x00);

    lp5562_enable(&furry_hal_i2c_handle_power);
    lp5562_configure(&furry_hal_i2c_handle_power);

    furry_hal_i2c_release(&furry_hal_i2c_handle_power);
}

void furry_hal_light_set(Light light, uint8_t value) {
    furry_hal_i2c_acquire(&furry_hal_i2c_handle_power);
    if(light & LightRed) {
        lp5562_set_channel_value(&furry_hal_i2c_handle_power, LP5562ChannelRed, value);
    }
    if(light & LightGreen) {
        lp5562_set_channel_value(&furry_hal_i2c_handle_power, LP5562ChannelGreen, value);
    }
    if(light & LightBlue) {
        lp5562_set_channel_value(&furry_hal_i2c_handle_power, LP5562ChannelBlue, value);
    }
    if(light & LightBacklight) {
        if(XTREME_SETTINGS()->rgb_backlight) {
            rgb_backlight_update(value);
        } else {
            uint8_t prev =
                lp5562_get_channel_value(&furry_hal_i2c_handle_power, LP5562ChannelWhite);
            lp5562_execute_ramp(
                &furry_hal_i2c_handle_power, LP5562Engine1, LP5562ChannelWhite, prev, value, 100);
        }
    }
    furry_hal_i2c_release(&furry_hal_i2c_handle_power);
}

void furry_hal_light_blink_start(Light light, uint8_t brightness, uint16_t on_time, uint16_t period) {
    furry_hal_i2c_acquire(&furry_hal_i2c_handle_power);
    lp5562_set_channel_src(
        &furry_hal_i2c_handle_power,
        LP5562ChannelRed | LP5562ChannelGreen | LP5562ChannelBlue,
        LP5562Direct);
    LP5562Channel led_ch = 0;
    if(light & LightRed) led_ch |= LP5562ChannelRed;
    if(light & LightGreen) led_ch |= LP5562ChannelGreen;
    if(light & LightBlue) led_ch |= LP5562ChannelBlue;
    lp5562_execute_blink(
        &furry_hal_i2c_handle_power, LP5562Engine2, led_ch, on_time, period, brightness);
    furry_hal_i2c_release(&furry_hal_i2c_handle_power);
}

void furry_hal_light_blink_stop() {
    furry_hal_i2c_acquire(&furry_hal_i2c_handle_power);
    lp5562_set_channel_src(
        &furry_hal_i2c_handle_power,
        LP5562ChannelRed | LP5562ChannelGreen | LP5562ChannelBlue,
        LP5562Direct);
    lp5562_stop_program(&furry_hal_i2c_handle_power, LP5562Engine2);
    furry_hal_i2c_release(&furry_hal_i2c_handle_power);
}

void furry_hal_light_blink_set_color(Light light) {
    furry_hal_i2c_acquire(&furry_hal_i2c_handle_power);
    LP5562Channel led_ch = 0;
    lp5562_set_channel_src(
        &furry_hal_i2c_handle_power,
        LP5562ChannelRed | LP5562ChannelGreen | LP5562ChannelBlue,
        LP5562Direct);
    if(light & LightRed) led_ch |= LP5562ChannelRed;
    if(light & LightGreen) led_ch |= LP5562ChannelGreen;
    if(light & LightBlue) led_ch |= LP5562ChannelBlue;
    lp5562_set_channel_src(&furry_hal_i2c_handle_power, led_ch, LP5562Engine2);
    furry_hal_i2c_release(&furry_hal_i2c_handle_power);
}

void furry_hal_light_sequence(const char* sequence) {
    do {
        if(*sequence == 'R') {
            furry_hal_light_set(LightRed, 0xFF);
        } else if(*sequence == 'r') {
            furry_hal_light_set(LightRed, 0x00);
        } else if(*sequence == 'G') {
            furry_hal_light_set(LightGreen, 0xFF);
        } else if(*sequence == 'g') {
            furry_hal_light_set(LightGreen, 0x00);
        } else if(*sequence == 'B') {
            furry_hal_light_set(LightBlue, 0xFF);
        } else if(*sequence == 'b') {
            furry_hal_light_set(LightBlue, 0x00);
        } else if(*sequence == 'W') {
            furry_hal_light_set(LightBacklight, 0xFF);
        } else if(*sequence == 'w') {
            furry_hal_light_set(LightBacklight, 0x00);
        } else if(*sequence == '.') {
            furry_delay_ms(250);
        } else if(*sequence == '-') {
            furry_delay_ms(500);
        }
        sequence++;
    } while(*sequence != 0);
}