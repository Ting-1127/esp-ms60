#pragma once

/**
 * @file    timer_utils.h
 * @brief   轻量非阻塞定时器 — 基于 millis()
 *
 * 用法:
 *   static TimerUtils timer(1000);  // 1 秒周期
 *   if (timer.ready()) { do_something(); }
 */

#include <Arduino.h>

class TimerUtils {
public:
    explicit TimerUtils(unsigned long interval_ms)
        : _interval(interval_ms), _last(0) {}

    /// 重置定时器 (立即从当前时刻重新计时)
    void reset() { _last = millis(); }

    /// 检查是否到期。到期后自动复位, 返回 true
    bool ready() {
        unsigned long now = millis();
        if (now - _last >= _interval) {
            _last = now;
            return true;
        }
        return false;
    }

    /// 只读已过时间, 不修改状态
    unsigned long elapsed() const { return millis() - _last; }

    /// 修改定时间隔 (下次 ready 生效)
    void set_interval(unsigned long ms) { _interval = ms; }

private:
    unsigned long _interval;
    unsigned long _last;
};
