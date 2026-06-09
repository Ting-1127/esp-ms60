#pragma once

/**
 * @file    logger.h
 * @brief   统一日志模块 — 封装 Serial + 级别控制 + 格式化
 *
 * 用法:
 *   LOG_INFO("主循环", "free heap: %u", ESP.getFreeHeap());
 *   LOG_WARN("WiFi", "信号弱: RSSI=%d", rssi);
 *   LOG_ERROR("传感器", "初始化失败, 错误码=%d", err);
 */

#include <Arduino.h>

// ===================== 日志级别 ================================
enum class LogLevel {
    NONE    = 0,
    ERROR   = 1,
    WARN    = 2,
    INFO    = 3,
    DEBUG   = 4
};

// 可通过串口修改变量调整级别, 默认在 system_config.h 中定义, 这里给出 fallback
#ifndef LOG_LEVEL_CURRENT
  #ifdef CORE_DEBUG_LEVEL
    #if CORE_DEBUG_LEVEL >= 4
      #define LOG_LEVEL_CURRENT  LogLevel::DEBUG
    #elif CORE_DEBUG_LEVEL >= 3
      #define LOG_LEVEL_CURRENT  LogLevel::INFO
    #elif CORE_DEBUG_LEVEL >= 2
      #define LOG_LEVEL_CURRENT  LogLevel::WARN
    #elif CORE_DEBUG_LEVEL >= 1
      #define LOG_LEVEL_CURRENT  LogLevel::ERROR
    #else
      #define LOG_LEVEL_CURRENT  LogLevel::NONE
    #endif
  #else
    #define LOG_LEVEL_CURRENT    LogLevel::INFO
  #endif
#endif

// ===================== 日志宏 =================================
#define LOG_ERROR(tag, fmt, ...)  log_print(LogLevel::ERROR, tag, fmt, ##__VA_ARGS__)
#define LOG_WARN(tag,  fmt, ...)  log_print(LogLevel::WARN,  tag, fmt, ##__VA_ARGS__)
#define LOG_INFO(tag,  fmt, ...)  log_print(LogLevel::INFO,  tag, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(tag, fmt, ...)  log_print(LogLevel::DEBUG, tag, fmt, ##__VA_ARGS__)

// ===================== 实现 (header-only) =====================
inline void log_print(LogLevel level, const char* tag, const char* fmt, ...) {
    if (static_cast<int>(level) > static_cast<int>(LOG_LEVEL_CURRENT)) return;

    static const char* level_str[] = { "", "[ERR]", "[WRN]", "[INF]", "[DBG]" };
    Serial.print(level_str[static_cast<int>(level)]);
    Serial.print(" [");
    Serial.print(tag);
    Serial.print("] ");

    va_list args;
    va_start(args, fmt);
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Serial.println(buf);
}
