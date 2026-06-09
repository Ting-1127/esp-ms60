#pragma once

/**
 * @file    gpio_driver.h
 * @brief   GPIO 驱动封装 — 演示 DriverBase 接口用法
 */

#include "driver_base.h"
#include "project.h"

class GpioDriver : public DriverBase {
public:
    explicit GpioDriver(uint8_t pin, uint8_t mode = OUTPUT)
        : _pin(pin), _mode(mode), _initialized(false) {}

    bool begin() override {
        if (_pin == static_cast<uint8_t>(PIN_NC)) {
            LOG_WARN("GPIO", "引脚未配置 (NC), 跳过初始化");
            return false;
        }
        pinMode(_pin, _mode);
        _initialized = true;
        LOG_DEBUG("GPIO", "引脚 %u 初始化为模式 %u", _pin, _mode);
        return true;
    }

    void reset() override {
        digitalWrite(_pin, LOW);
        LOG_DEBUG("GPIO", "引脚 %u 复位 (LOW)", _pin);
    }

    bool is_ok() override { return _initialized; }

    void on()  { digitalWrite(_pin, HIGH); }
    void off() { digitalWrite(_pin, LOW);  }
    void toggle() { digitalWrite(_pin, !digitalRead(_pin)); }

    int read() { return digitalRead(_pin); }

private:
    uint8_t _pin;
    uint8_t _mode;
    bool    _initialized;
};
