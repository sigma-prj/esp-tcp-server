#ifndef INCLUDE_MOD_ENUMS_H_
#define INCLUDE_MOD_ENUMS_H_

#include <user_interface.h>

#ifdef UART_DEBUG_LOGS

void lookup_station_status(char* buffer, uint8 value);
void lookup_cipher(char* buffer, CIPHER_TYPE value);
void lookup_espconn_error(char* buffer, sint8 value);

#endif

#ifdef UART_DEBUG_LOGS
#define OS_UART_LOG(...) os_printf(__VA_ARGS__)
#else
#define OS_UART_LOG(...)
#endif

#endif /* INCLUDE_MOD_ENUMS_H_ */
