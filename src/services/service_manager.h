#pragma once

/**
 * @file    service_manager.h
 * @brief   服务管理器 — 统一管理模块生命周期
 *
 * 派生自 NonCopyable, 单例模式。
 * 所有 Module/Service 在此注册, 统一调用 begin() / loop()。
 *
 * 用法:
 *   ServiceManager::instance().register_module(&sysMon);
 *   ServiceManager::instance().begin_all();
 *   ServiceManager::instance().loop_all();
 */

#include "project.h"
#include <vector>

// ===================== 模块基类 ===============================
class IModule {
public:
    virtual ~IModule() = default;
    virtual bool begin() = 0;
    virtual void loop() = 0;
};

// ===================== 服务管理器 =============================
class ServiceManager : private NonCopyable {
public:
    static ServiceManager& instance() {
        static ServiceManager inst;
        return inst;
    }

    void register_module(IModule* mod) {
        if (mod) _modules.push_back(mod);
    }

    void begin_all() {
        LOG_INFO("服务管理", "===== 初始化 %zu 个模块 =====", _modules.size());
        for (auto* m : _modules) {
            if (!m->begin()) {
                LOG_ERROR("服务管理", "模块 0x%p 初始化失败", (void*)m);
            }
        }
    }

    void loop_all() {
        for (auto* m : _modules) m->loop();
    }

private:
    ServiceManager() = default;
    std::vector<IModule*> _modules;
};
