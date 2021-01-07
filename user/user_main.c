#include <mem.h>

#include "osapi.h"
#include "user_interface.h"
#include "gpio.h"
#include "espconn.h"

#include "mod_enums.h"

// Establishes ESP access point WiFi session ID. Session ID which should be visible to other devices.
#define WIFI_ACCESS_POINT_SSID					"ESP8266_AP_LED"
// Establishes ESP access point WiFi passphrase
#define WIFI_ACCESS_POINT_PASSPHRASE			"ap_test5"
// Establishes maximum allowed clients to connect
#define WIFI_ACCESS_POINT_MAX_CONNECTIONS		3
// TCP Server socket port number
#define SERVER_SOCKET_PORT						1010

// Baud rate which will be used for debug logs UART output
#define UART_BAUD_RATE							115200

// Used to distinguish between client connection states. Used for internal LED indication:
// LED off - disconnected - no client WiFi sessions established
// LED blinking - client WiFi session established, but no socket connection is present yet
// LED on - client socket connected
#define STATE_DISCONNECTED						0
#define STATE_CLIENT_WIFI_CONNECTED				1
#define STATE_CLIENT_SOCKET_CONNECTED			2

// Sets timer period interval in ticks for different events (1 tick - 100ms)
#define TIMER_PERIOD_STATE_UPDATE				50
#define TIMER_PERIOD_WIFI_STATUS_LED			5
#define TIMER_PERIOD_RESET						1000000

// System partitions sizes definition
#define SYSTEM_PARTITION_RF_CAL_SZ				0x1000
#define SYSTEM_PARTITION_PHY_DATA_SZ			0x1000
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_SZ	0x3000

// SPI memory size definition
#define SYSTEM_SPI_SIZE							0x400000

// System partitions sizes definition
#define SYSTEM_PARTITION_RF_CAL_ADDR			SYSTEM_SPI_SIZE - SYSTEM_PARTITION_SYSTEM_PARAMETER_SZ - SYSTEM_PARTITION_PHY_DATA_SZ - SYSTEM_PARTITION_RF_CAL_SZ
#define SYSTEM_PARTITION_PHY_DATA_ADDR			SYSTEM_SPI_SIZE - SYSTEM_PARTITION_SYSTEM_PARAMETER_SZ - SYSTEM_PARTITION_PHY_DATA_SZ
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR	SYSTEM_SPI_SIZE - SYSTEM_PARTITION_SYSTEM_PARAMETER_SZ

// Input digit-chars range which will be processed by TCP Server
static const char CHAR_DIGITS_START = '0';
static const char CHAR_DIGITS_END = '7';
// Internal LED GPIO pin
static const uint8 GPIO_PIN_LED_INT = 2;
// External LEDs GPIO pins
static const uint8 GPIO_PIN_LED_1 = 12;
static const uint8 GPIO_PIN_LED_2 = 13;
static const uint8 GPIO_PIN_LED_3 = 14;
// Timer used to refresh LEDs state bits
static os_timer_t start_timer;
// Timer invocation index number
static uint32 tick_index = 0;
// Indicates current connection state
static uint8 client_connection_state = STATE_DISCONNECTED;
// Holds ESP connection resource
static struct espconn esp_conn;
// Holds TCP server socket resource
static esp_tcp esptcp;
// Holds value of previously established WiFi sessions. Used for logging purposes.
static uint8 prev_wifi_sessions_num = 0;
// Indicates how many client TCP connections have been established
static sint8 open_tcp_connections = 0;

static const partition_item_t part_table[] =
{
	{ SYSTEM_PARTITION_RF_CAL,				SYSTEM_PARTITION_RF_CAL_ADDR,		SYSTEM_PARTITION_RF_CAL_SZ					},
	{ SYSTEM_PARTITION_PHY_DATA,			SYSTEM_PARTITION_PHY_DATA_ADDR,		SYSTEM_PARTITION_PHY_DATA_SZ				},
	{ SYSTEM_PARTITION_SYSTEM_PARAMETER,	SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR, SYSTEM_PARTITION_SYSTEM_PARAMETER_SZ	}
};

// Pointer to ESP access point configuration struct
LOCAL struct softap_config* ap_config = NULL;

// System pre-init method. Used for partitions initialization.
void ICACHE_FLASH_ATTR user_pre_init(void)
{
	system_partition_table_regist(part_table, 3, SPI_FLASH_SIZE_MAP);
}

// ESP Access Point Deinitialization. AP resources releasing.
void access_point_release(void)
{
	os_free(ap_config);
	ap_config = NULL;
}

