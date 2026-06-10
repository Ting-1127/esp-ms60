#include "wifi_module.h"
#include "services/control_protocol.h"
#include "config/protocol_config.h"

bool WifiModule::begin() {
    LOG_INFO("WiFi模块", "初始化");

    SettingsStore::instance().begin();

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);

    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
        on_wifi_event(event, info);
    });

    // NVS 有凭据则自动连接
    if (SettingsStore::instance().has_wifi_credentials()) {
        SettingsStore::instance().load_wifi_credentials(_ssid, _password);
        LOG_INFO("WiFi模块", "发现已保存凭据, ssid=%s", _ssid.c_str());
        connect();
    }

    return true;
}

void WifiModule::loop() {
    if (_state == State::Connecting) {
        // 等待连接结果，超时 15 秒
        if (WiFi.status() == WL_CONNECTED) {
            // 由 on_wifi_event 处理
        }
    }

    // 失败后自动重连
    if (_state == State::Failed && _reconnect_attempts < MAX_RECONNECT) {
        unsigned long now = millis();
        if (now - _last_reconnect_ms >= _reconnect_interval_ms) {
            try_reconnect();
        }
    }

    // 定期刷新 RSSI
    if (_state == State::Connected) {
        unsigned long now = millis();
        if (now - _last_rssi_ms >= 2000) {
            _rssi = WiFi.RSSI();
            _last_rssi_ms = now;
        }
    }
}

bool WifiModule::set_credentials(const String& ssid, const String& password) {
    _ssid = ssid;
    _password = password;
    bool ok = SettingsStore::instance().save_wifi_credentials(ssid, password);
    LOG_INFO("WiFi模块", "凭据已保存: ssid=%s", ssid.c_str());
    return ok;
}

bool WifiModule::connect() {
    if (_ssid.isEmpty()) {
        LOG_WARN("WiFi模块", "无凭据, 无法连接");
        return false;
    }

    LOG_INFO("WiFi模块", "开始连接: ssid=%s", _ssid.c_str());
    set_state(State::Connecting);
    _reconnect_attempts = 0;
    _reconnect_interval_ms = 5000;

    WiFi.disconnect(true);
    delay(100);
    WiFi.begin(_ssid.c_str(), _password.c_str());

    return true;
}

void WifiModule::disconnect() {
    LOG_INFO("WiFi模块", "断开连接");
    WiFi.disconnect(true);
    _reconnect_attempts = MAX_RECONNECT;  // 禁止自动重连
    set_state(State::Idle);
}

void WifiModule::clear_credentials() {
    SettingsStore::instance().clear_wifi_credentials();
    _ssid = "";
    _password = "";
}

WifiModule::Status WifiModule::get_status() const {
    Status s;
    switch (_state) {
        case State::Connected:  s.status = "connected"; break;
        case State::Connecting: s.status = "connecting"; break;
        case State::Failed:     s.status = "failed"; break;
        default:                s.status = "disconnected"; break;
    }
    s.ssid = _ssid;
    s.ip = _ip;
    s.rssi = _rssi;
    return s;
}

bool WifiModule::has_pending_event() {
    return _has_event;
}

String WifiModule::take_pending_event() {
    _has_event = false;
    String evt = _pending_event;
    _pending_event = "";
    return evt;
}

void WifiModule::set_state(State new_state) {
    if (_state == new_state) return;
    _state = new_state;

    // 生成事件
    Status st = get_status();
    StaticJsonDocument<256> data;
    data["status"] = st.status;
    data["ssid"] = st.ssid;
    data["ip"] = st.ip;
    data["rssi"] = st.rssi;

    _pending_event = ControlProtocol::make_event(PROTO_EVT_WIFI_STATUS_CHANGED, data.as<JsonVariantConst>());
    _has_event = true;

    LOG_INFO("WiFi模块", "状态变更: %s", st.status.c_str());
}

void WifiModule::on_wifi_event(WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_GOT_IP: {
            _ip = WiFi.localIP().toString();
            _rssi = WiFi.RSSI();
            _reconnect_attempts = 0;
            set_state(State::Connected);
            LOG_INFO("WiFi模块", "已连接, IP=%s, RSSI=%d", _ip.c_str(), _rssi);
            break;
        }
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
            _ip = "";
            if (_state == State::Connecting) {
                set_state(State::Failed);
                _last_reconnect_ms = millis();
            } else if (_state == State::Connected) {
                set_state(State::Failed);
                _reconnect_attempts = 0;
                _last_reconnect_ms = millis();
                LOG_WARN("WiFi模块", "连接断开, 将自动重连");
            }
            break;
        }
        default:
            break;
    }
}

void WifiModule::try_reconnect() {
    _reconnect_attempts++;
    _reconnect_interval_ms = min(_reconnect_interval_ms * 2, (unsigned long)60000);
    _last_reconnect_ms = millis();

    LOG_INFO("WiFi模块", "重连尝试 %d/%d", _reconnect_attempts, MAX_RECONNECT);
    set_state(State::Connecting);
    WiFi.disconnect(true);
    delay(100);
    WiFi.begin(_ssid.c_str(), _password.c_str());
}
