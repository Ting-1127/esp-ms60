#pragma once

/**
 * @file    system_config.h
 * @brief   系统运行参数配置
 */

// ===================== 系统 ===================================
#define SYS_TASK_LOOP_DELAY_MS       10      // 主循环延时 (ms)
#define SYS_SERIAL_BAUDRATE           115200  // 主串口波特率
#define SYS_STARTUP_DELAY_MS          2000    // 启动等待 (ms), 便于烧录后打开串口监视器

// ===================== RTOS 任务 ==============================
#define TASK_MONITOR_INTERVAL_MS      5000    // 系统监控报告间隔

// ===================== 看门狗 =================================
#define WDT_TIMEOUT_SEC              0       // 看门狗超时 (秒), 0 = 禁用 (暂时关闭排查 boot loop)
