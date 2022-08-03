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
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 */

  /**
   * @file http_demo_s3_download.c
   * @brief Demonstrates usage of the HTTP library.
   */
//
// Copyright: Avnet 2022
// Created by Nik Markovic <nikola.markovic@avnet.com> on 6/15/22.
//

   /* Standard includes. */
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Include config as the first non-system header. */
#include "app_config.h"


/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

#include "sys_evt.h"
#include "mbedtls_transport.h"
#include "backoff_algorithm.h"
#include "core_http_client.h"

#include "iotconnect_certs.h"

#include "iotc_http_request.h"

/*------------- Demo configurations -------------------------*/


/* Check that a transport timeout for the transport send and receive functions
 * is defined. */
#ifndef IOTC_HTTP_CLIENT_SEND_RECV_TIMEOUT_MS
#define IOTC_HTTP_CLIENT_SEND_RECV_TIMEOUT_MS    ( 5000 )
#endif

 /* Check that the size of the user buffer is defined. */
#ifndef IOTC_HTTP_CLIENT_USER_BUFFER_SIZE
#define IOTC_HTTP_CLIENT_USER_BUFFER_SIZE    ( 4096 )
#endif

#define CONNECTION_RETRY_MAX_ATTEMPTS            ( 5U )
#define CONNECTION_RETRY_MAX_BACKOFF_DELAY_MS    ( 5000U )
#define CONNECTION_RETRY_BACKOFF_BASE_MS         ( 500U )


// The number of times to try the whole HTTP reqyuest with backoff
#ifndef MAX_HTTP_REQUEST_TRIES
#define MAX_HTTP_REQUEST_TRIES    ( 3 )
#endif

// The number of milliseconds to backof between HTTP request failures
#define HTTP_REQUEST_BACKOFF_MS    ( pdMS_TO_TICKS( 2000U ) )


/*-----------------------------------------------------------*/

/**
 * @brief A buffer used in the demo for storing HTTP request headers, and HTTP
 * response headers and body.
 *
 * @note This demo shows how the same buffer can be re-used for storing the HTTP
 * response after the HTTP request is sent out. However, the user can decide how
 * to use buffers to store HTTP requests and responses.
 */
static uint8_t httpClientBuffer[IOTC_HTTP_CLIENT_USER_BUFFER_SIZE];

/**
 * @brief Represents header data that will be sent in an HTTP request.
 */
static HTTPRequestHeaders_t requestHeaders;

/**
 * @brief Configurations of the initial request headers that are passed to
 * #HTTPClient_InitializeRequestHeaders.
 */
static HTTPRequestInfo_t requestInfo;

/**
 * @brief Represents a response returned from an HTTP server.
 */
static HTTPResponse_t response;

typedef BaseType_t(*TransportConnect_t)(NetworkContext_t* pxNetworkContext, IotConnectHttpRequest* request);

static BaseType_t prvBackoffForRetry(BackoffAlgorithmContext_t* pxRetryParams)
{
    BaseType_t xReturnStatus = pdFAIL;
    uint16_t usNextRetryBackOff = 0U;
    BackoffAlgorithmStatus_t xBackoffAlgStatus = BackoffAlgorithmSuccess;

    extern UBaseType_t uxRand( void );

	/* Get back-off value (in milliseconds) for the next retry attempt. */
	xBackoffAlgStatus = BackoffAlgorithm_GetNextBackoff(pxRetryParams, uxRand(), &usNextRetryBackOff);

	if (xBackoffAlgStatus == BackoffAlgorithmRetriesExhausted)
	{
		LogError(("All retry attempts have exhausted. Operation will not be retried"));
	}
	else if (xBackoffAlgStatus == BackoffAlgorithmSuccess)
	{
		/* Perform the backoff delay. */
		vTaskDelay(pdMS_TO_TICKS(usNextRetryBackOff));

		xReturnStatus = pdPASS;

		LogInfo(("Retry attempt %lu out of maximum retry attempts %lu.",
			(pxRetryParams->attemptsDone + 1),
			pxRetryParams->maxRetryAttempts));
	}

	return xReturnStatus;
}

