#include "command_router.h"
#include "modules/wifi_module/wifi_module.h"
#include "modules/ota_module/ota_module.h"

String CommandRouter::handle(const ControlRequest& request, const RuntimeStatus& status) {
    if (request.cmd == PROTO_CMD_SYS_INFO)        return handle_sys_info(request);
    if (request.cmd == PROTO_CMD_DIAG_STATUS)      return handle_diag_status(request, status);
    if (request.cmd == PROTO_CMD_SYS_REBOOT)       return handle_sys_reboot(request);

    // WiFi
    if (request.cmd == PROTO_CMD_WIFI_SET)         return handle_wifi_set(request, status);
    if (request.cmd == PROTO_CMD_WIFI_CONNECT)     return handle_wifi_connect(request, status);
    if (request.cmd == PROTO_CMD_WIFI_DISCONNECT)  return handle_wifi_disconnect(request, status);
    if (request.cmd == PROTO_CMD_WIFI_STATUS)      return handle_wifi_status(request, status);

    // OTA
    if (request.cmd == PROTO_CMD_OTA_CHECK)        return handle_ota_check(request);
    if (request.cmd == PROTO_CMD_OTA_START)        return handle_ota_start(request, status);
    if (request.cmd == PROTO_CMD_OTA_STATUS)       return handle_ota_status(request, status);
    if (request.cmd == PROTO_CMD_OTA_CANCEL)       return handle_ota_cancel(request, status);

    return ControlProtocol::make_empty_response(request.id,
                                                false,
                                                PROTO_CODE_UNKNOWN_CMD,
                                                "unknown command");
}

String CommandRouter::handle_sys_info(const ControlRequest& request) {
    StaticJsonDocument<256> data;
    data["fw"] = "esp-ms60";
    data["proto"] = PROTO_VERSION;
    data["chip"] = ESP.getChipModel();
    data["flash_mb"] = (uint32_t)(ESP.getFlashChipSize() / (1024 * 1024));
    data["psram_mb"] = (uint32_t)(ESP.getPsramSize() / (1024 * 1024));
    data["uptime_ms"] = millis();

    return ControlProtocol::make_response(request.id,
                                          true,
                                          PROTO_CODE_OK,
                                          "OK",
                                          data.as<JsonVariantConst>());
}

String CommandRouter::handle_diag_status(const ControlRequest& request, const RuntimeStatus& status) {
    StaticJsonDocument<256> data;
    data["heap_free"] = ESP.getFreeHeap();
    data["heap_min"] = ESP.getMinFreeHeap();
    data["psram_free"] = ESP.getFreePsram();
    data["uptime_ms"] = millis();
    data["ble_connected"] = status.ble_connected;

    return ControlProtocol::make_response(request.id,
                                          true,
                                          PROTO_CODE_OK,
                                          "OK",
                                          data.as<JsonVariantConst>());
}

String CommandRouter::handle_sys_reboot(const ControlRequest& request) {
    return ControlProtocol::make_empty_response(request.id,
                                                false,
                                                PROTO_CODE_NOT_IMPLEMENTED,
                                                "reboot requires authorization support");
}

// ─── WiFi ───────────────────────────────────────────────────────────

String CommandRouter::handle_wifi_set(const ControlRequest& request, const RuntimeStatus& status) {
    if (!status.wifi) {
        return ControlProtocol::make_empty_response(request.id, false, PROTO_CODE_INTERNAL_ERROR, "WiFi module unavailable");
    }
    const char* ssid = request.payload["ssid"];
    const char* password = request.payload["password"];
    if (!ssid || ssid[0] == '\0') {
        return ControlProtocol::make_empty_response(request.id, false, PROTO_CODE_BAD_JSON, "ssid required");
    }
    bool ok = status.wifi->set_credentials(ssid, password ? password : "");
    if (ok) {
        return ControlProtocol::make_empty_response(request.id, true, PROTO_CODE_OK, "credentials saved");
    }
    return ControlProtocol::make_empty_response(request.id, false, PROTO_CODE_INTERNAL_ERROR, "save failed");
}

