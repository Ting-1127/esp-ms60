#pragma once

/**
 * @file    non_copyable.h
 * @brief   CRTP non-copyable 基类 — 禁止拷贝/移动的辅助工具
 *
 * 用法:
 *   class MyDriver : private NonCopyable { ... };
 */

class NonCopyable {
protected:
    NonCopyable()  = default;
    ~NonCopyable() = default;

    NonCopyable(const NonCopyable&)            = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
    NonCopyable(NonCopyable&&)                 = delete;
    NonCopyable& operator=(NonCopyable&&)      = delete;
};
