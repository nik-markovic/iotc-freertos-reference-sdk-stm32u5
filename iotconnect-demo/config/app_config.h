#pragma once
#ifndef __IOTCONNECT_CONFIG__H__
#define __IOTCONNECT_CONFIG__H__

#define IOTCONNECT_CPID "avtds"
#define IOTCONNECT_ENV  "Avnet"
#define IOTCONNECT_DUID "stm32u5"


/**************************************************/
/******* DO NOT CHANGE the following order ********/
/**************************************************/

/* Logging config definition and header files inclusion are required in the following order:
 * 1. Include the header file "logging_levels.h".
 * 2. Define the LIBRARY_LOG_NAME and LIBRARY_LOG_LEVEL macros depending on
 * the logging configuration for DEMO.
 * 3. Include the header file "logging_stack.h", if logging is enabled for DEMO.
 */

/* Include header that defines log levels. */
#include "logging_levels.h"



/************ End of logging configuration ****************/


#endif
