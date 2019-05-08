class RequestJsonListener: public JsonListener {
	public:
	bool key(char * key){
		return seq(key,"result");
	}
	void startArray(){}
	void endArray(){}
	void startObject(){}
	void endObject(){}
	void startDocument(){}
	void endDocument(){}
	void value(char * value){
		got_result = true;
		if(seq(value,"false")){
			result_good = false;
		}else{
			result_good = true;
			if(getting_auth){
				memcpy(chat_bind_json+8,value,strlen(value));
			}else{
				memcpy(auth_request_url+67,value,strlen(value));
			}
		}
	}
	void init(bool is_auth){
		getting_auth = is_auth;
		got_result = false;
	}
	bool got_result_key;
	bool getting_auth;
	bool got_result;
	bool result_good;
	char * auth_request_url = "http://smilebasicsource.com/query/request/chatauth?small=1&session=\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
};
