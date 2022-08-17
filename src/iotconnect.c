/*
 * FreeRTOS V202107.00
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
//
// Copyright: Avnet 2022
// Created by Nik Markovic <nikola.markovic@avnet.com> on 6/15/22.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"

#include "iotc_device_client.h"

//
// Copyright: Avnet, Softweb Inc. 2021
// Modified by Nik Markovic <nikola.markovic@avnet.com> on 6/24/21.
//

#include "iotc_device_client.h"
#include "iotconnect_sync.h"
#include "iotconnect.h"

static IotclConfig lib_config = { 0 };
static IotConnectClientConfig config = { 0 };

static void on_mqtt_c2d_message(unsigned char* message, size_t message_len) {
    char* str = malloc(message_len + 1);
    memcpy(str, message, message_len);
    str[message_len] = 0;
    printf("event>>> %s\n", str);
    if (!iotcl_process_event(str)) {
        fprintf(stderr, "Error encountered while processing %s\n", str);
    }
    free(str);
}

bool iotconnect_sdk_is_connected() {
    return iotc_device_client_is_connected();
}

IotConnectClientConfig* iotconnect_sdk_init_and_get_config() {
    memset(&config, 0, sizeof(config));
    return &config;
}

IotclConfig* iotconnect_sdk_get_lib_config() {
    return iotcl_get_config();
}

static void on_message_intercept(IotclEventData data, IotConnectEventType type) {
    switch (type) {
    case ON_FORCE_SYNC:
        printf("Got a SYNC request request.\n");
        break;
    case ON_CLOSE:
        printf("Got a disconnect request.\n");
        break;
    default:
        break; // not handling nay other messages
    }

    if (NULL != config.msg_cb) {
        config.msg_cb(data, type);
    }
}

int iotconnect_sdk_send_packet(const char* data) {
    return iotc_device_client_send_message(data);
}



///////////////////////////////////////////////////////////////////////////////////
// this the Initialization os IoTConnect SDK
int iotconnect_sdk_init() {
    int ret;

    // TODO: Sort sync out
    // iotc_sync_obtain_response();

    // We want to print only first 4 characters of cpid
    lib_config.device.env = config.env;
    lib_config.device.cpid = config.cpid;
    lib_config.device.duid = config.duid;

    if (!config.env || !config.cpid || !config.duid) {
        printf("Error: Device configuration is invalid. Configuration values for env, cpid and duid are required.\n");
        return -1;
    }

    lib_config.event_functions.ota_cb = config.ota_cb;
    lib_config.event_functions.cmd_cb = config.cmd_cb;
    lib_config.event_functions.msg_cb = on_message_intercept;

    lib_config.telemetry.dtg = iotc_sync_get_dtg();

    char cpid_buff[5];
    strncpy(cpid_buff, config.cpid, 4);
    cpid_buff[4] = 0;
    printf("CPID: %s***\n", cpid_buff);
    printf("ENV:  %s\n", config.env);

    if (!iotcl_init(&lib_config)) {
        fprintf(stderr, "Error: Failed to initialize the IoTConnect Lib\n");
        return -1;
    }

    IotConnectDeviceClientConfig pc;

    pc.c2d_msg_cb = on_mqtt_c2d_message;

    ret = iotc_device_client_init(&pc);
    if (ret) {
        fprintf(stderr, "Failed to connect!\n");
        return ret;
    }

    return ret;
}
