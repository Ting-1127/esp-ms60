#pragma once

/**
 * @file    driver_base.h
 * @brief   驱动基类接口 — 所有硬件驱动必须实现的统一生命周期
 *
 * 约定:
 *   begin()   — 初始化, 返回 true 表示成功
 *   reset()   — 软复位
 *   is_ok()   — 自检状态 (可由子类扩展为定时心跳检查)
 */

class DriverBase {
public:
    virtual ~DriverBase() = default;

    virtual bool begin()  = 0;
    virtual void reset()  = 0;
    virtual bool is_ok()  = 0;
};
