#define STR_EXPAND(tok) #tok
#define STR(tok) STR_EXPAND(tok)
#define BOT_UID 411
#define seq(x,y) !strcmp(x,y)
#define rgb(r,g,b) (r>>3<<(5+6)|g>>2<<5|b>>3) //standard to 565 format. If this isn't precomputed I swear....
#define ceil_div(a,b) ((a)/(b) + ((a)%(b)!=0))

typedef uint16_t Color;

//screen
#include <TFT_ST7735.h>
#define SCW 128 //screen width, height
#define SCH 128
#define TFA 13 //top fixed (non-scrolling) area
#define BFA 0 //bottom fixed area
#define NFA (SCH-TFA-BFA) //"non-fixed" (scrolling) area (I wanted all the names to end in "FA" so...)
#define MESSAGE_AREA_WIDTH (SCW-2)
TFT_ST7735 tft = TFT_ST7735(D2, D3, D4);
#include "print.h"

//connection
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
ESP8266WiFiMulti wifi_multi;
#include <WebSocketsClient.h>
WebSocketsClient websocket;
#include <ESP8266HTTPClient.h>

//json
#include <JsonStreamingParser.h>
JsonStreamingParser parser;
char json_buffer[512];

char * chat_bind_json = "{\"key\":\"................\",\"type\":\"bind\",\"uid\":" STR(BOT_UID) ",\"lessData\":true}";
#include "request.h"

#include "listener.h"
ChatJsonListener listener;

//Websocket callback functions
void websocket_char(uint8_t chr){
	parser.parse((char)chr);
}
 void websocket_event(WStype_t type, uint8_t * data, size_t length){
	switch(type){
		case WStype_DISCONNECTED:
			Displ::text_color = rgb(255,0,0);
			Displ::bg_color = 0;
			Displ::print("Disconnected\n");
		break;case WStype_CONNECTED:
			websocket.sendTXT(chat_bind_json);
			Displ::print("Connected\n");
		break;case WStype_TEXT:
			parser.reset();
			if(bind_response){
				if(response_result)
					websocket.sendTXT("{\"type\":\"request\",\"request\":\"messageList\"}");
				else{
					Displ::bg_color = 0;
					Displ::text_color = rgb(255,0,0);
					Displ::print("Bind failed\n");
					websocket.disconnect();
				}
			}
	}
}

//int last_update;

#include <Ticker.h>
Ticker update_screen_ticker;

const char * wifi_status_name[] = {
	"Idle","No SSID","Scan Completed","Connected","Connection Failed","Connection Lost","Disconnected"
};

//button debounce system
class Button {
	private:
		bool old_raw = false; //previous raw state
		unsigned long start = 0; //time (ms) of last raw state change
	public:
		bool state = false; //current debounced state
		bool old = false; //previous debounced state (for detecting start/end of press)
		const uint8_t pin;
		//const uint16_t delay;
		Button(uint8_t pin/*, uint16_t delay = 10*/):pin(pin){}
		void update(){
			old = state;
			bool state_raw = digitalRead(pin) == LOW;
			if(state_raw != state){ //if the true button state is different from .state
				if(state_raw != old_raw) //if the raw state has changed
					start = millis();
				else if(millis() - start > 10);
					state = state_raw;
			}
			old_raw = state_raw;
		}
};

Button button1(D1);
Button button0(D6);

int8_t vel = 0;
bool auto_scroll = true;
bool scroll_down = false;

void io(){
	button1.update();
	button0.update();
	
	if(button1.state){
		vel+=1;
		if(vel>10)
			vel=10;
		auto_scroll=false;
	}else if(button0.state){
		vel-=1;
		if(vel<-10)
			vel=-10;
		auto_scroll=false;
	}else{
		vel=0;
	}
	
	if(next_scroll==0)
		auto_scroll=true;
	
	if(auto_scroll && next_scroll){
		vel = -(next_scroll/10);
		if(vel > -2)
			vel = -2;
	}
	
	next_scroll += vel;
	
	if(next_scroll<0){
		next_scroll=0;
		vel=0;
	}
	if(total_height<NFA){
		next_scroll=0;
	}else if(next_scroll>total_height-NFA){
		next_scroll = total_height-NFA;
	}
	
	change_scroll(next_scroll);

	Userlist::draw();
}

