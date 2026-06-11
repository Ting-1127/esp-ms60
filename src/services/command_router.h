#pragma once

/**
 * @file    command_router.h
 * @brief   APP 控制命令路由
 */

#include "project.h"
#include "services/control_protocol.h"

class WifiModule;
class OtaModule;
class RadarModule;

struct RuntimeStatus {
    bool ble_connected;
    WifiModule* wifi;
    OtaModule* ota;
    RadarModule* radar;
};

class CommandRouter {
public:
    static String handle(const ControlRequest& request, const RuntimeStatus& status);

private:
    static String handle_sys_info(const ControlRequest& request);
    static String handle_diag_status(const ControlRequest& request, const RuntimeStatus& status);
    static String handle_sys_reboot(const ControlRequest& request);

    static String handle_wifi_on(const ControlRequest& request, const RuntimeStatus& status);
    static String handle_wifi_off(const ControlRequest& request, const RuntimeStatus& status);
    static String handle_wifi_set(const ControlRequest& request, const RuntimeStatus& status);
    static String handle_wifi_connect(const ControlRequest& request, const RuntimeStatus& status);
    static String handle_wifi_disconnect(const ControlRequest& request, const RuntimeStatus& status);
    static String handle_wifi_status(const ControlRequest& request, const RuntimeStatus& status);

    static String handle_ota_check(const ControlRequest& request);
    static String handle_ota_start(const ControlRequest& request, const RuntimeStatus& status);
    static String handle_ota_status(const ControlRequest& request, const RuntimeStatus& status);
    static String handle_ota_cancel(const ControlRequest& request, const RuntimeStatus& status);

    static String handle_bsd_status(const ControlRequest& request, const RuntimeStatus& status);
    static String handle_bsd_config(const ControlRequest& request, const RuntimeStatus& status);
    static String handle_bsd_config_get(const ControlRequest& request, const RuntimeStatus& status);
};
