//
// Copyright: Avnet 2022
// Created by Nik Markovic <nikola.markovic@avnet.com> on 6/15/22.
//

#ifndef IOTC_DEVICE_CLIENT_H
#define IOTC_DEVICE_CLIENT_H

#include "stdbool.h"
#include "iotconnect_discovery.h"

#ifdef __cplusplus
extern   "C" {
#endif


typedef void (*IotConnectC2dCallback)(const char* message, size_t message_len);

typedef struct {
    IotConnectC2dCallback c2d_msg_cb; // callback for inbound messages
} IotConnectDeviceClientConfig;

int iotc_device_client_init(IotConnectDeviceClientConfig *c);

// NOTE: Currently not supported
int iotc_device_client_disconnect();

bool iotc_device_client_is_connected();

int iotc_device_client_send_message(const char *message);

#ifdef __cplusplus
}
#endif

#endif // IOTC_DEVICE_CLIENT_H
