#pragma once

/**
 * @file    command_router.h
 * @brief   APP 控制命令路由
 */

#include "project.h"
#include "services/control_protocol.h"

struct RuntimeStatus {
    bool ble_connected;
};

class CommandRouter {
public:
    static String handle(const ControlRequest& request, const RuntimeStatus& status);

private:
    static String handle_sys_info(const ControlRequest& request);
    static String handle_diag_status(const ControlRequest& request, const RuntimeStatus& status);
    static String handle_sys_reboot(const ControlRequest& request);
};
