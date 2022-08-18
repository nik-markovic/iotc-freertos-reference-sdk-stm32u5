# About

This repository contains a **PARTIAL** implementation the IoTConnect SDK for FreeRTOS STM32U5 reference project.

In order for this code to be fully functional, SNTP implementation is required as IoTConnect
requires timestamp to be sent for telemetry messages, but the FreeRTOS reference project
does not provide means to obtain time from the network.

This issue will be remedied by a future addition of SNTP support.

### Build Instructions

- Clone the FreeRTOS reference project and this project:
```shell
git clone --recurse-submodules  https://github.com/FreeRTOS/iot-reference-stm32u5.git 
cd iot-reference-stm32u5 # be in the root of the repo for next steps
```
- Ensure that coreHTTP and submodule's dependencies are pulled:
```shell
git submodule update --init --recursive Middleware/FreeRTOS/coreHTTP
```
- Clone this project into Middleware/Avnet:
```shell
mkdir -p Middleware/Avnet
git clone --recurse-submodules  https://github.com/avnet-iotconnect/iotc-freertos-reference-sdk-stm32u5.git Middleware/Avnet/iotc-freertos-reference-sdk-stm32u5
```
- Follow the original instructions in the FreeRTOS repo to bring up the project with STM32CubeIDE 
- Link the iotc-freertos-reference-sdk-stm32u5 directory into the NTZ (no trust zones) project:
  - Navigate to Libraries directory in project explorer and right-click it.
  - Select **New->Folder**
  - Enter: *iotc-freertos-reference-sdk-stm32u5* for **Folder Name**
  - Expand the ***Advanced>>*** section and select "Link to alternate location (Linked Folder)"
  - Enter: *WORKSPACE_LOC/Middleware/Avnet/iotc-freertos-reference-sdk-stm32u5*
  - Click **Finish**
- Link the coreHTTP directory into the NTZ (no trust zones) project:
  - Navigate to Libraries directory in project explorer and right-click it.
  - Select **New->Folder**
  - Enter: *coreHTTP* for **Folder Name**
  - Expand the ***Advanced>>*** section and select "Link to alternate location (Linked Folder)"
  - Enter: *WORKSPACE_LOC/Middleware/FreeRTOS/coreHTTP*
  - Click **Finish**

- Right-click each of the following folder and select **Add/Remove include path** 
and ensure that the following directories are added to include paths:
  - Libraries/coreHTTP/source/include
  - Libraries/coreHTTP/source/dependency/3rdparty/http_parser
  - Libraries/iotc-freertos-reference-sdk-stm32u5 folder directories:
    - lib/iotc-c-lib/include
    - lib/cJSON
    - iotconnect-afr-layer/include
    - include
    - iotconnect-demo/config 