// AP DHCP configuration
void access_point_dhcp_and_ip_setup(void)
{
	wifi_softap_dhcps_stop();

	// Sets access point host IP address
	struct ip_info info;
	IP4_ADDR(&info.ip, 10, 0, 0, 1);
	IP4_ADDR(&info.netmask, 255, 255, 255, 0);
	wifi_set_ip_info(SOFTAP_IF, &info);

	// Sets access point DHCP IPs assignment range
	struct dhcps_lease dhcp_lease;
	// DHCP IP ranges start
	IP4_ADDR(&dhcp_lease.start_ip, 10, 0, 0, 100);
	// DHCP IP ranges end
	IP4_ADDR(&dhcp_lease.end_ip, 10, 0, 0, 110);
	wifi_softap_set_dhcps_lease(&dhcp_lease);

	if (wifi_softap_dhcps_start())
	{
		OS_UART_LOG("[INFO] AP DHCP Started\n");
	}
	else
	{
		OS_UART_LOG("[ERROR] Unable to start AP DHCP\n");
	}
}

// ESP Access Point initialization. AP configuring.
void access_point_setup(void)
{
	if (ap_config != NULL)
	{
		access_point_release();
	}

	ap_config = (struct softap_config*)os_zalloc(sizeof(struct softap_config));

	// ESP Access Point parameters:
	ap_config->authmode = AUTH_WPA_WPA2_PSK;
	ap_config->max_connection = WIFI_ACCESS_POINT_MAX_CONNECTIONS;
	ap_config->ssid_hidden = false;
	os_strcpy(ap_config->ssid, WIFI_ACCESS_POINT_SSID);
	ap_config->ssid_len = 0;
	os_strcpy(ap_config->password, WIFI_ACCESS_POINT_PASSPHRASE);
	ap_config->channel = 10;

	if (wifi_softap_set_config(ap_config))
	{
		OS_UART_LOG("[INFO] AP config is set\n");
		access_point_dhcp_and_ip_setup();
	}
	else
	{
		OS_UART_LOG("[ERROR] Unable to set Access Point configuration\n");
		access_point_release();
	}
}

// Method is used to set LEDs state according to the last 3 bits of input digit (e.g '7' - all LEDs are on, '5' - only the first and last LEDs are on, etc)
void process_digit_key(char digit)
{
	uint8 num = (digit - CHAR_DIGITS_START) & 0x07;
	OS_UART_LOG("[INFO] Processing digit-key: %d\n", num);
	// Sets 3 LED pins in bulk
	gpio_output_set(0x07 << GPIO_PIN_LED_1, (num ^ 0x07) << GPIO_PIN_LED_1, 0, 0);
}

// This callback method is triggered when server receives data from client
LOCAL void ICACHE_FLASH_ATTR on_tcp_server_receive(void* arg, char* pusrdata, unsigned short length)
{
	OS_UART_LOG("[INFO] TCP Server 'on data received' event. Received %d bytes.\n", length);
	// In case of logs are enabled - will try to print received package content to UART
#ifdef UART_DEBUG_LOGS
	char* pstr_buf = (char*)os_zalloc(length + 1);
	os_memcpy(pstr_buf, pusrdata, length);
	pstr_buf[length] = 0;
	OS_UART_LOG("[INFO] Received package content:\n%s\n", pstr_buf);
	os_free(pstr_buf);
#endif
	if (length)
	{
		char last_char = 0;
		unsigned short idx;
		for (idx = length; idx > 0 && !last_char; --idx)
		{
			last_char = pusrdata[idx - 1];
			if (last_char >= CHAR_DIGITS_START && last_char <= CHAR_DIGITS_END)
			{
				process_digit_key(last_char);
			}
			else
			{
				last_char = 0;
			}
		}
	}
}

// This callback method is triggered when client reconnects to the server due to some issues
LOCAL void ICACHE_FLASH_ATTR on_tcp_server_reconnect(void *arg, sint8 err)
{
	struct espconn *pesp_conn = arg;
	OS_UART_LOG("[WARN] TCP Server %d.%d.%d.%d:%d err %d 'on reconnect' event\n", pesp_conn->proto.tcp->remote_ip[0],
					pesp_conn->proto.tcp->remote_ip[1],pesp_conn->proto.tcp->remote_ip[2],
					pesp_conn->proto.tcp->remote_ip[3],pesp_conn->proto.tcp->remote_port, err);
}

// This callback method is triggered when client becomes disconnected from server
LOCAL void ICACHE_FLASH_ATTR on_tcp_server_disconnect(void *arg)
{
	struct espconn *pesp_conn = arg;
	OS_UART_LOG("[INFO] TCP Server %d.%d.%d.%d:%d 'on disconnect' event\n", pesp_conn->proto.tcp->remote_ip[0],
					pesp_conn->proto.tcp->remote_ip[1],pesp_conn->proto.tcp->remote_ip[2],
					pesp_conn->proto.tcp->remote_ip[3],pesp_conn->proto.tcp->remote_port);
	open_tcp_connections--;
}

