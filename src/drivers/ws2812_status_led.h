#pragma once

/**
 * @file    ws2812_status_led.h
 * @brief   WS2812 状态灯驱动封装
 */

#include "driver_base.h"
#include "project.h"

class Ws2812StatusLed : public DriverBase {
public:
    explicit Ws2812StatusLed(uint8_t pin, uint8_t brightness = 16)
        : _pin(pin), _brightness(brightness), _enabled(false), _initialized(false) {}

    bool begin() override {
        if (_pin == static_cast<uint8_t>(PIN_NC)) {
            LOG_WARN("WS2812", "引脚未配置 (NC), 跳过初始化");
            return false;
        }
        off();
        _initialized = true;
        LOG_INFO("WS2812", "状态灯已初始化, GPIO=%u", _pin);
        return true;
    }

    void reset() override { off(); }

    bool is_ok() override { return _initialized; }

    void on() {
        _enabled = true;
        write(_brightness, _brightness, _brightness);
    }

    void off() {
        _enabled = false;
        write(0, 0, 0);
    }

    void toggle() {
        if (_enabled) {
            off();
        } else {
            on();
        }
    }

private:
    void write(uint8_t red, uint8_t green, uint8_t blue) {
        neopixelWrite(_pin, red, green, blue);
    }

    uint8_t _pin;
    uint8_t _brightness;
    bool    _enabled;
    bool    _initialized;
};
