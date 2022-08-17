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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"



#include "core_mqtt.h"
#include "core_mqtt_agent.h"
#include "subscription_manager.h"
#include "mqtt_agent_task.h"

#include "iotconnect_sync.h"
#include "iotc_device_client.h"

#define MQTT_PUBLISH_BLOCK_TIME_MS           ( 200 )
#define MQTT_NOTIFY_IDX                      ( 1 )
#define MQTT_PUBLISH_NOTIFICATION_WAIT_MS    ( 1000 )
#define MQTT_PUBLISH_QOS                     ( MQTTQoS1 )


/*-----------------------------------------------------------*/
typedef struct MQTTAgentCommandContext
{
    /**
     * @brief The handle of this task. It is used by callbacks to notify this task.
     */
    TaskHandle_t xShadowDeviceTaskHandle;

    MQTTAgentHandle_t xAgentHandle;
} ShadowDeviceCtx_t;


static IotConnectC2dCallback c2d_msg_cb = NULL; // callback for inbound messages
static MQTTAgentHandle_t xAgentHandle = NULL;
static bool is_initialized = false;

static void devicebound_event_callback( void * pvCtx, MQTTPublishInfo_t * pxPublishInfo ) {

	(void)pvCtx;

    configASSERT( pxPublishInfo != NULL );
    configASSERT( pxPublishInfo->pPayload != NULL );

    LogInfo( "Inbound message:%.*s.",
              pxPublishInfo->payloadLength,
              ( const char * ) pxPublishInfo->pPayload );

    if (c2d_msg_cb) {
    	c2d_msg_cb(( const char * ) pxPublishInfo->pPayload, pxPublishInfo->payloadLength);
    }

}

static bool subscribe_to_devicebound_topic() {
    MQTTStatus_t xStatus = MQTTSuccess;

    xStatus = MqttAgent_SubscribeSync( xAgentHandle,
                                       iotc_sync_get_sub_topic(),
                                       MQTTQoS1,
                                       devicebound_event_callback,
                                       NULL );

    if( xStatus != MQTTSuccess )
    {
        LogError( "Failed to subscribe to topic: %s", iotc_sync_get_sub_topic());
        return pdFALSE;
    }

    return pdTRUE;
}

static void prvPublishCommandCallback( MQTTAgentCommandContext_t * pxCommandContext,
	MQTTAgentReturnInfo_t * pxReturnInfo
	)
{
    TaskHandle_t xTaskHandle = ( TaskHandle_t ) pxCommandContext;

    configASSERT( pxReturnInfo != NULL );

    uint32_t ulNotifyValue = pxReturnInfo->returnCode;

    if( xTaskHandle != NULL )
    {
        /* Send the context's ulNotificationValue as the notification value so
         * the receiving task can check the value it set in the context matches
         * the value it receives in the notification. */
        ( void ) xTaskNotifyIndexed( xTaskHandle,
                                     MQTT_NOTIFY_IDX,
                                     ulNotifyValue,
                                     eSetValueWithOverwrite );
    }
}


