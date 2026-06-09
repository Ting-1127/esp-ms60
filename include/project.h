#pragma once

/**
 * @file    project.h
 * @brief   项目全局公共头文件 — 所有模块统一引用入口
 *
 * 包含顺序: 标准库 → Arduino → 本项目各层
 */

// ===================== Arduino / ESP-IDF =====================
#include <Arduino.h>

// ===================== 本项目各层 =============================
#include "config/config.h"
#include "utils/logger.h"
#include "utils/timer_utils.h"
#include "utils/non_copyable.h"
