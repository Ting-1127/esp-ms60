#include "command_router.h"
#include "modules/wifi_module/wifi_module.h"
#include "modules/ota_module/ota_module.h"
#include "modules/radar_module/radar_module.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>

String CommandRouter::handle(const ControlRequest& request, const RuntimeStatus& status) {
    if (request.cmd == PROTO_CMD_SYS_INFO)        return handle_sys_info(request);
    if (request.cmd == PROTO_CMD_DIAG_STATUS)      return handle_diag_status(request, status);
    if (request.cmd == PROTO_CMD_SYS_REBOOT)       return handle_sys_reboot(request);

    // WiFi
    if (request.cmd == PROTO_CMD_WIFI_ON)           return handle_wifi_on(request, status);
    if (request.cmd == PROTO_CMD_WIFI_OFF)          return handle_wifi_off(request, status);
    if (request.cmd == PROTO_CMD_WIFI_SET)         return handle_wifi_set(request, status);
    if (request.cmd == PROTO_CMD_WIFI_CONNECT)     return handle_wifi_connect(request, status);
    if (request.cmd == PROTO_CMD_WIFI_DISCONNECT)  return handle_wifi_disconnect(request, status);
    if (request.cmd == PROTO_CMD_WIFI_STATUS)      return handle_wifi_status(request, status);

    // OTA
    if (request.cmd == PROTO_CMD_OTA_CHECK)        return handle_ota_check(request);
    if (request.cmd == PROTO_CMD_OTA_START)        return handle_ota_start(request, status);
    if (request.cmd == PROTO_CMD_OTA_STATUS)       return handle_ota_status(request, status);
    if (request.cmd == PROTO_CMD_OTA_CANCEL)       return handle_ota_cancel(request, status);

    // BSD
    if (request.cmd == PROTO_CMD_BSD_STATUS)       return handle_bsd_status(request, status);
    if (request.cmd == PROTO_CMD_BSD_CONFIG)       return handle_bsd_config(request, status);
    if (request.cmd == PROTO_CMD_BSD_CONFIG_GET)   return handle_bsd_config_get(request, status);

    return ControlProtocol::make_empty_response(request.id,
                                                false,
                                                PROTO_CODE_UNKNOWN_CMD,
                                                "unknown command");
}

String CommandRouter::handle_sys_info(const ControlRequest& request) {
    StaticJsonDocument<256> data;
    data["fw"] = FW_NAME;
    data["version"] = FW_VERSION;
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

String CommandRouter::handle_wifi_on(const ControlRequest& request, const RuntimeStatus& status) {
    if (!status.wifi) {
        return ControlProtocol::make_empty_response(request.id, false, PROTO_CODE_INTERNAL_ERROR, "WiFi module unavailable");
    }
    status.wifi->wifi_on();
    return ControlProtocol::make_empty_response(request.id, true, PROTO_CODE_OK, "wifi on");
}

String CommandRouter::handle_wifi_off(const ControlRequest& request, const RuntimeStatus& status) {
    if (!status.wifi) {
        return ControlProtocol::make_empty_response(request.id, false, PROTO_CODE_INTERNAL_ERROR, "WiFi module unavailable");
    }
    status.wifi->wifi_off();
    return ControlProtocol::make_empty_response(request.id, true, PROTO_CODE_OK, "wifi off");
}

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
    data["enabled"] = st.enabled;
    data["status"] = st.status;
    data["ssid"] = st.ssid;
    data["ip"] = st.ip;
    data["rssi"] = st.rssi;
    return ControlProtocol::make_response(request.id, true, PROTO_CODE_OK, "OK", data.as<JsonVariantConst>());
}

// ─── OTA ────────────────────────────────────────────────────────────

