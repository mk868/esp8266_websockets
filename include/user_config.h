/*
	ESP8266 websocket client
	version: 0.1
	GIT: https://github.com/mkuch95/esp8266_websockets
*/

#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__


#define WIFI_SSID "YOUR-SSID"

#define WIFI_PASSWORD "YOUR-PASSWORD"

#define WS_SERVER "YOUR-SERVER-IP-OR-HOST"

#define WS_PORT 1337

#define WS_PATH "/"

#define SET_DNS1(ipaddr) IP4_ADDR(ipaddr, 8, 8, 8, 8)
#define SET_DNS2(ipaddr) IP4_ADDR(ipaddr, 8, 8, 4, 4)

#endif


