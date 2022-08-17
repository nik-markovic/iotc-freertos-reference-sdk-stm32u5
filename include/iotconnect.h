//
// Copyright: Avnet, Softweb Inc. 2020
// Modified by Nik Markovic <nikola.markovic@avnet.com> on 6/15/20.
//

#ifndef IOTCONNECT_H
#define IOTCONNECT_H

#include <stddef.h>

#include "iotconnect_event.h"
#include "iotconnect_telemetry.h"
#include "iotconnect_lib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char *env;    // Environment name. Contact your representative for details.
    char *cpid;   // Settings -> Company Profile.
    char *duid;   // Name of the device.
    IotclOtaCallback ota_cb; // callback for OTA events.
    IotclCommandCallback cmd_cb; // callback for command events.
    IotclMessageCallback msg_cb; // callback for ALL messages, including the specific ones like cmd or ota callback.
} IotConnectClientConfig;

IotConnectClientConfig *iotconnect_sdk_init_and_get_config();

int iotconnect_sdk_init();

bool iotconnect_sdk_is_connected();

IotclConfig *iotconnect_sdk_get_lib_config();

int iotconnect_sdk_send_packet(const char *data);

#ifdef __cplusplus
}
#endif

#endif
