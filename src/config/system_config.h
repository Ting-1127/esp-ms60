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
// (如需开启 FreeRTOS 多任务，在此分配栈空间和优先级)
#define TASK_MONITOR_STACK_SIZE       4096
#define TASK_MONITOR_PRIORITY         1
#define TASK_MONITOR_INTERVAL_MS      5000    // 系统监控报告间隔

// ===================== 看门狗 =================================
#define WDT_TIMEOUT_SEC              10      // 看门狗超时 (秒), 0 = 禁用
