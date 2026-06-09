#include "system_monitor.h"

bool SystemMonitor::begin() {
    LOG_INFO("系统监控", "模块启动, 上报间隔=%lu ms", (unsigned long)TASK_MONITOR_INTERVAL_MS);
    return true;
}

void SystemMonitor::loop() {
    if (!_timer.ready()) return;

    uint32_t free_heap  = ESP.getFreeHeap();
    uint32_t min_heap   = ESP.getMinFreeHeap();
    uint32_t free_psram = ESP.getFreePsram();
    size_t   max_psram  = ESP.getPsramSize();

    LOG_INFO("系统监控",
             "Heap: 空闲=%u / 最小=%u | PSRAM: 空闲=%u / 总计=%u",
             free_heap, min_heap, free_psram, max_psram);
}
