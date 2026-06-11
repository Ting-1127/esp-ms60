#pragma once

/**
 * @file    board_pins.h
 * @brief   板级引脚定义 — 基于 ESP32-S3 N16R8 核心开发板实际针脚
 *
 * 使用规范:
 *   - 所有引脚定义统一写在此处, 模块代码不硬编码引脚号
 *   - 命名规则: PIN_<功能>_<子功能> (如 PIN_LED_STATUS)
 *   - 未使用的引脚注释为 PIN_NC (-1)
 *   - 添加外设前先查此表, 避免引脚冲突
 */

// ===================== 通用 GPIO =============================
#define PIN_LED_STATUS      48      // 板载 WS2812/RGB 状态灯数据脚
#define PIN_BUTTON_BOOT     36      // BOOT 按钮 (文档标注 GPIO35/36/45 均为 BOOT)

// ===================== USB (硬件占用, 勿用) ====================
// USB_D+ = GPIO20, USB_D- = GPIO19

// ===================== UART ==================================
#define PIN_UART0_TX        43      // U0TXD (绿:TX) — 默认调试串口
#define PIN_UART0_RX        44      // U0RXD (蓝:RX)
#define PIN_UART1_TX        17      // 备用串口 (RTC)
#define PIN_UART1_RX        18      // 备用串口 (RTC)
#define PIN_UART2_TX        16      // 雷达 MS60 UART (RTC)
#define PIN_UART2_RX        15      // 雷达 MS60 UART (RTC)

// ===================== I2C (避开 USB 和 RTC 高频脚) ============
#define PIN_I2C_SDA         4       // RTC_GPIO4
#define PIN_I2C_SCL         5       // RTC_GPIO5

// ===================== SPI (使用 RTC 引脚组, 方便布线) =========
#define PIN_SPI_MOSI        11      // RTC_GPIO11
#define PIN_SPI_MISO        13      // RTC_GPIO13
#define PIN_SPI_SCK         12      // RTC_GPIO12
#define PIN_SPI_CS0         10      // RTC_GPIO10

// ===================== 可用 GPIO 清单 (供分配参考) ============
// 通用:   GPIO1, GPIO21, GPIO37, GPIO38, GPIO47
// RTC:    GPIO2,3,4,5,6,7,9,10,11,12,13,14,15,16,17,18,39,40,41,42,46
// 已占用: GPIO19(USB_D-), GPIO20(USB_D+), GPIO36(BOOT), GPIO43(U0TXD),
//         GPIO44(U0RXD), GPIO48(LED)
// 保留未分配: GPIO21,37,38,47 等

// ===================== 保留 ===================================
#define PIN_NC              (-1)    // Not Connected