String CommandRouter::handle_wifi_connect(const ControlRequest& request, const RuntimeStatus& status) {
    if (!status.wifi) {
        return ControlProtocol::make_empty_response(request.id, false, PROTO_CODE_INTERNAL_ERROR, "WiFi module unavailable");
    }
    bool ok = status.wifi->connect();
    return ControlProtocol::make_empty_response(request.id, ok, ok ? PROTO_CODE_OK : PROTO_CODE_INTERNAL_ERROR,
                                                ok ? "connecting" : "no credentials");
}

String CommandRouter::handle_wifi_disconnect(const ControlRequest& request, const RuntimeStatus& status) {
    if (!status.wifi) {
        return ControlProtocol::make_empty_response(request.id, false, PROTO_CODE_INTERNAL_ERROR, "WiFi module unavailable");
    }
    status.wifi->disconnect();
    return ControlProtocol::make_empty_response(request.id, true, PROTO_CODE_OK, "disconnected");
}

String CommandRouter::handle_wifi_status(const ControlRequest& request, const RuntimeStatus& status) {
    if (!status.wifi) {
        return ControlProtocol::make_empty_response(request.id, false, PROTO_CODE_INTERNAL_ERROR, "WiFi module unavailable");
    }
    auto st = status.wifi->get_status();
    StaticJsonDocument<256> data;
    data["status"] = st.status;
    data["ssid"] = st.ssid;
    data["ip"] = st.ip;
    data["rssi"] = st.rssi;
    return ControlProtocol::make_response(request.id, true, PROTO_CODE_OK, "OK", data.as<JsonVariantConst>());
}

// ─── OTA ────────────────────────────────────────────────────────────

String CommandRouter::handle_ota_check(const ControlRequest& request) {
    StaticJsonDocument<256> data;
    data["current_version"] = FW_VERSION;
    data["update_available"] = false;
    return ControlProtocol::make_response(request.id, true, PROTO_CODE_OK, "OK", data.as<JsonVariantConst>());
}

String CommandRouter::handle_ota_start(const ControlRequest& request, const RuntimeStatus& status) {
    if (!status.ota) {
        return ControlProtocol::make_empty_response(request.id, false, PROTO_CODE_INTERNAL_ERROR, "OTA module unavailable");
    }
    const char* url = request.payload["url"];
    if (!url || url[0] == '\0') {
        return ControlProtocol::make_empty_response(request.id, false, PROTO_CODE_BAD_JSON, "url required");
    }
    const char* version = request.payload["version"];
    const char* sha256 = request.payload["sha256"];
    bool ok = status.ota->start(url, version ? version : "", sha256 ? sha256 : "");
    return ControlProtocol::make_empty_response(request.id, ok, ok ? PROTO_CODE_OK : PROTO_CODE_INTERNAL_ERROR,
                                                ok ? "ota started" : "ota start failed");
}

String CommandRouter::handle_ota_status(const ControlRequest& request, const RuntimeStatus& status) {
    if (!status.ota) {
        return ControlProtocol::make_empty_response(request.id, false, PROTO_CODE_INTERNAL_ERROR, "OTA module unavailable");
    }
    auto st = status.ota->get_status();
    StaticJsonDocument<256> data;
    data["status"] = st.status;
    data["progress"] = st.progress;
    data["message"] = st.message;
    return ControlProtocol::make_response(request.id, true, PROTO_CODE_OK, "OK", data.as<JsonVariantConst>());
}

String CommandRouter::handle_ota_cancel(const ControlRequest& request, const RuntimeStatus& status) {
    if (!status.ota) {
        return ControlProtocol::make_empty_response(request.id, false, PROTO_CODE_INTERNAL_ERROR, "OTA module unavailable");
    }
    status.ota->cancel();
    return ControlProtocol::make_empty_response(request.id, true, PROTO_CODE_OK, "ota cancelled");
}
