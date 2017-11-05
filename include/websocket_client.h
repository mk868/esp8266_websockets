#pragma once
/*
	ESP8266 websocket client
	version: 0.1
	GIT: https://github.com/mkuch95/esp8266_websockets
*/

#ifndef WEBSOCKET_CLIENT_H_
#define WEBSOCKET_CLIENT_H_


#include <ip_addr.h>
#include <espconn.h>
#include <os_type.h>

// WebSocket protocol constants
// First byte
#define WS_FIN            0x80
#define WS_OPCODE_TEXT    0x01
#define WS_OPCODE_BINARY  0x02
#define WS_OPCODE_CLOSE   0x08
#define WS_OPCODE_PING    0x09
#define WS_OPCODE_PONG    0x0a
// Second byte
#define WS_MASK           0x80
#define WS_SIZE16         126
#define WS_SIZE64         127


#define WS_DEFAULT_TIMEOUT_IN_MS 30000

//#define WS_DEBUG


enum WebSocketClientStatus {
	WebSocketClientConnected,
	WebSocketClientDnsError,
	WebSocketClientDisconnected,//by server, or internet connection
	WebSocketConnectionTimeoutError,// by timeout
	WebSocketClientHandshakeError,
	WebSocketClientFrameParseError
};

struct WebSocketClient;

typedef void(*WebSocketClientStatusChangeCallback)(struct WebSocketClient *client, enum WebSocketClientStatus status);
typedef void(*WebSocketClientDataReceiveCallback)(struct WebSocketClient *client, uint8_t opcode, char* data, unsigned int length);

typedef struct WebSocketClient {
	struct espconn	_conn; // has the same address like "struct WebSocketClient"
	esp_tcp			_tcp;
	struct ip_addr  _serverAddress;
	
	bool			_connected;
	bool			_handshakeInited;
	char			_key[25];//TODO dynamic alloc...
	
	uint32			timeout;//in ms
	os_timer_t		_timeoutTimer;

	const char* _host;
	const char* _path;
	uint32 _port;
	//bool secure

	WebSocketClientStatusChangeCallback statusChangeCallback;
	WebSocketClientDataReceiveCallback dataReceiveCallback;
} WebSocketClient;




void websocket_client_init(WebSocketClient *client);

bool websocket_client_connect(WebSocketClient *client, const char* host, uint32 port, const char* path);

bool websocket_client_send(WebSocketClient *client, char *data, unsigned int length, uint8_t opcode);

bool websocket_client_disconnect(WebSocketClient *client);

bool websocket_client_is_connected(WebSocketClient *client);


#endif