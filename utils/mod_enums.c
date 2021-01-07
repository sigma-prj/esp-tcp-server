#include "mod_enums.h"

#include <espconn.h>
#include <osapi.h>

#ifdef UART_DEBUG_LOGS

void lookup_station_status(char* buffer, uint8 value)
{
	switch (value)
	{
		case STATION_IDLE:
			os_strcpy(buffer, "STATION_IDLE");
			break;
		case STATION_CONNECTING:
			os_strcpy(buffer, "STATION_CONNECTING");
			break;
		case STATION_WRONG_PASSWORD:
			os_strcpy(buffer, "STATION_WRONG_PASSWORD");
			break;
		case STATION_NO_AP_FOUND:
			os_strcpy(buffer, "STATION_NO_AP_FOUND");
			break;
		case STATION_CONNECT_FAIL:
			os_strcpy(buffer, "STATION_CONNECT_FAIL");
			break;
		case STATION_GOT_IP:
			os_strcpy(buffer, "STATION_GOT_IP");
			break;
	}
}

void lookup_cipher(char* buffer, CIPHER_TYPE value)
{
	switch (value)
	{
		case CIPHER_NONE:
			os_strcpy(buffer, "CIPHER_NONE");
			break;
		case CIPHER_WEP40:
			os_strcpy(buffer, "CIPHER_WEP40");
			break;
		case CIPHER_WEP104:
			os_strcpy(buffer, "CIPHER_WEP104");
			break;
		case CIPHER_TKIP:
			os_strcpy(buffer, "CIPHER_TKIP");
			break;
		case CIPHER_CCMP:
			os_strcpy(buffer, "CIPHER_CCMP");
			break;
		case CIPHER_TKIP_CCMP:
			os_strcpy(buffer, "CIPHER_TKIP_CCMP");
			break;
		case CIPHER_UNKNOWN:
			os_strcpy(buffer, "CIPHER_UNKNOWN");
			break;
	}
}

void lookup_espconn_error(char* buffer, sint8 value)
{
	switch (value)
	{
		case ESPCONN_OK:
			os_strcpy(buffer, "ESPCONN_OK: No error");
			break;
		case ESPCONN_MEM:
			os_strcpy(buffer, "ESPCONN_MEM: Out of memory");
			break;
		case ESPCONN_TIMEOUT:
			os_strcpy(buffer, "ESPCONN_TIMEOUT: Timeout");
			break;
		case ESPCONN_ABRT:
			os_strcpy(buffer, "ESPCONN_ABRT: TCP connection aborted");
			break;
		case ESPCONN_RST:
			os_strcpy(buffer, "ESPCONN_RST: TCP connection reset");
			break;
		case ESPCONN_CLSD:
			os_strcpy(buffer, "ESPCONN_CLSD: TCP connection closed");
			break;
		case ESPCONN_CONN:
			os_strcpy(buffer, "ESPCONN_CONN: TCP connection fails");
			break;
		case ESPCONN_HANDSHAKE:
			os_strcpy(buffer, "ESPCONN_HANDSHAKE: TCP SSL handshake fails");
			break;
		case ESPCONN_SSL_INVALID_DATA:
			os_strcpy(buffer, "ESPCONN_SSL_INVALID_DATA: TCP SSL application invalid");
			break;
		case ESPCONN_ISCONN:
			os_strcpy(buffer, "ESPCONN_ISCONN: Already connected");
			break;
		case ESPCONN_RTE:
			os_strcpy(buffer, "ESPCONN_RTE: Routing Problem");
			break;
		case ESPCONN_ARG:
			os_strcpy(buffer, "ESPCONN_ARG: Illegal argument; cannot find TCP connection according to structure espconn.");
			break;
		default:
			os_strcpy(buffer, "UNKNOWN: Unknown error");
			break;
	}
}

#endif
