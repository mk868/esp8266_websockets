/*
	ESP8266 websocket client
	version: 0.1
	GIT: https://github.com/mkuch95/esp8266_websockets
*/

#include "websocket_client.h"

#include <user_interface.h>
#include <string.h>
#include <osapi.h>
#include <mem.h>

#include "sha1.h"
#include "Base64.h"


#ifdef WS_DEBUG
	#define websocket_client_log(format, ...) os_printf("websocket_client:%s: "format"\n", __FUNCTION__, ##__VA_ARGS__)
#else
	#define websocket_client_log(format, ...)
#endif // WS_DEBUG



//connection timeout
static void timer_timeout_cb(void *arg);
static void stop_timer(WebSocketClient* client);
static void bring_up_timer(WebSocketClient* client);

//conn events
static void connect_cb(void *arg);
static void disconnect_cb(void *arg);
static void recon_cb(void *arg, sint8 err);
static void receive_data_cb(void *arg, char *pdata, unsigned short len);
static void sent_cb(void *arg);


static void dns_result_cb(const char *name, ip_addr_t *ipaddr, void *arg);
static void connect_to_host(WebSocketClient * client);
static void make_handshake(WebSocketClient * client);
static bool analyze_handshake_result(WebSocketClient * client, char* data, unsigned short len);


struct handle_websocket_data_result {
	unsigned int length;
	uint8_t opcode;
	bool error;
	char* data;
};
static struct handle_websocket_data_result handle_websocket_data(char* data, unsigned short len);



static void ICACHE_FLASH_ATTR timer_timeout_cb(void *arg)
{
	WebSocketClient *client = (WebSocketClient*)arg;
	websocket_client_log("client timeout");

	if (!client->_connected) {
		return;
	}

	espconn_disconnect(&(client->_conn));//FIXME send 0x88???
	client->_connected = false;

	client->statusChangeCallback(client, WebSocketConnectionTimeoutError);
}

static void ICACHE_FLASH_ATTR stop_timer(WebSocketClient* client) {
	os_timer_disarm(&(client->_timeoutTimer));
}

static void ICACHE_FLASH_ATTR bring_up_timer(WebSocketClient* client) {
	websocket_client_log("bring_up_timer");
	os_timer_disarm(&(client->_timeoutTimer));
	os_timer_setfn(&(client->_timeoutTimer), (os_timer_func_t *)timer_timeout_cb, client);
	os_timer_arm(&(client->_timeoutTimer), client->timeout, false);
}




static void ICACHE_FLASH_ATTR connect_cb(void *arg) {
	WebSocketClient *client = (WebSocketClient*)arg;

	websocket_client_log("CONNECTED!");

	client->_connected = true;

	bring_up_timer(client);

	make_handshake(client);
}

static void ICACHE_FLASH_ATTR disconnect_cb(void *arg) {
	WebSocketClient *client = (WebSocketClient*)arg;

	websocket_client_log("DISCONNECTED");
	client->_connected = false;

	stop_timer(client);

	client->statusChangeCallback(client, WebSocketClientDisconnected);
}

static void ICACHE_FLASH_ATTR recon_cb(void *arg, sint8 err) {
	WebSocketClient *client = (WebSocketClient*)arg;

	websocket_client_log("error: %d - DISCONNECTED", err);
	client->_connected = false;

	stop_timer(client);

	client->statusChangeCallback(client, WebSocketClientDisconnected);
}

static void ICACHE_FLASH_ATTR receive_data_cb(void *arg, char *pdata, unsigned short len) {
	WebSocketClient *client = (WebSocketClient*)arg;


	if (!client->_handshakeInited) {
		client->_handshakeInited = true;

		if (analyze_handshake_result(client, pdata, len)) {
			bring_up_timer(client);
			client->_connected = true;

			client->statusChangeCallback(client, WebSocketClientConnected);
		}
		else {
			client->statusChangeCallback(client, WebSocketClientHandshakeError);

			espconn_disconnect(&(client->_conn));
		}

		return;
	}

	bring_up_timer(client);

	struct handle_websocket_data_result data = handle_websocket_data(pdata, len);

	if (data.error) {
		client->statusChangeCallback(client, WebSocketClientFrameParseError);
		return;
	}

	if (data.opcode & WS_OPCODE_PING == WS_OPCODE_PING) {

		websocket_client_send(client, NULL, 0, WS_OPCODE_PONG);
	}


	client->dataReceiveCallback(client, data.opcode, data.data, data.length);
}