// Reworked Amazon provided demo function to allow for the host parameter
static BaseType_t connectToServerWithBackoffRetriesV2(NetworkContext_t* pxNetworkContext, IotConnectHttpRequest* r)
{
    /* Struct containing the next backoff time. */
    BackoffAlgorithmContext_t xReconnectParams;
    BaseType_t xBackoffStatus = 0U;

    configASSERT(pxNetworkContext != NULL);

    /* Initialize reconnect attempts and interval. */
    BackoffAlgorithm_InitializeParams(&xReconnectParams,
        CONNECTION_RETRY_BACKOFF_BASE_MS,
        CONNECTION_RETRY_MAX_BACKOFF_DELAY_MS,
        CONNECTION_RETRY_MAX_ATTEMPTS);

    /* Attempt to connect to the HTTP server. If connection fails, retry after a
     * timeout. The timeout value will exponentially increase until either the
     * maximum timeout value is reached or the set number of attempts are
     * exhausted.*/
    TlsTransportStatus_t xTlsStatus;
    do
    {
        ( void ) xEventGroupWaitBits( xSystemEvents,
                                      EVT_MASK_NET_CONNECTED,
                                      0x00,
                                      pdTRUE,
                                      portMAX_DELAY );

        xTlsStatus = mbedtls_transport_connect( pxNetworkContext,
                                                r->host_name,
                                                443,
                                                0, 0 );

    	if( xTlsStatus != TLS_TRANSPORT_SUCCESS )
        {
            LogWarn(("Connection to the HTTP server failed. "
                "Retrying connection with backoff and jitter."));

            /* As the connection attempt failed, we will retry the connection after an
             * exponential backoff with jitter delay. */

             /* Calculate the backoff period for the next retry attempt and perform the wait operation. */
            xBackoffStatus = prvBackoffForRetry(&xReconnectParams);
        } else {
        	return pdTRUE;
        }
    } while ((xTlsStatus != TLS_TRANSPORT_SUCCESS) && (xBackoffStatus == pdPASS));

    return pdFAIL;
}

#if 0
/*-----------------------------------------------------------*/
static BaseType_t prvConnectToServerOld(NetworkContext_t* pxNetworkContext, IotConnectHttpRequest* r)
{
    ServerInfo_t serverInfo = { 0 };
    SocketsConfig_t socketsConfig = { 0 };
    BaseType_t status = pdPASS;
    TransportSocketStatus_t networkStatus;

    serverInfo.pHostName = r->host_name;
    serverInfo.hostNameLength = strlen(r->host_name);
    serverInfo.port = 443;

    socketsConfig.enableTls = true;
    socketsConfig.pAlpnProtos = NULL;
    socketsConfig.maxFragmentLength = 0;
    socketsConfig.disableSni = false;
    socketsConfig.pRootCa = r->tls_cert;
    socketsConfig.rootCaSize = strlen(r->tls_cert) + 1;
    socketsConfig.sendTimeoutMs = IOTC_HTTP_CLIENT_SEND_RECV_TIMEOUT_MS;
    socketsConfig.recvTimeoutMs = IOTC_HTTP_CLIENT_SEND_RECV_TIMEOUT_MS;

    LogInfo(("Establishing a TLS session with %s.", r->host_name));

    networkStatus = SecureSocketsTransport_Connect(pxNetworkContext,
        &serverInfo,
        &socketsConfig);

    if (networkStatus != TRANSPORT_SOCKET_STATUS_SUCCESS) {
        status = pdFAIL;
    }
    return status;
}


static BaseType_t prvConnectToServer(NetworkContext_t* pxCtx, IotConnectHttpRequest* r)
{
	pxCtx->xTransport.pNetworkContext
    ServerInfo_t serverInfo = { 0 };
    SocketsConfig_t socketsConfig = { 0 };
    BaseType_t status = pdPASS;
    TransportSocketStatus_t networkStatus;

    serverInfo.pHostName = r->host_name;
    serverInfo.hostNameLength = strlen(r->host_name);
    serverInfo.port = 443;

    socketsConfig.enableTls = true;
    socketsConfig.pAlpnProtos = NULL;
    socketsConfig.maxFragmentLength = 0;
    socketsConfig.disableSni = false;
    socketsConfig.pRootCa = r->tls_cert;
    socketsConfig.rootCaSize = strlen(r->tls_cert) + 1;
    socketsConfig.sendTimeoutMs = IOTC_HTTP_CLIENT_SEND_RECV_TIMEOUT_MS;
    socketsConfig.recvTimeoutMs = IOTC_HTTP_CLIENT_SEND_RECV_TIMEOUT_MS;

    LogInfo(("Establishing a TLS session with %s.", r->host_name));

    networkStatus = SecureSocketsTransport_Connect(pxNetworkContext,
        &serverInfo,
        &socketsConfig);

    if (networkStatus != TRANSPORT_SOCKET_STATUS_SUCCESS) {
        status = pdFAIL;
    }
    return status;
}

#endif

