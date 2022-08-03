# About

This repository contains the IoTConnect SDK for FreeRTOS STM32U5 reference project.

### Build Instructions

- Clone the FreeRTOS reference project and this project:
```shell
git clone https://github.com/FreeRTOS/iot-reference-stm32u5.git --recurse-submodules
mkdir cd lab-iot-reference-stm32u5/Middleware/avnet
cd lab-iot-reference-stm32u5/Middleware/avnet
git clone https://github.com/avnet-iotconnect/iotc-freertos-reference-sdk-stm32u5.git --recurse-submodules
```
- Snure that HTTP dependencies are pulled (TODO)
```shell script
TODO
```
- Follow the original instructions in the FreeRTOS repo to bring up the project with STM32CubeIDE 
- Link the iotc-freertos-reference-sdk-stm32u5:
  - Navigate to Libraries directory in project explorer and right-click it.
  - Select **New->Folder**
  - Enter: *iotc-freertos-reference-sdk-stm32u5*
  - Expand the ***Advanced>>*** section and select "Link to alternate location (Linked Folder)"
  - Enter: *WORKSPACE_LOC/Middleware/avnet/iotc-freertos-reference-sdk-stm32u5*
  - Click **Finish**


#define METRICS_PLATFORM_NAME "poc-iotconnect-iothub-003-eu2.azure-devices.net/avtds-stm32u5/?api-version=2018-06-30"
conf set thing_name avtds-stm32u5
conf get thing_name
conf commit
reset
mqtt_agent_task.c
        pxCtx->xConnectInfo.pUserName = AWS_IOT_METRICS_STRING;
        pxCtx->xConnectInfo.userNameLength = AWS_IOT_METRICS_STRING_LENGTH;


x509crt.c OR mbedtls_transport.c
const mbedtls_x509_crt_profile mbedtls_x509_crt_profile_default =
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

