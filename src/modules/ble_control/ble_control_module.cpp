#include "ble_control_module.h"
#include "modules/wifi_module/wifi_module.h"
#include "modules/ota_module/ota_module.h"
#include "modules/radar_module/radar_module.h"
#include <string>

class BleControlModule::ServerCallbacks : public NimBLEServerCallbacks {
public:
    explicit ServerCallbacks(BleControlModule& owner) : _owner(owner) {}

    void onConnect(NimBLEServer* server, NimBLEConnInfo& conn_info) override {
        (void)server;
        (void)conn_info;
        _owner.on_connected();
    }

    void onDisconnect(NimBLEServer* server, NimBLEConnInfo& conn_info, int reason) override {
        (void)server;
        (void)conn_info;
        _owner.on_disconnected(reason);
    }

private:
    BleControlModule& _owner;
};

class BleControlModule::RxCallbacks : public NimBLECharacteristicCallbacks {
public:
    explicit RxCallbacks(BleControlModule& owner) : _owner(owner) {}

    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& conn_info) override {
        (void)conn_info;
        std::string value = characteristic->getValue();
        _owner.on_rx(String(value.c_str()));
    }

private:
    BleControlModule& _owner;
};

bool BleControlModule::begin() {
    LOG_INFO("BLE控制", "模块启动, 设备名=%s", BLE_DEVICE_NAME);

    NimBLEDevice::init(BLE_DEVICE_NAME);

    _server = NimBLEDevice::createServer();
    _server_callbacks = new ServerCallbacks(*this);
    _server->setCallbacks(_server_callbacks);

    NimBLEService* service = _server->createService(BLE_CTRL_SERVICE_UUID);

    NimBLECharacteristic* ctrl_rx = service->createCharacteristic(
        BLE_CTRL_RX_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    _rx_callbacks = new RxCallbacks(*this);
    ctrl_rx->setCallbacks(_rx_callbacks);

    _ctrl_tx = service->createCharacteristic(
        BLE_CTRL_TX_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    _data_tx = service->createCharacteristic(
        BLE_DATA_TX_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    service->start();

    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->addServiceUUID(BLE_CTRL_SERVICE_UUID);
    advertising->setName(BLE_DEVICE_NAME);
    advertising->start();

    LOG_INFO("BLE控制", "广播已启动, service=%s", BLE_CTRL_SERVICE_UUID);
    return true;
}

void BleControlModule::loop() {
    process_pending_rx();

    if (_connected && !_ready_event_sent) {
        send_ready_event();
        _ready_event_sent = true;
    }

    // 轮询 WiFi/OTA 事件
    if (_connected) {
        auto& wifi = WifiModule::instance();
        auto& ota = OtaModule::instance();
        auto& radar = RadarModule::instance();
        if (wifi.has_pending_event()) {
            notify_control(wifi.take_pending_event());
        }
        if (ota.has_pending_event()) {
            notify_control(ota.take_pending_event());
        }
        if (radar.has_pending_event()) {
            notify_data(radar.take_pending_event());
        }
    }
}

void BleControlModule::on_connected() {
    _connected = true;
    _ready_event_sent = false;
    LOG_INFO("BLE控制", "客户端已连接");
}

void BleControlModule::on_disconnected(int reason) {
    _connected = false;
    _ready_event_sent = false;
    LOG_INFO("BLE控制", "客户端已断开, reason=%d, 重新广播", reason);
    NimBLEDevice::startAdvertising();
}

void BleControlModule::on_rx(const String& value) {
    if (value.length() > BLE_MAX_RX_BYTES) {
        _pending_rx_error = PendingRxError::TooLarge;
        _has_pending_rx = true;
        return;
    }

    if (_has_pending_rx) {
        _rx_queue_overflow = true;
        return;
    }

    _pending_rx = value;
    _pending_rx_error = PendingRxError::None;
    _has_pending_rx = true;
}

void BleControlModule::process_pending_rx() {
    if (!_has_pending_rx) {
        if (_rx_queue_overflow) {
            _rx_queue_overflow = false;
            notify_control(ControlProtocol::make_empty_response("",
                                                               false,
                                                               PROTO_CODE_INTERNAL_ERROR,
                                                               "request queue full"));
        }
        return;
    }

    PendingRxError error = _pending_rx_error;
    String input = _pending_rx;
    _pending_rx = "";
    _pending_rx_error = PendingRxError::None;
    _has_pending_rx = false;

    if (error == PendingRxError::TooLarge) {
        notify_control(ControlProtocol::make_empty_response("",
                                                           false,
                                                           PROTO_CODE_BAD_JSON,
                                                           "request too large"));
        return;
    }

    StaticJsonDocument<PROTO_JSON_DOC_BYTES> doc;
    ControlRequest request;
    String error_code;
    String error_message;

    if (!ControlProtocol::parse_request(input, doc, request, error_code, error_message)) {
        LOG_WARN("BLE控制", "RX 无效请求: %s", error_code.c_str());
        notify_control(ControlProtocol::make_empty_response(request.id,
                                                           false,
                                                           error_code.c_str(),
                                                           error_message.c_str()));
        return;
    }

    LOG_INFO("BLE控制", "RX cmd=%s id=%s len=%u",
             request.cmd.c_str(), request.id.c_str(), (unsigned)input.length());

    RuntimeStatus status{_connected, &WifiModule::instance(), &OtaModule::instance(), &RadarModule::instance()};
    String response = CommandRouter::handle(request, status);
    notify_control(response);
}

void BleControlModule::notify_control(const String& message) {
    if (!_connected || !_ctrl_tx) return;

    for (unsigned int offset = 0; offset < message.length(); offset += BLE_NOTIFY_CHUNK_BYTES) {
        String chunk = message.substring(offset, offset + BLE_NOTIFY_CHUNK_BYTES);
        _ctrl_tx->setValue(chunk.c_str());
        _ctrl_tx->notify();
        delay(5);
    }
    LOG_INFO("BLE控制", "TX CTRL len=%u", (unsigned)message.length());
}

void BleControlModule::notify_data(const String& message) {
    if (!_connected || !_data_tx) return;

    for (unsigned int offset = 0; offset < message.length(); offset += BLE_NOTIFY_CHUNK_BYTES) {
        String chunk = message.substring(offset, offset + BLE_NOTIFY_CHUNK_BYTES);
        _data_tx->setValue(chunk.c_str());
        _data_tx->notify();
        delay(5);
    }
    LOG_DEBUG("BLE控制", "TX DATA len=%u", (unsigned)message.length());
}

void BleControlModule::send_ready_event() {
    notify_control(ControlProtocol::make_empty_event(PROTO_EVT_SYS_READY));
    LOG_INFO("BLE控制", "TX evt=%s", PROTO_EVT_SYS_READY);
}
