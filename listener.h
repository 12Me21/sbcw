////////////////////////////////////////////
// JSON parser listener for chat messages //
////////////////////////////////////////////

namespace Userlist {
	const int WIDTH = 4*4;
	const int PER_ROW = SCW/WIDTH;
	const int SIZE = PER_ROW*2;
	
	int16_t uid[SIZE] = {};
	char name[SIZE][30];
	bool active[SIZE] = {};
	uint8_t length=0;

	int16_t drawn_uid[SIZE] = {};
	bool drawn_active[SIZE] = {};
	bool redraw=true;

	void draw(int index){
		if(drawn_uid[index]!=uid[index] || drawn_active[index]!=active[index]){
			tft.fillRect(index%PER_ROW*WIDTH, index/PER_ROW*Font::line_height, WIDTH, Font::line_height, active[index]?0:rgb(32,32,32));
			if(index<length){
				name[index][4]='\0';
				draw_text(index%PER_ROW*WIDTH, index/PER_ROW*Font::line_height, name[index], name_color(uid[index]));
				drawn_uid[index]=uid[index];
				drawn_active[index]=active[index];
			}else{
				drawn_uid[index]=uid[index]=0;
			}
		}
	}
	
	void draw(){
		if(!redraw)
			return;
		uint8_t i;
		for(i=0;i<SIZE;i++)
			draw(i);
		redraw = false;
	}
};

//Current Message being parsed
namespace CMsg {
	//data for the message or user list item that is currently being parsed
	char message[3001]; //max message length is basically 3000 chars. Technically messages can be ~30000 but the website input form has a max length 3000 so you'll rarely see longer.
	char username[30] = {'\0'}; //max username length is 20 chars but whatever
	int uid;
	enum room {roomUnset=0,roomProgramming,roomOff_topic,roomStaff,roomAny,roomOther} room;
	enum encoding {encUnset=0,encText,encDraw} encoding;
	enum subtype {subUnset=0,subJoin,subLeave,subWelcome,subWarning,subBlocked,subNone} subtype;
	enum type {typeUnset=0,typeWarning,typeSystem,typeModule,typeMessage} type;
	int8_t sender_active;
	uint64_t id;

	void reset(){
		message[0] = '\0';
		username[0] = '\0';
		uid = 0;
		room = roomUnset;
		encoding = encUnset;
		subtype = subUnset;
		type = typeUnset;
		id = 0;
		sender_active = -1;
	}
}

const char * room_chars = "efdn"; //gEneral oFftopic aDmin aNy
enum CMsg::room room_number(char room_char_2){
	char * x = strchr(room_chars,room_char_2);
	return x?(enum CMsg::room)(x-room_chars+1):CMsg::roomOther;
}

//dealing with message ids
uint64_t message_ids[5][10] = {}; //this stores the past 10 message ids from each room. pm rooms are grouped together because 12Me23 won't be in any pm rooms anyway.
int message_id_pointer[5] = {0,0,0,0,0};
bool is_new_message(uint8_t room, uint64_t id){
	for(uint8_t i=0;i<10;i++)
		if(message_ids[room][i]==id)
			return false;
	message_ids[room][message_id_pointer[room]]=id;
	message_id_pointer[room]=(message_id_pointer[room]+1)%10;
	return true;
}
//read a 64 bit integer
uint64_t read_64(char * x){
	uint64_t y = 0;
	while(*x){
		y *= 10;
		y += *x++-'0';
	}
	return y;
}

void finish_message(){
	if(CMsg::message[0] && CMsg::username[0] && CMsg::uid && CMsg::room && CMsg::encoding && CMsg::subtype && CMsg::type && CMsg::id){
		if (!is_new_message(CMsg::room-1,CMsg::id)) return;

		if(CMsg::sender_active!=-1)
			for(uint8_t i=0;i<Userlist::length;i++)
				if(Userlist::uid[i]==CMsg::uid){
					if(Userlist::active[i]!=CMsg::sender_active){
						Userlist::active[i]=CMsg::sender_active;
						Userlist::redraw=true;
					}
					break;
				}
		
		Displ::text_color = name_color(CMsg::uid);
		Displ::bg_color = room_colors[CMsg::room-1];
		//messages (with username)
		unescape_html(CMsg::message);
		Displ::is_first=true;
		if(CMsg::type==CMsg::typeMessage){
			Displ::print(CMsg::username);
			Displ::print(':');
			Displ::print(' ');
			if(CMsg::encoding == CMsg::encDraw){
				Displ::text_color = 0b0111101111101111;
				Displ::print("[drawing]");
			}else{
				Displ::print(CMsg::message);
			}
		//system/module messages
		}else{
			//join/leave
			switch(CMsg::subtype){
				case CMsg::subJoin:
					Displ::print(CMsg::username);
					Displ::print(" has entered.");
				break;case CMsg::subLeave:
					Displ::print(CMsg::username);
					Displ::print(" has left.");
				break;default:
					Displ::print(CMsg::message);
			}
		}
		Displ::print('\n');
	}else{
		Serial.println("error: missing some message info");
	}
}

bool bind_response = false;
bool response_result;

//JSON state
enum js {
	jsStart,jsMain,jsMain_type,jsMain_from,jsMain_result,
	jsUsers,jsUsers_array,jsUser,jsUser_username,jsUser_uid,jsUser_active,
	jsMessages,jsMessages_array,jsMessage,jsMessage_tag,jsMessage_encoding,jsMessage_subtype,jsMessage_sender,jsMessage_type,jsMessage_id,jsMessage_message,
	jsSender,jsSender_username,jsSender_uid,jsSender_active,
};
enum js js = jsStart;

void json_error(char * msg){
	Serial.print("JSON error: ");
	Serial.println(msg);
	Serial.println(js);
	parser.buffer = json_buffer;
	parser.buffer_size = 512;
}

