## ESP8266 websocket client
This is a library that implements the websockets client using the NONOS SDK.
When I wrote it relied on: https://github.com/morrissinger/ESP8266-Websocket
The repository contains an example usage of client in the _user_main.c_ file.

### Features
- client based on _struct espconn_
- client is asynchronous
- timeout detection on client side
- auto _PONG_ response

### TODO:
- merging multiple message fragments into one(useful in JSON data type)
- wss
