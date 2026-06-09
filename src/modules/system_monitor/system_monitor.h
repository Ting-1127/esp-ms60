#pragma once

/**
 * @file    system_monitor.h
 * @brief   系统监控模块 — 定期上报 FreeRTOS 任务栈 / 堆内存 / PSRAM
 */

#include "project.h"

class SystemMonitor : private NonCopyable {
public:
    static SystemMonitor& instance() {
        static SystemMonitor inst;
        return inst;
    }

    bool begin();
    void loop();   ///< 主循环中持续调用

private:
    SystemMonitor() = default;
    TimerUtils _timer{TASK_MONITOR_INTERVAL_MS};
};