static void ICACHE_FLASH_ATTR sent_cb(void *arg) {
	WebSocketClient *client = (WebSocketClient*)arg;

	bring_up_timer(client);

	websocket_client_log("sent_cb");
}


static void ICACHE_FLASH_ATTR dns_result_cb(const char *name, ip_addr_t *ipaddr, void *arg) {
	WebSocketClient *client = (WebSocketClient*)arg;

	if (ipaddr == NULL) {
		//Critical error...
		os_printf("DNS resolve error, host: %s", name);

		client->statusChangeCallback(client, WebSocketClientDnsError);
	}
	else {
		client->_serverAddress.addr = ipaddr->addr;

		websocket_client_log("ip resolved!");

		connect_to_host(client);
	}
}


static void ICACHE_FLASH_ATTR connect_to_host(WebSocketClient * client) {
	websocket_client_log("connecting to: "IPSTR" (%s)", IP2STR(&(client->_serverAddress)), client->_host);

	client->_tcp.remote_port = client->_port;
	client->_tcp.local_port = espconn_port();//empty port

	os_memcpy(client->_tcp.remote_ip, &(client->_serverAddress.addr), 4);//remote ip

	struct ip_info ipconfig;
	wifi_get_ip_info(STATION_IF, &ipconfig);//TODO: move to init
	os_memcpy(client->_tcp.local_ip, &ipconfig.ip, 4);//local ip

	espconn_connect(&(client->_conn));
}

static struct handle_websocket_data_result ICACHE_FLASH_ATTR handle_websocket_data(char* data, unsigned short len) {//FIXME checking length of message
	struct handle_websocket_data_result result;
	result.error = false;

	uint8_t msgtype;
	unsigned int length;
	uint8_t mask[4];
	unsigned int i;
	uint32 pos = 0;
	bool hasMask = false;


	msgtype = data[pos++];

	length = data[pos++];

	if (length & WS_MASK) {
		hasMask = true;
		length = length & ~WS_MASK;
	}

	if (length == WS_SIZE16) {
		length = data[pos++] << 8;

		length |= data[pos++];
	}
	else if (length == WS_SIZE64) {
		os_printf("No support for over 16 bit sized messages");
		result.error = true;
		return result;
	}

	if (hasMask) {
		// get the mask
		mask[0] = data[pos++];
		mask[1] = data[pos++];
		mask[2] = data[pos++];
		mask[3] = data[pos++];
	}

	result.opcode = msgtype;// &~WS_FIN;
	result.length = length;

	//result.data = (char*)malloc(length);
	result.data = NULL;
	if (length > 0)
		result.data = (data + pos);//let use existing buffer

	if (hasMask) {
		for (i = 0; i < length; ++i) {
			//result.data[i] = (char)(data[pos++] ^ mask[i % 4]);
			result.data[i] = (char)(data[pos++] ^ mask[i % 4]);
		}
	}
	else {
		/*for (i = 0; i < length; ++i) {
			result.data[i] = (char)data[pos++];
		}*/
	}

	return result;

}

static void ICACHE_FLASH_ATTR make_handshake(WebSocketClient * client) {
	char buffer[200];
	char keyStart[17];
	char b64Key[25];
	os_bzero(client->_key, 25);

	os_get_random(keyStart, 16);

	base64_encode(b64Key, keyStart, 16);

	int i;
	for (i = 0; i < 24; ++i) {
		client->_key[i] = b64Key[i];
	}

	os_sprintf(buffer,
		"GET %s HTTP/1.1\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Host: %s:%d\r\n"
		"Sec-WebSocket-Key: %s\r\n"
		"Sec-WebSocket-Version: 13\r\n"
		"\r\n",
		client->_path,
		client->_host, client->_port,
		client->_key
	);

	espconn_send(&(client->_conn), buffer, strlen(buffer));
}

