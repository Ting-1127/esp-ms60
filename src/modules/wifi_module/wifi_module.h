#pragma once

/**
 * @file    wifi_module.h
 * @brief   WiFi 连接管理模块 (STA 模式)
 */

#include "project.h"
#include "services/service_manager.h"
#include "services/settings_store.h"
#include <WiFi.h>

class WifiModule : private NonCopyable, public IModule {
public:
    static WifiModule& instance() {
        static WifiModule inst;
        return inst;
    }

    bool begin() override;
    void loop() override;

    // 命令接口
    bool set_credentials(const String& ssid, const String& password);
    bool connect();
    void disconnect();
    void clear_credentials();

    struct Status {
        String status;   // "connected" | "disconnected" | "connecting" | "failed" | "unknown"
        String ssid;
        String ip;
        int rssi;
    };
    Status get_status() const;

    // 事件接口 (供 BleControlModule 轮询)
    bool has_pending_event();
    String take_pending_event();

private:
    WifiModule() = default;

    enum class State { Idle, Connecting, Connected, Failed };

    void set_state(State new_state);
    void on_wifi_event(WiFiEvent_t event, WiFiEventInfo_t info);
    void try_reconnect();

    State _state = State::Idle;
    String _ssid;
    String _password;
    String _ip;
    int _rssi = 0;

    // 重连
    int _reconnect_attempts = 0;
    static const int MAX_RECONNECT = 3;
    unsigned long _last_reconnect_ms = 0;
    unsigned long _reconnect_interval_ms = 5000;

    // RSSI 刷新
    unsigned long _last_rssi_ms = 0;

    // 事件队列 (单 slot)
    bool _has_event = false;
    String _pending_event;
};
