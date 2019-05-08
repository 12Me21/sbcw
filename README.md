Changes:
- removed "connections.h" (contains wifi names/passwords)
- removed password md5 hash


This requires several modified libraries (as well as the custom hardware of course)
- Json_Streaming_Parser (slightly modified)
- TFT_ST7735 (fixed hardware scrolling, etc.)
- WebSockets (add support for receiving messages that wouldn't fit into RAM, by parsing them as the packets are recieved)
