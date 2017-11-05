/*
	ESP8266 websocket client
	version: 0.1
	GIT: https://github.com/mkuch95/esp8266_websockets
*/

#include "ets_sys.h"
#include "osapi.h"

#include "driver/uart.h"
#include <user_interface.h>
#include <espconn.h>

#include "websocket_client.h"

#include "esplogger.h"

/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
*******************************************************************************/
uint32 ICACHE_FLASH_ATTR
user_rf_cal_sector_set(void)
{
	enum flash_size_map size_map = system_get_flash_size_map();
	uint32 rf_cal_sec = 0;

	switch (size_map) {
	case FLASH_SIZE_4M_MAP_256_256:
		rf_cal_sec = 128 - 5;
		break;

	case FLASH_SIZE_8M_MAP_512_512:
		rf_cal_sec = 256 - 5;
		break;

	case FLASH_SIZE_16M_MAP_512_512:
	case FLASH_SIZE_16M_MAP_1024_1024:
		rf_cal_sec = 512 - 5;
		break;

	case FLASH_SIZE_32M_MAP_512_512:
	case FLASH_SIZE_32M_MAP_1024_1024:
		rf_cal_sec = 1024 - 5;
		break;

	default:
		rf_cal_sec = 0;
		break;
	}

	return rf_cal_sec;
}

void ICACHE_FLASH_ATTR
user_rf_pre_init(void)
{
}


static struct WebSocketClient wsClient;


void ICACHE_FLASH_ATTR dns_server_init() {
	ip_addr_t dns;
	SET_DNS1(&dns);
	espconn_dns_setserver(0, &dns);
	SET_DNS2(&dns);
	espconn_dns_setserver(1, &dns);
}


void status_change_callback(WebSocketClient *client, enum WebSocketClientStatus status) {
	char* names[] = {
		"Connected",
		"Dns Error",
		"Disconnected",
		"Connection Timeout Error",
		"Handshake Error",
		"FrameParse Error"
	};
	log_info("status: (%d) %s", status, names[status]);

	if (status == WebSocketClientConnected) {
		char* message = "first message";
		websocket_client_send(client, message, strlen(message), WS_OPCODE_TEXT);
	}
}


void data_receive_callback(WebSocketClient *client, uint8_t opcode, char* data, unsigned int length) {
	log_info("data(%d):: %s (%d)", opcode, data, length);
}


static os_timer_t timer1;


static bool ws_started = false;
static int ws_i = 0;

static void ICACHE_FLASH_ATTR timer_cb(void *arg)
{
	//when connected to the wifi network:
	// - connect to webscoket server
	// - send "first message"
	// - send random data 10 times
	// - disconnect websocket

	if (wifi_station_get_connect_status() != 5) {//no internet connection
		return;
	}

	if (!ws_started) {
		log_info("ws connecting");
		ws_started = true;

		websocket_client_connect(&wsClient, WS_SERVER, WS_PORT, WS_PATH);
		return;
	}

	if (websocket_client_is_connected(&wsClient)) {
		
		if (ws_i++ < 10) {
			char buff[200];

			os_sprintf(buff, "test message: %d", (int)os_random());

			websocket_client_send(&wsClient, buff, strlen(buff), WS_OPCODE_TEXT);

			return;
		}

		websocket_client_disconnect(&wsClient);
	}
}


void ICACHE_FLASH_ATTR init_done() {
	log_info("init done");


	wifi_station_set_auto_connect(false);
	wifi_set_opmode(STATION_MODE);

	char* ssid = WIFI_SSID;
	char* password = WIFI_PASSWORD;

	struct station_config stationConfig;
	os_bzero(&stationConfig, sizeof(struct station_config));

	strncpy(stationConfig.ssid, ssid, strlen(ssid));
	strncpy(stationConfig.password, password, strlen(password));
	wifi_station_set_config_current(&stationConfig);
	wifi_station_connect();


	websocket_client_init(&wsClient);
	wsClient.dataReceiveCallback = data_receive_callback;
	wsClient.statusChangeCallback = status_change_callback;
}



/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR user_init(void)
{
	uart_init(BIT_RATE_115200, BIT_RATE_115200);
	//system_set_os_print(0);

	os_printf("SDK version:%s\n", system_get_sdk_version());
	os_printf("HI I'm ESP8266\n");


	os_timer_disarm(&timer1);
	os_timer_setfn(&timer1, (os_timer_func_t *)timer_cb, NULL);
	os_timer_arm(&timer1, 3000, true);

	dns_server_init();

	system_init_done_cb(init_done);
}