static bool ICACHE_FLASH_ATTR analyze_handshake_result(WebSocketClient* client, char* data, unsigned short len) {

	char* pos = strstr(data, "Sec-WebSocket-Accept: ");

	if (pos == NULL) {
		return false;
	}
	pos += 22;//strlen("Sec-WebSocket-Accept: ");

	char serverKey[40];
	os_bzero(serverKey, 40);

	strncpy(serverKey, pos, (uint32)strstr(pos, "\r\n") - (uint32)pos);

	websocket_client_log("serverKey: '%s'", serverKey);


	char buffKey[100];

	os_sprintf(buffKey, "%s%s", client->_key, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

	uint8_t *hash;
	char result[21];
	char b64Result[30];

	SHA1Context sha;
	int err;
	uint8_t Message_Digest[20];

	err = SHA1Reset(&sha);
	err = SHA1Input(&sha, (const uint8_t*)buffKey, strlen(buffKey));
	err = SHA1Result(&sha, Message_Digest);
	hash = Message_Digest;

	int i;
	for (i = 0; i < 20; ++i) {
		result[i] = (char)hash[i];
	}
	result[20] = '\0';

	base64_encode(b64Result, result, 20);

	websocket_client_log("'%s' == '%s'", serverKey, b64Result);

	return strncmp(serverKey, b64Result, strlen(serverKey)) == 0;
}




void ICACHE_FLASH_ATTR websocket_client_init(WebSocketClient * client)
{
	client->_conn.type = ESPCONN_TCP;
	client->_conn.state = ESPCONN_NONE;
	client->_conn.proto.tcp = &client->_tcp;
	client->timeout = WS_DEFAULT_TIMEOUT_IN_MS;

	client->_connected = false;
	client->dataReceiveCallback = NULL;
	client->statusChangeCallback = NULL;

	//espconn_regist_time(&(client->_conn1), 10, true);//TODO
	espconn_regist_connectcb(&(client->_conn), connect_cb);
	espconn_regist_disconcb(&(client->_conn), disconnect_cb);
	espconn_regist_reconcb(&(client->_conn), recon_cb);
	espconn_regist_recvcb(&(client->_conn), receive_data_cb);
	espconn_regist_sentcb(&(client->_conn), sent_cb);
}

bool ICACHE_FLASH_ATTR websocket_client_connect(WebSocketClient *client, const char* host, uint32 port, const char* path)
{
	if (client->_connected) {
		os_printf("already connected!");
		return false;
	}

	client->_host = host;
	client->_port = port;
	client->_path = path;
	client->_handshakeInited = false;
	client->_connected = false;
	err_t resp = espconn_gethostbyname(&(client->_conn), host, &(client->_serverAddress), dns_result_cb);//resolve dns

	if (resp == ESPCONN_OK) {
		websocket_client_log("ip from cache");

		connect_to_host(client);
		return true;
	}

	if (resp == ESPCONN_ARG) {//critical error...
		os_printf("host not valid!");
		return false;
	}

	return true;//IN_PROGRESS
}

bool ICACHE_FLASH_ATTR websocket_client_send(WebSocketClient *client, char* data, unsigned int length, uint8_t opcode)
{
	if (!client->_connected) {
		return false;
	}

	char* buff = (char*)os_malloc(length + 10);
	int buffPos = 0;

	uint8_t mask[4];

	// Opcode; final fragment
	buff[buffPos++] = opcode | WS_FIN;

	// NOTE: no support for > 16-bit sized messages
	if (length > 125) {
		buff[buffPos++] = WS_SIZE16 | WS_MASK;
		buff[buffPos++] = (uint8_t)(length >> 8);
		buff[buffPos++] = (uint8_t)(length & 0xFF);
	}
	else {
		buff[buffPos++] = (uint8_t)length | WS_MASK;
	}



	os_get_random(mask, 4);

	buff[buffPos++] = mask[0];
	buff[buffPos++] = mask[1];
	buff[buffPos++] = mask[2];
	buff[buffPos++] = mask[3];

	int i;
	for (i = 0; i < length; ++i) {
		buff[buffPos++] = data[i] ^ mask[i % 4];
	}

	espconn_send(&(client->_conn), buff, buffPos);
	websocket_client_log("sending %d bytes", buffPos);

	os_free(buff);

	return true;
}

bool ICACHE_FLASH_ATTR websocket_client_disconnect(WebSocketClient * client)
{
	if (!client->_connected) {
		return false;
	}

	websocket_client_send(client, NULL, 0, WS_FIN | WS_OPCODE_CLOSE);

	os_delay_us(10000);

	espconn_disconnect(&(client->_conn));
	return true;
}

bool ICACHE_FLASH_ATTR websocket_client_is_connected(WebSocketClient * client)
{
	return client->_connected;
}
