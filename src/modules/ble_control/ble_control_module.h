#pragma once

/**
 * @file    ble_control_module.h
 * @brief   BLE APP 控制模块
 */

#include "project.h"
#include "services/command_router.h"
#include "services/control_protocol.h"
#include "services/service_manager.h"
#include <NimBLEDevice.h>

class BleControlModule : private NonCopyable, public IModule {
public:
    static BleControlModule& instance() {
        static BleControlModule inst;
        return inst;
    }

    bool begin() override;
    void loop() override;
    bool is_connected() const { return _connected; }

private:
    class ServerCallbacks;
    class RxCallbacks;

    BleControlModule() = default;

    void on_connected();
    void on_disconnected(int reason);
    void on_rx(const String& value);
    void process_pending_rx();
    void notify_control(const String& message);
    void notify_data(const String& message);
    void send_ready_event();

    enum class PendingRxError {
        None,
        TooLarge
    };

    NimBLEServer* _server = nullptr;
    NimBLECharacteristic* _ctrl_tx = nullptr;
    NimBLECharacteristic* _data_tx = nullptr;
    ServerCallbacks* _server_callbacks = nullptr;
    RxCallbacks* _rx_callbacks = nullptr;
    String _pending_rx;
    PendingRxError _pending_rx_error = PendingRxError::None;
    bool _has_pending_rx = false;
    bool _rx_queue_overflow = false;
    bool _connected = false;
    bool _ready_event_sent = false;
};