// This callback method is triggered when client's connection is accepted by server
LOCAL void ICACHE_FLASH_ATTR on_tcp_server_accepted(void *arg)
{
	OS_UART_LOG("[INFO] TCP Server 'on client connection accepted' event\n");
	struct espconn *pesp_conn = arg;
	espconn_regist_recvcb(pesp_conn, on_tcp_server_receive);
	espconn_regist_reconcb(pesp_conn, on_tcp_server_reconnect);
	espconn_regist_disconcb(pesp_conn, on_tcp_server_disconnect);
	open_tcp_connections++;
}

// This method makes a setup of TCP server to listen for client connections
void tcp_server_setup(void)
{
	esp_conn.type = ESPCONN_TCP;
	esp_conn.state = ESPCONN_NONE;
	esp_conn.proto.tcp = &esptcp;
	esp_conn.proto.tcp->local_port = SERVER_SOCKET_PORT;
	espconn_regist_connectcb(&esp_conn, on_tcp_server_accepted);
	sint8 res = espconn_accept(&esp_conn);
	if (res == ESPCONN_OK)
	{
		OS_UART_LOG("[INFO] TCP Server accepts connections on port %d\n", SERVER_SOCKET_PORT);
	}
	else
	{
#ifdef UART_DEBUG_LOGS
		char state_str[250];
		lookup_espconn_error(state_str, res);
		OS_UART_LOG("[ERROR] Unable set TCP Server to accept connections: %s\n", state_str);
#endif
	}
	// Increased client connection timeout (set to 1 minute)
	espconn_regist_time(&esp_conn, 60, 0);
}

// Timer callback method. Triggered 10 times per second.
void on_timer(void* arg)
{
	// WiFi client_connection_state variable update
	if (tick_index % TIMER_PERIOD_STATE_UPDATE == 0)
	{
		uint8 wifi_sessions_num = wifi_softap_get_station_num();

		if (wifi_sessions_num != prev_wifi_sessions_num)
		{
			OS_UART_LOG("[INFO] Number of connected WiFi sessions: %d\n", wifi_sessions_num);
			prev_wifi_sessions_num = wifi_sessions_num;
		}

		if (wifi_sessions_num)
		{
			if (open_tcp_connections)
			{
				client_connection_state = STATE_CLIENT_SOCKET_CONNECTED;
			}
			else
			{
				client_connection_state = STATE_CLIENT_WIFI_CONNECTED;
			}
		}
		else
		{
			client_connection_state = STATE_DISCONNECTED;
			open_tcp_connections = 0;
		}
	}

	// WiFi status LED indication update
	if (tick_index % TIMER_PERIOD_WIFI_STATUS_LED == 0)
	{
		if (client_connection_state == STATE_CLIENT_SOCKET_CONNECTED)
		{
			gpio_output_set(0, (1 << GPIO_PIN_LED_INT), 0, 0);
		}
		else if (client_connection_state == STATE_CLIENT_WIFI_CONNECTED)
		{
			if (GPIO_REG_READ(GPIO_OUT_ADDRESS) & (1 << GPIO_PIN_LED_INT))
			{
				gpio_output_set(0, (1 << GPIO_PIN_LED_INT), 0, 0);
			}
			else
			{
				gpio_output_set((1 << GPIO_PIN_LED_INT), 0, 0, 0);
			}
		}
		else
		{
			gpio_output_set((1 << GPIO_PIN_LED_INT), 0, 0, 0);
		}
	}

	tick_index++;
	if (tick_index >= TIMER_PERIOD_RESET)
	{
		tick_index = 0;
	}
}

// Callback method is triggered upon ESP initialization completion
void on_user_init_completed(void)
{
	access_point_setup();
	struct ip_info info;
	wifi_get_ip_info(SOFTAP_IF, &info);
	OS_UART_LOG("[INFO] AP Host IP: %d.%d.%d.%d\n",
			*((uint8*) &info.ip.addr),
			*((uint8*)&info.ip.addr+1),
			*((uint8*)&info.ip.addr+2),
			*((uint8*)&info.ip.addr+3));
	OS_UART_LOG("[INFO] ESP Access Point initialization is completed\n");
	tcp_server_setup();
	os_timer_setfn(&start_timer, (os_timer_func_t*)on_timer, NULL);
	os_timer_arm(&start_timer, 100, 1);
}

// Main user initialization method
void ICACHE_FLASH_ATTR user_init(void)
{
	// UART output initialization for logging output
	uart_init(UART_BAUD_RATE, UART_BAUD_RATE);
	// LEDs pins initialization
	gpio_init();
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_GPIO12);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, FUNC_GPIO14);
	gpio_output_set(0, 0, (1 << GPIO_PIN_LED_INT), 0);
	gpio_output_set(0, 0, (1 << GPIO_PIN_LED_1), 0);
	gpio_output_set(0, 0, (1 << GPIO_PIN_LED_2), 0);
	gpio_output_set(0, 0, (1 << GPIO_PIN_LED_3), 0);
	// Sets ESP to access point mode
	wifi_set_opmode(SOFTAP_MODE);
	// on_user_init_completed callback triggered upon initialization is completed
	system_init_done_cb(on_user_init_completed);
}