String CommandRouter::handle_ota_check(const ControlRequest& request) {
    StaticJsonDocument<512> data;
    data["current_version"] = FW_VERSION;

    if (WiFi.status() != WL_CONNECTED) {
        data["update_available"] = false;
        data["message"] = "WiFi 未连接";
        return ControlProtocol::make_response(request.id, true, PROTO_CODE_OK, "OK", data.as<JsonVariantConst>());
    }

    // 查询 GitHub Releases API
    HTTPClient http;
    String url = String("https://api.github.com/repos/") + OTA_GITHUB_OWNER + "/" + OTA_GITHUB_REPO + "/releases/latest";
    http.begin(url);
    http.addHeader("User-Agent", FW_NAME);
    http.setConnectTimeout(10000);
    http.setTimeout(10000);

    int code = http.GET();
    if (code == 200) {
        DynamicJsonDocument resp(4096);
        DeserializationError err = deserializeJson(resp, http.getString());
        if (!err) {
            const char* tag = resp["tag_name"];
            // 查找固件下载 URL
            const char* download_url = nullptr;
            JsonArray assets = resp["assets"];
            for (JsonObject asset : assets) {
                const char* name = asset["name"];
                if (name && strcmp(name, OTA_FIRMWARE_FILE) == 0) {
                    download_url = asset["browser_download_url"];
                    break;
                }
            }

            bool update_available = false;
            if (tag) {
                String remote = tag;
                if (remote.startsWith("v")) remote.remove(0, 1);
                update_available = (remote != FW_VERSION);
                data["latest_version"] = tag;
            }

            data["update_available"] = update_available;
            if (update_available && download_url) {
                data["url"] = download_url;
            }
        } else {
            data["update_available"] = false;
            data["message"] = "parse error";
        }
    } else {
        data["update_available"] = false;
        data["message"] = String("HTTP ") + code;
    }
    http.end();

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

// ─── BSD ────────────────────────────────────────────────────────────

String CommandRouter::handle_bsd_status(const ControlRequest& request, const RuntimeStatus& status) {
    if (!status.radar) {
        return ControlProtocol::make_empty_response(request.id, false, PROTO_CODE_INTERNAL_ERROR, "Radar module unavailable");
    }
    StaticJsonDocument<256> data;
    data["radar_ok"] = status.radar->is_radar_ok();
    data["target_count"] = status.radar->get_target_count();
    data["last_update_ms"] = status.radar->get_last_update_ms();

    JsonObject left = data.createNestedObject("left");
    left["level"] = (uint8_t)status.radar->get_zone_level(BSD_ZONE_LEFT);
    left["lca_level"] = (uint8_t)status.radar->get_zone_lca_level(BSD_ZONE_LEFT);

    JsonObject right = data.createNestedObject("right");
    right["level"] = (uint8_t)status.radar->get_zone_level(BSD_ZONE_RIGHT);
    right["lca_level"] = (uint8_t)status.radar->get_zone_lca_level(BSD_ZONE_RIGHT);

    JsonObject rear = data.createNestedObject("rear");
    rear["level"] = (uint8_t)status.radar->get_zone_level(BSD_ZONE_REAR);

    return ControlProtocol::make_response(request.id, true, PROTO_CODE_OK, "OK", data.as<JsonVariantConst>());
}

static void apply_bsd_config_field(const char* key, JsonVariantConst val, BsdConfig& cfg) {
    if (!key) return;
    if (strcmp(key, "level1_dist") == 0)      cfg.level1_dist = val.as<int32_t>();
    else if (strcmp(key, "level2_dist") == 0)  cfg.level2_dist = val.as<int32_t>();
    else if (strcmp(key, "level3_dist") == 0)  cfg.level3_dist = val.as<int32_t>();
    else if (strcmp(key, "rcw_half_width") == 0) cfg.rcw_half_width = val.as<int32_t>();
    else if (strcmp(key, "lca_outer_edge") == 0) cfg.lca_outer_edge = val.as<int32_t>();
    else if (strcmp(key, "angle_dead_zone") == 0) cfg.angle_dead_zone = val.as<int8_t>();
    else if (strcmp(key, "ttc_threshold") == 0) cfg.ttc_threshold = val.as<uint8_t>();
    else if (strcmp(key, "sensitivity") == 0)   cfg.sensitivity = val.as<uint8_t>();
}

static void bsd_config_to_json(StaticJsonDocument<256>& data, const BsdConfig& cfg) {
    data["level1_dist"] = cfg.level1_dist;
    data["level2_dist"] = cfg.level2_dist;
    data["level3_dist"] = cfg.level3_dist;
    data["rcw_half_width"] = cfg.rcw_half_width;
    data["lca_outer_edge"] = cfg.lca_outer_edge;
    data["angle_dead_zone"] = cfg.angle_dead_zone;
    data["ttc_threshold"] = cfg.ttc_threshold;
    data["sensitivity"] = cfg.sensitivity;
}

String CommandRouter::handle_bsd_config(const ControlRequest& request, const RuntimeStatus& status) {
    if (!status.radar) {
        return ControlProtocol::make_empty_response(request.id, false, PROTO_CODE_INTERNAL_ERROR, "Radar module unavailable");
    }
    BsdConfig cfg = status.radar->get_config();

    // 遍历 payload 中的字段逐个应用
    JsonObjectConst payload = request.payload.as<JsonObjectConst>();
    for (JsonPairConst kv : payload) {
        apply_bsd_config_field(kv.key().c_str(), kv.value(), cfg);
    }

    status.radar->set_config(cfg);

    StaticJsonDocument<256> data;
    bsd_config_to_json(data, status.radar->get_config());
    return ControlProtocol::make_response(request.id, true, PROTO_CODE_OK, "OK", data.as<JsonVariantConst>());
}

String CommandRouter::handle_bsd_config_get(const ControlRequest& request, const RuntimeStatus& status) {
    if (!status.radar) {
        return ControlProtocol::make_empty_response(request.id, false, PROTO_CODE_INTERNAL_ERROR, "Radar module unavailable");
    }
    StaticJsonDocument<256> data;
    bsd_config_to_json(data, status.radar->get_config());
    return ControlProtocol::make_response(request.id, true, PROTO_CODE_OK, "OK", data.as<JsonVariantConst>());
}