- Right-click each of the directories and files mentioned below and exclude them from build (**Build Configurations -> Exclude From Build**, check **Debug** and click **OK**)  
- Libraries/iotc-freertos-reference-sdk-stm32u5/lib/cJSON: *test.c* and all subdirectories   
- Libraries/iotc-freertos-reference-sdk-stm32u5/lib/iotc-c-lib: *tests* directory
- Libraries/coreHTTP/source/dependency/3rdparty/http_parser: *test.c*, *bench.c* and all subdirectories   
- Edit Common/app/mqtt/mqtt_agent_task.c: add the include line, modify the xConnectInfo.pUserName assignment and add a call to iotc_sync_obtain_response:
```C
/// Add to headers:
#include "iotconnect_sync.h"
/// Modify prvConfigureAgentTaskCtx()
        pxCtx->xConnectInfo.pUserName = iotc_sync_get_username();
        pxCtx->xConnectInfo.userNameLength = (uint16_t) strlen(iotc_sync_get_username());
        pxCtx->xConnectInfo.pPassword = NULL;
        pxCtx->xConnectInfo.passwordLength = 0U;

        pxCtx->xConnectInfo.pClientIdentifier = iotc_sync_get_client_id();
        uxTempSize = trlen(iotc_sync_get_client_id());

        if( ( pxCtx->xConnectInfo.pClientIdentifier != NULL ) &&
            ( uxTempSize > 0 ) &&
            ( uxTempSize <= UINT16_MAX ) )
        // ...    
            if( xStatus == MQTTSuccess )
    {
        // pxCtx->pcMqttEndpoint = KVStore_getStringHeap( CS_CORE_MQTT_ENDPOINT,
    	//                                                &( pxCtx->uxMqttEndpointLen ) );
        pxCtx->pcMqttEndpoint = iotc_sync_get_iothub_host();
        pxCtx->uxMqttEndpointLen = strlen(iotc_sync_get_iothub_host());

        if( ( pxCtx->uxMqttEndpointLen == 0 ) ||
            ( pxCtx->pcMqttEndpoint == NULL ) )
        // ...
// Add a call to iotc_sync_obtain_response() to vMQTTAgentTask:

    if( pucNetworkBuffer == NULL )
    {
        LogError( "Failed to allocate %d bytes for pucNetworkBuffer.", MQTT_AGENT_NETWORK_BUFFER_SIZE );
        xMQTTStatus = MQTTNoMemory;
    }

    if (iotc_sync_obtain_response())
    {
        LogError( "Failed run IoTConnect SYNC" );
        xMQTTStatus = MQTTIllegalState;
    }

    if( xMQTTStatus == MQTTSuccess )
    {
        pxNetworkContext = mbedtls_transport_allocate();


    /* Miscellaneous initialization. */
    ulGlobalEntryTimeMs = prvGetTimeMs();
    
    // Run IoTConnect sync befor any MQTT tls setup
   
    
    /* Memory Allocation */
    pucNetworkBuffer = ( uint8_t * ) pvPortMalloc( MQTT_AGENT_NETWORK_BUFFER_SIZE );

```
- Add a profile declaration that adds MBEDTLS_MD_SHA1, and assign it instead of the default in Common/net/mbedls_transport.c:
```C
const mbedtls_x509_crt_profile mbedtls_x509_crt_profile_iotconnect =
{
    /* Hashes from SHA-256 and above. Note that this selection
     * should be aligned with ssl_preset_default_hashes in ssl_tls.c. */
    MBEDTLS_X509_ID_FLAG( MBEDTLS_MD_SHA256 ) |
    MBEDTLS_X509_ID_FLAG( MBEDTLS_MD_SHA384 ) |
    MBEDTLS_X509_ID_FLAG( MBEDTLS_MD_SHA512 ) |
	MBEDTLS_X509_ID_FLAG( MBEDTLS_MD_SHA1 ), // added
    0xFFFFFFF, /* Any PK alg    */
#if defined(MBEDTLS_ECP_C)
    /* Curves at or above 128-bit security level. Note that this selection
     * should be aligned with ssl_preset_default_curves in ssl_tls.c. */
    MBEDTLS_X509_ID_FLAG( MBEDTLS_ECP_DP_SECP256R1 ) |
    MBEDTLS_X509_ID_FLAG( MBEDTLS_ECP_DP_SECP384R1 ) |
    MBEDTLS_X509_ID_FLAG( MBEDTLS_ECP_DP_SECP521R1 ) |
    MBEDTLS_X509_ID_FLAG( MBEDTLS_ECP_DP_BP256R1 ) |
    MBEDTLS_X509_ID_FLAG( MBEDTLS_ECP_DP_BP384R1 ) |
    MBEDTLS_X509_ID_FLAG( MBEDTLS_ECP_DP_BP512R1 ) |
    0,
#else
    0,
#endif
    2048,
};

// .... in mbedtls_transport_configure()
        mbedtls_ssl_conf_cert_profile( pxSslConfig, &mbedtls_x509_crt_profile_iotconnect );
```
- Disable AWS sample tasks and add the IoTConnect Sample task in Src/app_main.c
```C
    //xResult = xTaskCreate( vOTAUpdateTask, "OTAUpdate", 4096, NULL, tskIDLE_PRIORITY + 1, NULL );
    //configASSERT( xResult == pdTRUE );

    //xResult = xTaskCreate( vEnvironmentSensorPublishTask, "EnvSense", 1024, NULL, 6, NULL );
    //configASSERT( xResult == pdTRUE );

    // xResult = xTaskCreate( vMotionSensorsPublish, "MotionS", 2048, NULL, 5, NULL );
    // configASSERT( xResult == pdTRUE );


    //xResult = xTaskCreate( vShadowDeviceTask, "ShadowDevice", 1024, NULL, 5, NULL );
    //configASSERT( xResult == pdTRUE );

    //xResult = xTaskCreate( vDefenderAgentTask, "AWSDefender", 2048, NULL, 5, NULL );
    //configASSERT( xResult == pdTRUE );
    
    extern int iotconnect_app_main();
    xResult = xTaskCreate( iotconnect_app_main, "IoTConnect", 10 * 1024, NULL, 5, NULL );
 
```
- Configure your IoTConnect account parameters (CPID, at 
Libraries/iotc-freertos-reference-sdk-stm32u5/iotconnect-demo/config/app_config.h.
- Once the firmware is uploaded and running on the board per FreeRTOS's project instructions, enter these commands into the serial terminal:
```shell
conf set wifi_ssid <YourSSID>
conf set wifi_credential <YourWiFiPassword>
conf commit
reset
```