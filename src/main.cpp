/**
 * @file    main.cpp
 * @brief   程序入口 — 系统初始化 → 模块注册 → 主循环
 *
 * 分层依赖:
 *   main → ServiceManager → Modules → Drivers → Config/Utils
 */

#include "project.h"
#include "drivers/ws2812_status_led.h"
#include "modules/ble_control/ble_control_module.h"
#include "modules/system_monitor/system_monitor.h"
#include "services/service_manager.h"

// ===================== 全局驱动 ================================
Ws2812StatusLed   g_status_led(PIN_LED_STATUS);

// ===================== Arduino setup() ========================
void setup() {
    // ---- 1. 硬件初始化 ----
    Serial.begin(SYS_SERIAL_BAUDRATE);
    delay(SYS_STARTUP_DELAY_MS);
    LOG_INFO("主程序", "========================================");
    LOG_INFO("主程序", " ESP32-S3-N16R8 启动");
    LOG_INFO("主程序", " Flash: %uMB | PSRAM: %uMB",
             (unsigned)(ESP.getFlashChipSize() / (1024 * 1024)),
             (unsigned)(ESP.getPsramSize() / (1024 * 1024)));
    LOG_INFO("主程序", "========================================");

#if WDT_TIMEOUT_SEC > 0
    esp_task_wdt_init(WDT_TIMEOUT_SEC, true);
    esp_task_wdt_add(NULL);
    LOG_INFO("主程序", "看门狗已启用, 超时=%d秒", WDT_TIMEOUT_SEC);
#endif

    // ---- 2. 驱动初始化 ----
    if (!g_status_led.begin()) {
        LOG_ERROR("主程序", "LED 初始化失败");
    }

    // ---- 3. 模块注册 & 启动 ----
    auto& sm = ServiceManager::instance();
    sm.register_module(&SystemMonitor::instance());
    sm.register_module(&BleControlModule::instance());
    sm.begin_all();

    // ---- 4. 启动就绪信号 ----
    g_status_led.on();
    LOG_INFO("主程序", "===== 系统就绪 =====");
}

// ===================== Arduino loop() =========================
void loop() {
    ServiceManager::instance().loop_all();

    // 心跳 (LED 闪烁)
    static TimerUtils heartbeat(500);
    if (heartbeat.ready()) g_status_led.toggle();

#if WDT_TIMEOUT_SEC > 0
    esp_task_wdt_reset();
#endif

    delay(SYS_TASK_LOOP_DELAY_MS);
}