static BaseType_t prvClientRequest(const TransportInterface_t* ptransportInterface, IotConnectHttpRequest* r)
{
    BaseType_t status = pdFAIL;
    HTTPStatus_t httpStatus;


    configASSERT(r->resource != NULL);

    (void)memset(&requestHeaders, 0, sizeof(requestHeaders));
    (void)memset(&requestInfo, 0, sizeof(requestInfo));
    (void)memset(&response, 0, sizeof(response));

    requestInfo.pHost = r->host_name;
    requestInfo.hostLen = strlen(r->host_name);
    requestInfo.pMethod = r->payload ? HTTP_METHOD_POST : HTTP_METHOD_GET;
    requestInfo.methodLen = strlen(requestInfo.pMethod);
    requestInfo.pPath = r->resource;
    requestInfo.pathLen = strlen(r->resource);

    requestHeaders.pBuffer = httpClientBuffer;
    requestHeaders.bufferLen = IOTC_HTTP_CLIENT_USER_BUFFER_SIZE;

    response.pBuffer = httpClientBuffer;
    response.bufferLen = IOTC_HTTP_CLIENT_USER_BUFFER_SIZE;

    httpStatus = HTTPClient_InitializeRequestHeaders(&requestHeaders, &requestInfo);

    if (httpStatus != HTTPSuccess) {
        LogError(("Failed to initialize HTTP request headers: Error=%s.",
            HTTPClient_strerror(httpStatus)));
        return pdFAIL;
    }
    httpStatus = HTTPClient_AddHeader(&requestHeaders, 
        "Content-Type", strlen("Content-Type"),
        "application/json", strlen("application/json")
    );

    int tries = 0;
    do {
		httpStatus = HTTPClient_Send(ptransportInterface,
			&requestHeaders,
			(const uint8_t *) r->payload,
			r->payload ? strlen(r->payload) : 0,
			&response,
			0
		);
		if (httpStatus != HTTPNoResponse) {
			break;
		}
        vTaskDelay(pdMS_TO_TICKS(200));
        tries++;
    } while (tries < 50); // timeout after 200msz * 50 = 10 seconds.

    if (httpStatus != HTTPSuccess) {
        LogError(("An error occurred in downloading the file. Failed to send HTTP GET request to %s%s: Error=%s.",
            r->host_name, r->resource, HTTPClient_strerror(httpStatus)));
        return pdFAIL;
    }

    LogDebug(("Received HTTP response from %s%s...", host, path));
    LogDebug(("Response Headers:\n%.*s", (int32_t)response.headersLen, response.pHeaders));
    LogInfo(("Response Body (%lu):\n%.*s\n",
        response.bodyLen,
        (int32_t)response.bodyLen,
        response.pBody));
    r->response = (char *) response.pBody;
    r->response[response.bodyLen] = 0; // null terminate
    status = (response.statusCode == 200) ? pdPASS : pdFAIL;

    if (status != pdPASS) {
        LogError(("Received an invalid response from the server Result: %u.", response.statusCode));
    }

    return status;
}


int iotconnect_https_request(IotConnectHttpRequest* request)
{
    TransportInterface_t transportInterface;
    NetworkContext_t* networkContext;
    BaseType_t status = pdPASS;

    networkContext = mbedtls_transport_allocate();
    if (!networkContext) {
    	LogError( "HTTP: Failed to allocate an mbedtls transport context." );
    	return pdFALSE;
    }

    PkiObject_t httpsRootCaPkiObject = PKI_OBJ_PEM(CERT_GODADDY_INT_SECURE_G2, strlen(CERT_GODADDY_INT_SECURE_G2)+1);
    if (mbedtls_transport_configure( networkContext,
    		NULL,
			NULL,
			NULL,
			&httpsRootCaPkiObject,
			1 )
    		) {
        LogError( "HTTP: Failed to configure mbedtls transport." );
        return pdFALSE;
    }

    BaseType_t tries = 0;
    do {

        // keep trying and break if tries is exceeded
        status = connectToServerWithBackoffRetriesV2(networkContext, request);

        if (status == pdFAIL) {
            LogError(("Failed to connect to HTTP server %s. Tries so far %d...", request->host_name, tries));
        }

        if (status == pdPASS) {
            transportInterface.pNetworkContext = networkContext;
            transportInterface.send = mbedtls_transport_send;
            transportInterface.recv = mbedtls_transport_recv;

            vTaskDelay(pdMS_TO_TICKS(20)); // allow connection to establish to run to avoid "Zero returned from transport recv" error spam.

            status = prvClientRequest(&transportInterface, request);

            // cleanup/disconnect
            mbedtls_transport_disconnect(networkContext);
            break;
        }

        if (tries < MAX_HTTP_REQUEST_TRIES) {
            LogWarn(("HTTP request iteration %lu failed. Retrying...", tries));
        }
        else {
            LogError(("All %d HTTP request iterations failed.", MAX_HTTP_REQUEST_TRIES));
            break;
        }

        vTaskDelay(HTTP_REQUEST_BACKOFF_MS);
        tries++;



    } while (status != pdPASS);
    mbedtls_transport_free(networkContext);

    if (status == pdPASS) {
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}
