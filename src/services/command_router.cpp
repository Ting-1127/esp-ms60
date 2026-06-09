#include "command_router.h"

String CommandRouter::handle(const ControlRequest& request, const RuntimeStatus& status) {
    if (request.cmd == PROTO_CMD_SYS_INFO) {
        return handle_sys_info(request);
    }

    if (request.cmd == PROTO_CMD_DIAG_STATUS) {
        return handle_diag_status(request, status);
    }

    if (request.cmd == PROTO_CMD_SYS_REBOOT) {
        return handle_sys_reboot(request);
    }

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