static BaseType_t prvPublishAndWaitForAck(const char * pcTopic,
	const void * pvPublishData,
	size_t xPublishDataLen
	)
{
    MQTTStatus_t xStatus;
    size_t uxTopicLen = 0;

    configASSERT( pcTopic != NULL );
    configASSERT( pvPublishData != NULL );
    configASSERT( xPublishDataLen > 0 );

    uxTopicLen = strnlen( pcTopic, UINT16_MAX );

    MQTTPublishInfo_t xPublishInfo =
    {
        .qos             = MQTT_PUBLISH_QOS,
        .retain          = 0,
        .dup             = 0,
        .pTopicName      = pcTopic,
        .topicNameLength = ( uint16_t ) uxTopicLen,
        .pPayload        = pvPublishData,
        .payloadLength   = xPublishDataLen
    };

    MQTTAgentCommandInfo_t xCommandParams =
    {
        .blockTimeMs                 = MQTT_PUBLISH_BLOCK_TIME_MS,
        .cmdCompleteCallback         = prvPublishCommandCallback,
        .pCmdCompleteCallbackContext = ( void * ) xTaskGetCurrentTaskHandle(),
    };

    if( xPublishInfo.qos > MQTTQoS0 )
    {
        xCommandParams.pCmdCompleteCallbackContext = ( void * ) xTaskGetCurrentTaskHandle();
    }

    /* Clear the notification index */
    xTaskNotifyStateClearIndexed( NULL, MQTT_NOTIFY_IDX );


    xStatus = MQTTAgent_Publish( xAgentHandle,
                                 &xPublishInfo,
                                 &xCommandParams );

    if( xStatus == MQTTSuccess )
    {
        uint32_t ulNotifyValue = 0;
        BaseType_t xResult = pdFALSE;

        xResult = xTaskNotifyWaitIndexed( MQTT_NOTIFY_IDX,
                                          0xFFFFFFFF,
                                          0xFFFFFFFF,
                                          &ulNotifyValue,
                                          pdMS_TO_TICKS( MQTT_PUBLISH_NOTIFICATION_WAIT_MS ) );

        if( xResult )
        {
            xStatus = ( MQTTStatus_t ) ulNotifyValue;

            if( xStatus != MQTTSuccess )
            {
                LogError( "MQTT Agent returned error code: %d during publish operation.",
                          xStatus );
                xResult = pdFALSE;
            }
        }
        else
        {
            LogError( "Timed out while waiting for publish ACK or Sent event. xTimeout = %d",
                      pdMS_TO_TICKS( MQTT_PUBLISH_NOTIFICATION_WAIT_MS ) );
            xResult = pdFALSE;
        }
    }
    else
    {
        LogError( "MQTTAgent_Publish returned error code: %d.", xStatus );
    }

    return( xStatus == MQTTSuccess );
}

int iotc_device_client_disconnect() {
	LogError(("MQTT Disconnect is not supported at this time"));
    return EXIT_FAILURE;
}

bool iotc_device_client_is_connected() {
    return xIsMqttAgentConnected();
}

int iotc_device_client_send_message(const char* message) {
    BaseType_t xResult = pdFALSE;

    xResult = prvPublishAndWaitForAck(
	   iotc_sync_get_pub_topic(),
	   message,
	   ( size_t ) strlen(message)
	   );

    if( xResult != pdPASS )
    {
        LogError( "Failed to publish message %s", message);
    }


    return (xResult == pdPASS ? EXIT_SUCCESS : EXIT_FAILURE);
}

#if 0
void iotc_device_client_loop(unsigned int timeout_ms) {
    BaseType_t ret = ProcessLoop(& xMqttContext, (uint32_t) timeout_ms);
    bool connected = xMqttContext.connectStatus == MQTTConnected;
    if (pdPASS != ret) {
        LogError(("Received an error from ProcessLoop! Connection status: %s", connected ? "CONNECTED" : "DISCONNECTED"));      
    }
    if (is_connected && !connected) {
        if (status_cb) {
            status_cb(IOTC_CS_MQTT_CONNECTED);
        }
        is_connected = false;
    }
}
#endif

int iotc_device_client_init(IotConnectDeviceClientConfig* c) {

    c2d_msg_cb = NULL;

    if (is_initialized) {
		LogWarn(("WARN: iotc_device_client_init should not be called twice. Disconnect is not supported, so initialization is partial..."));
    }
    is_initialized = true;

    /* Wait for MqttAgent to be ready. */
    vSleepUntilMQTTAgentReady();

    xAgentHandle = xGetMqttAgentHandle();

    if (!subscribe_to_devicebound_topic()) {
		LogWarn(("iotc_device_client_init: Unable to subscribe to devicebound messages topic."));
        return EXIT_FAILURE;
    }

    c2d_msg_cb = c->c2d_msg_cb;

    return EXIT_SUCCESS;
}


