#pragma once

/**
 * @file    firmware_config.h
 * @brief   固件元数据配置
 */

#define FW_NAME              "esp-ms60"
#define FW_VERSION           "0.1.0"
#define FW_BOARD             "ESP32-S3-N16R8"
#define FW_BUILD_DATE        __DATE__
#define FW_BUILD_TIME        __TIME__

// GitHub OTA 源
#define OTA_GITHUB_OWNER     "Ting-1127"
#define OTA_GITHUB_REPO      "esp-ms60"
#define OTA_FIRMWARE_FILE    "ms-60-firmware.zip"