void setup() {
	
	pinMode(D1,INPUT_PULLUP);
	//pinMode(D6,INPUT_PULLUP);
	
	Serial.begin(115200);
	
	//init screen
	tft.begin();
	tft.setRotation(ROT_180);
	tft.defineScrollArea(TFA,BFA);
	tft.drawFastHLine(0,TFA-1,SCW,rgb(255,255,255));
	pinMode(D6,INPUT_PULLUP); //D6 is used as MISO (master in, slave out) by SPI (but we don't ever use that since we only send data TO the screen) so this SHOULD be fine
	//no idea why D0 didn't work ...
	
	update_screen_ticker.attach_ms(1000/50, io); //update input/output 50x per second
	
	//Connect to wifi
	//Displ::print("Waiting...\n");
	//delay(4000);
	Displ::text_color=rgb(0,224,255);
	Displ::bg_color=rgb(64,64,64);
	Displ::println("* 12Me21 SBS Chat Viewer *");
	
	Displ::text_color=rgb(255,255,255);
	Displ::bg_color=rgb(0,0,0);
	
	Displ::println("Connecting to wifi...");
	WiFi.mode(WIFI_STA);
	WiFi.setAutoReconnect(true);
	#include "connections.h"
	wl_status_t wifi_status;
	do{
		wifi_status = wifi_multi.run();
		Displ::print("Status: ");
		//Serial.println((int)wifi_status);
		Displ::println(wifi_status_name[wifi_status]);
		delay(500);
	}while(wifi_status != WL_CONNECTED);
	
	Displ::print("Connected to: ");
	Displ::print(WiFi.SSID().c_str());
	Displ::print('\n');
	Displ::print("Local IP: ");
	Displ::println(WiFi.localIP().toString().c_str());
	
	WiFiClient wifi_client;
	
	//prepare JSON parser for parsing HTTP response JSON
	const char * response_json;
	parser.buffer = json_buffer;
	parser.buffer_size = 512;
	RequestJsonListener request_listener;
	parser.listener = &request_listener;
	
	HTTPClient http;
	
	//Log in
	while(1){
		Displ::println("Logging in to SBS...");
		http.begin(wifi_client,"http://smilebasicsource.com/query/submit/login?session=x&small=1");
		http.addHeader("Content-Type","application/x-www-form-urlencoded");
		if(http.POST("username=12Me23&password=[########## REDACTED ##########]")==HTTP_CODE_OK){ //(redacted password MD5 hash)
			request_listener.init(false);
			response_json = http.getString().c_str();
			while(*response_json && !request_listener.got_result)
				parser.parse(*response_json++);
			if(request_listener.got_result && request_listener.result_good){
				Displ::println("Logged in");
				break;
			}else{
				Displ::println("Login failed");
				//Displ::print(http.getString().c_str());
				//Displ::finish();
			}
		}else{
			Displ::println("Logged request failed");
		}
		http.end();
		delay(2000);
	}
	
	//Get chat auth
	while(1){
		Displ::println("Requesting auth key...");
		http.begin(wifi_client, request_listener.auth_request_url);
		if(http.GET()==HTTP_CODE_OK){
			//Serial.println("ok what");
			parser.reset();
			request_listener.init(true);
			response_json = http.getString().c_str();
			//Serial.println(response_json);
			while(*response_json && !request_listener.got_result)
				parser.parse(*response_json++);
			if(request_listener.got_result && request_listener.result_good){
				Displ::println("Got chat auth key");
				break;
			}else{
				Displ::println("Failed to get chat auth");
			}
		}else{
			Displ::println("Auth request failed");
		}
		http.end();
		delay(2000);
	}
	
	parser.reset();
	//start websocket
	parser.listener = &listener;
	Displ::println("Connecting to websocket...");
	
	websocket.begin("chat.smilebasicsource.com",45695,"chatserver");
	websocket.onEvent(websocket_event,websocket_char);
	websocket.setReconnectInterval(0);
	websocket.loop();
	websocket.setReconnectInterval(999999);
}

void loop() {
	websocket.loop();
	//io();
}