class ChatJsonListener: public JsonListener {
	public:
	void startDocument(){
		js = jsStart;
		bind_response = false;
	}
	void endDocument(){
		if(js!=jsStart)
			json_error("end doc");
	}
	void startObject(){
		switch(js){
			case jsUsers_array:
				Userlist::name[Userlist::length][0]='\0';
				Userlist::uid[Userlist::length]=-1;
				Userlist::active[Userlist::length]=false;
				js=jsUser;
			break;case jsMessages_array:
				js=jsMessage;
				CMsg::reset();
			break;case jsMessage_sender:
				js=jsSender;
			break;case jsStart:
				js=jsMain;
			break;default:
				json_error("start object");
		}
	}
	void endObject(){
		switch(js){
			case jsUser:
				//todo: check username and uid to make sure they were read
				if(Userlist::uid[Userlist::length]!=BOT_UID){ //411 = 12Me23
					Userlist::redraw = true;
					if(Userlist::length++ >= Userlist::SIZE){
						Userlist::length=0;
						Serial.println("Error: User list full!");
					}
				}
				js=jsUsers_array;
			break;case jsMessage:
				finish_message();
				js=jsMessages_array;
			break;case jsSender:
				js=jsMessage;
			break;case jsMain:
				js=jsStart;
			break;default:
				json_error("end obj");
		}
	}
	void startArray(){
		switch(js){
			case jsUsers:
				Userlist::length = 0;
				//csrx2=0;
				//csry2=0;
				js=jsUsers_array;
			break;case jsMessages:
				js=jsMessages_array;
			break;default:
				json_error("start array");
		}
	}
	void endArray(){
		switch(js){
			case jsUsers_array:
				//after printing user list, print an empty line to fill ssssssssssss
				//if(csry2==0)
					//Displ("\n ",csrx2,csry2);
			case jsMessages_array: //no break
				js=jsMain;
			break;default:
				json_error("end array");
		}
	}
	bool key(char * key){
		switch(js){
			case jsMain:
				if(seq(key,"users"))         js=jsUsers;
				else if(seq(key,"messages")) js=jsMessages;
				else if(seq(key,"type"))     js=jsMain_type;
				else if(seq(key,"from"))     js=jsMain_from;
				else if(seq(key,"result"))   js=jsMain_result;
				else                         return false;
			break;case jsUser:
				if(seq(key,"username"))      js=jsUser_username;
				else if(seq(key,"uid"))      js=jsUser_uid;
				else if(seq(key,"active"))   js=jsUser_active;
				else                         return false;
			break;case jsSender:
				if(seq(key,"username"))      js=jsSender_username;
				else if(seq(key,"uid"))      js=jsSender_uid;
				else if(seq(key,"active"))   js=jsSender_active;
				else                         return false;
			break;case jsMessage:
				if(seq(key,"tag"))           js=jsMessage_tag;
				else if(seq(key,"encoding")) js=jsMessage_encoding;
				else if(seq(key,"subtype"))  js=jsMessage_subtype;
				else if(seq(key,"sender"))   js=jsMessage_sender;
				else if(seq(key,"type"))     js=jsMessage_type;
				else if(seq(key,"id"))       js=jsMessage_id;
				else if(seq(key,"message")){
					parser.buffer = CMsg::message;
					parser.buffer_size = 3001;
					js=jsMessage_message;
				}else
					return false;
			break;default:
				json_error("key");
				return false;
		}
		return true;
	}
	virtual void value(char * value){
		switch(js){
			case jsUser_username:
				strcpy(Userlist::name[Userlist::length],value);
				js=jsUser;
			break;case jsUser_uid:
				Userlist::uid[Userlist::length] = atoi(value);
				js=jsUser;
			break;case jsUser_active:
				Userlist::active[Userlist::length] = seq(value,"true");
				js=jsUser;
			break;case jsSender_username:
				strcpy(CMsg::username,value); 
				js=jsSender;
			break;case jsSender_uid:
				CMsg::uid=atoi(value);
				js=jsSender;
			break;case jsSender_active:
				CMsg::sender_active = seq(value,"true");
				js=jsSender;
			break;case jsMain_type:
				js=jsMain;
			break;case jsMain_from:
				if(seq(value,"bind"))
					bind_response=true;
				js=jsMain;
			break;case jsMain_result:
				response_result = !seq(value,"false");
				js=jsMain;
			break;case jsMessage_tag:
				CMsg::room = room_number(value[1]);
				js=jsMessage;
			break;case jsMessage_encoding:
				if(seq(value,"draw")) CMsg::encoding = CMsg::encDraw;
				else                  CMsg::encoding = CMsg::encText;
				//todo: check other encodings
				js=jsMessage;
			break;case jsMessage_subtype:
				if(seq(value,"join"))       CMsg::subtype = CMsg::subJoin;
				else if(seq(value,"leave")) CMsg::subtype = CMsg::subLeave;
				else                        CMsg::subtype = CMsg::subNone;
				//todo: check other subtypes
				js=jsMessage;
			break;case jsMessage_type:
				if(seq(value,"message"))      CMsg::type = CMsg::typeMessage;
				else if(seq(value,"system"))  CMsg::type = CMsg::typeSystem;
				else if(seq(value,"module"))  CMsg::type = CMsg::typeModule;
				else if(seq(value,"warning")) CMsg::type = CMsg::typeWarning;
				js=jsMessage;
			break;case jsMessage_id:
				CMsg::id = read_64(value);
				js=jsMessage;
			break;case jsMessage_message:
				parser.buffer=json_buffer;
				parser.buffer_size=512;
				js=jsMessage;
			break;default:
				json_error("value");
		}
	}
};
