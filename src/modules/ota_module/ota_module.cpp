#include "ota_module.h"
#include "services/control_protocol.h"
#include "config/protocol_config.h"
#include <HTTPClient.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

bool OtaModule::begin() {
    LOG_INFO("OTA模块", "初始化");
    return true;
}

void OtaModule::loop() {
    // OTA 在 start() 中同步执行，loop 无需处理
}

bool OtaModule::start(const String& url, const String& version, const String& sha256) {
    if (WiFi.status() != WL_CONNECTED) {
        set_state(State::Failed, 0, "WiFi 未连接");
        return false;
    }
    if (_state == State::Downloading || _state == State::Installing) {
        set_state(State::Failed, 0, "OTA 进行中");
        return false;
    }

    _url = url;
    _version = version;
    _sha256 = sha256;
    _cancel_flag = false;

    LOG_INFO("OTA模块", "开始升级: url=%s ver=%s", url.c_str(), version.c_str());
    run_ota();
    return true;
}

void OtaModule::cancel() {
    _cancel_flag = true;
    LOG_INFO("OTA模块", "取消升级");
}

OtaModule::Status OtaModule::get_status() const {
    Status s;
    switch (_state) {
        case State::Downloading: s.status = "downloading"; break;
        case State::Installing:  s.status = "installing"; break;
        case State::Done:        s.status = "done"; break;
        case State::Failed:      s.status = "failed"; break;
        default:                 s.status = "idle"; break;
    }
    s.progress = _progress;
    s.message = _message;
    return s;
}

bool OtaModule::has_pending_event() {
    return _has_event;
}

String OtaModule::take_pending_event() {
    _has_event = false;
    String evt = _pending_event;
    _pending_event = "";
    return evt;
}

void OtaModule::set_state(State s, int progress, const String& msg) {
    _state = s;
    _progress = progress;
    _message = msg;

    StaticJsonDocument<256> data;
    data["status"] = get_status().status;
    data["progress"] = progress;
    data["message"] = msg;

    _pending_event = ControlProtocol::make_event(PROTO_EVT_OTA_PROGRESS, data.as<JsonVariantConst>());
    _has_event = true;

    LOG_INFO("OTA模块", "状态: %s %d%% %s", get_status().status.c_str(), progress, msg.c_str());
}

void OtaModule::run_ota() {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, _url);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setConnectTimeout(15000);
    http.setTimeout(15000);
    int httpCode = http.GET();

    if (httpCode != 200) {
        set_state(State::Failed, 0, "HTTP 错误: " + String(httpCode));
        http.end();
        return;
    }

    int totalSize = http.getSize();
    if (totalSize <= 0) {
        set_state(State::Failed, 0, "无法获取固件大小");
        http.end();
        return;
    }

    set_state(State::Downloading, 0, "开始下载...");

    if (!Update.begin(totalSize)) {
        set_state(State::Failed, 0, "Update.begin 失败");
        http.end();
        return;
    }

    WiFiClient* stream = http.getStreamPtr();
    static uint8_t buf[2048];
    int written = 0;

    while (http.connected() && written < totalSize) {
        if (_cancel_flag) {
            Update.abort();
            set_state(State::Failed, 0, "用户取消");
            http.end();
            return;
        }

        size_t available = stream->available();
        if (available) {
            int bytesRead = stream->readBytes(buf, min((size_t)sizeof(buf), available));
            size_t bytesWritten = Update.write(buf, bytesRead);
            if (bytesWritten != (size_t)bytesRead) {
                set_state(State::Failed, 0, "写入失败");
                Update.abort();
                http.end();
                return;
            }
            written += bytesRead;

            int pct = (written * 100) / totalSize;
            if (pct != _progress) {
                _progress = pct;
                // 每 1% 发一次事件
                set_state(State::Downloading, pct, "下载中...");
            }
        }
        delay(1);
    }

    http.end();

    if (written != totalSize) {
        set_state(State::Failed, written * 100 / totalSize, "下载不完整");
        Update.abort();
        return;
    }

    set_state(State::Installing, 100, "正在安装...");

    if (!_sha256.isEmpty()) {
        if (!Update.setMD5(_sha256.c_str())) {
            LOG_WARN("OTA模块", "SHA256 设置失败，跳过校验");
        }
    }

    if (Update.end(true)) {
        set_state(State::Done, 100, "升级完成，即将重启");
        LOG_INFO("OTA模块", "升级成功，3秒后重启");
        delay(3000);
        ESP.restart();
    } else {
        set_state(State::Failed, 100, "Update.end 失败: " + String(Update.errorString()));
    }
}
