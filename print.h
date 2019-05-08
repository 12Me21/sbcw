Color room_colors[] = {rgb(0,0,96),rgb(96,0,0),rgb(0,32,0),rgb(0,0,0),rgb(255,128,255)};

//choose a random color based on a seed

#define MIN_BRIGHTNESS 192
Color name_color(int uid){
	randomSeed(uid);
	uint16_t r=random(256);
	uint16_t g=random(256);
	uint16_t b=random(256);
	uint16_t avg=(r+r+r+g+g+g+g+b);
	if(avg<MIN_BRIGHTNESS*8){
		r=r*MIN_BRIGHTNESS*8/avg;
		g=g*MIN_BRIGHTNESS*8/avg;
		b=b*MIN_BRIGHTNESS*8/avg;
	}
	return rgb(r,g,b);
		//(random(0x10000) & 0b0011100111101111);
	//return ~(random(0x10000) | 0b0100001000001000);
}
//double a = -0XC.90FEP-2;

//really bad/dangerous html escape sequence handler.
//could overwrite the string terminator and crash everything.
//but works fine if you assume message only contains &amp; &lt; &gt; &apos; and &quot;
//and your text display system must support ascii DEL properly (ignoring it)
void unescape_html(char * html){
	while(*html){
		if(*html=='&'){
			*html++ = (char)127;
			switch(*html){
				case 'a':
					*html++ = (char)127;
					if(*html=='p'){
						*html++ = '\'';
						*html++ = (char)127;
						*html++ = (char)127;
						*html   = (char)127;
					}else{
						*html++ = '&';
						*html++ = (char)127;
						*html   = (char)127;
					}
				break;case 'l':
					*html++ = '<';
					*html++ = (char)127;
					*html   = (char)127;
				break;case 'g':
					*html++ = '>';
					*html++ = (char)127;
					*html   = (char)127;
				break;case 'q':
					*html++ = '"';
					*html++ = (char)127;
					*html++ = (char)127;
					*html++ = (char)127;
					*html   = (char)127;
			}
		}
		html++;
	}
}

#include "font.h"

#define LINES 500 //# of lines of text to save

typedef uint16_t LineIndex; //index in the text lines buffer

typedef int16_t VirtualY; //this is for positions relating to the scroll position. units: pixels. range: small negative numbers to maybe like 10000?
typedef int16_t ScreenPos; //positions on the screen

VirtualY drawn_scroll = -9999;
VirtualY next_scroll = 0;

uint8_t line_type[LINES] = {}; //0=unused, 1=normal, 2=gap above
Color line_color[LINES];
Color line_bg_color[LINES];
char * lines[LINES] = {}; //bytes per line: min: 4+1+alloc. max: 4+width+1+alloc
//char lines[LINES][SCW/2+1];
LineIndex next_line = 0;
//LineIndex oldest_line=0;
VirtualY total_height = 0;

//todo: make sure nothing bad happens if the display update interrupt happens during this
void push_line(const char * text, uint16_t length, Color text_color, Color bg_color, bool is_first){
	//delete old line
	switch(line_type[next_line]){
		case 2:
			total_height--;
		case 1:
			total_height-=Font::line_height;
	}
	if(lines[next_line])
		free(lines[next_line]);
	//make a copy of the text
	if(lines[next_line] = (char *)malloc(length+1)){
		memcpy(lines[next_line],text,length);
		lines[next_line][length] = '\0';
		
		line_color[next_line] = text_color;
		line_bg_color[next_line] = bg_color;

		line_type[next_line] = is_first?2:1; //last
		
		uint8_t row_height = Font::line_height+(is_first?1:0);
		drawn_scroll += row_height;
		next_scroll += row_height;
		total_height += row_height;
		
		next_line++;
		if(next_line==LINES)
			next_line = 0;

		
	}else{
		//todo: try freeing more old lines?
		Serial.println("Could not allocate memory for line!");
	}
}

// Usage idea:
//Displ::start(text color, background color); //mark the beginning of a message, and set the colors
//Displ::print("whatever"); //print things
//Displ::print('.');
//Displ::print("Hello, World!\n");
//Displ::print("12345");
//Displ::finish(); //flush the line buffer (in case the last message didn't end with \n)

namespace Displ {
	char line_buffer[MESSAGE_AREA_WIDTH/2+1]; //length = max # of chars per row +1 just in case. 2 = min(char_width)
	uint8_t line_buffer_size = 0;
	bool is_first=true;
	int16_t line_width;
	int16_t break_spot = -1;
	int16_t break_width;
	Color text_color = 0b1111111111111111;
	Color bg_color = 0b0000000000000000;
	
	//Start a message
	//void start(Color text, Color bg){
	//	is_first = true;
	//	text_color = text;
	//	bg_color = bg;
	//}
	
	//End a message
	
	void print(const char * message);
	
	void print(char chr){
		//Serial.print(chr);
		int16_t pre_width = line_width;
		if(chr=='\n'){
			break_spot = line_buffer_size;
			break_width = line_width;
			goto newline;
		}else if(chr>=' ' && chr<='~'){
			line_width += Font::char_width[chr-FONT_START];
			line_buffer[line_buffer_size++] = chr;
		}else if(chr=='\t'){ //for now tabs are just converted to 4 spaces (since the only time they're used is for indentation). I would make them into really wide blank chars but I don't feel like extending the charset;
			print("    ");
			return;
		}
		if(chr==' '){
			break_spot = line_buffer_size;
			break_width = line_width;
		}
		if(line_width>MESSAGE_AREA_WIDTH+1){
			newline:
			//if no spot to wrap was found:
			if(break_spot<0){
				break_spot = line_buffer_size-1;
				break_width = pre_width;
			}
			//output beginning of line buffer
			push_line(line_buffer,break_spot,text_color,bg_color,is_first);
			//move rest of line to start of line buffer
			line_buffer_size = line_buffer_size-break_spot;
			memcpy(line_buffer,line_buffer+break_spot,line_buffer_size);
			//reset
			break_spot = -1;
			line_width = line_width-break_width;
			break_width = 0;
			is_first = false;
		}
	}
	
	//display string
	void print(const char * message){
		while(*message)
			print(*message++);
	}
	
	void finish(){
		if(line_buffer_size)
			print('\n');
		is_first = true;
	}
	
	void println(const char * message){
		print(message);
		finish();
	}
	
	//void color(Color text_color, Color bg_color):text_color(text_color),bg_color(bg_color){}
}
//todo: eventually just put all of this into namespace Displ
void draw_text(ScreenPos x, ScreenPos y, const char * message, Color color){
	char chr;
	while(chr = *message){
		uint32_t data = Font::data[chr-FONT_START];
		uint8_t width = Font::glyph_width[chr-FONT_START];
		for(uint8_t i=0;i<32;i++)
			if(data & 0x800000>>i)
				tft.drawPixel(x+i%width,y+i/width,color);
		x+=Font::char_width[chr-FONT_START];
		message++;
	}
}

//visual Y position (relative to TFA) -> Y location in screen memory
ScreenPos vtom(ScreenPos y){
	y += tft._scroll;
	if(y>=NFA)
		y -= NFA;
	return y+TFA;
}

//Draw a rectangle in the scrolling area, using visual coords
//Y is relative to the top of the scrolling area
void rect(ScreenPos x, ScreenPos y, ScreenPos w, ScreenPos h, Color color){
	if(h<=0)
		return;
	y+=tft._scroll;
	if(y>=NFA)
		y-=NFA;
	if(y+h > NFA){
		ScreenPos top_part_height = NFA-y;
		tft.fillRect(x, TFA+y, w,   top_part_height, color);
		tft.fillRect(x, TFA  , w, h-top_part_height, color);
	}else{
		tft.fillRect(x, TFA+y, w, h                , color);
	}
}

VirtualY scrollbar_drawn_total_height = 0;
VirtualY scrollbar_drawn_scroll_pos = 0;

void draw_scrollbar(VirtualY total_height, VirtualY scroll_pos){
	if(scrollbar_drawn_total_height==total_height && scrollbar_drawn_scroll_pos==scroll_pos)
		return;
	
	scrollbar_drawn_total_height = total_height;
	scrollbar_drawn_scroll_pos = scroll_pos;
	
	ScreenPos size = ceil_div(NFA * NFA, total_height);
	if(size<1)
		size=1;
	else if(size>NFA)
		size=NFA;
	ScreenPos pos = scroll_pos * NFA / total_height;
	//todo: optimize drawing! (even more!)
	rect(SCW-1, NFA-pos-size, 1, size        , rgb(128,128,128)); //scroll bar
	rect(SCW-1, NFA-pos     , 1, NFA-size    , rgb(32,32,32)); //blank area above/below the scrollbar
}

//todo: function to display text in a non-scrolling area
//void disp(ScreenPos x, ScreenPos y, char chr){
//	uint8_t i;
//}

void draw_message_bg(ScreenPos start,ScreenPos end,Color color){
	rect(0,start,MESSAGE_AREA_WIDTH,end+1-start,color);
}

//y must be signed!
void render_row(ScreenPos y, LineIndex row, int8_t row_start, int8_t row_end, Color text_color, Color bg_color){
	if(line_type[row]==2){
		if(row_start==0)
			draw_message_bg(y,y,0b0000000000000000);
		else
			row_start--;
		y++;
		row_end--;
	}
	draw_message_bg(y+row_start,y+row_end-1,bg_color);
	ScreenPos x = 0;
	char * j = lines[row];
	while(*j){
		ScreenPos width = Font::glyph_width[*j-FONT_START];
		uint32_t data = Font::data[*j-FONT_START];
		for(uint8_t i = row_start*width;i<row_end*width;i++)
			if(data & 0x800000>>i)
				tft.drawPixel(x+i%width,vtom(y+i/width),text_color);
				//this technically would throw a divide by 0 error except it checks the data first.
		x += Font::char_width[*j-FONT_START];
		j++;
	}
}

//void draw_text(ScreenPos x, ScreenPos y, char * text, Color text_color, Color bg_color){
	
	
//}

int wrap(int a,int b){
	while(a<0) a += b;
	while(a>=b) a -= b;
	return a;
}

bool row_at(VirtualY py, LineIndex & row, VirtualY & row_top){
	if(py<0)
		return false;
	
	row = next_line;
	row_top = 0;
	
	LineIndex start = row;
	uint8_t row_height;
	
	while(1){
		if(!row)
			row = LINES;
		row--;
		if(row==start)
			return false;
		switch(line_type[row]){
			case 0:
				return false;
			case 1:
				row_height = 6;
			break;case 2:
				row_height = 7;
		}
		row_top += row_height;
		if(row_top>py)
			return true;
	}
}

//I don't know what these input values really represent. Figure it out idiot
void redraw(VirtualY bottom, VirtualY top, VirtualY dest, bool dir){
	VirtualY y=bottom;
	LineIndex row;
	VirtualY row_top;
	while(y<=top){
		if(!row_at(y,row,row_top)){
			y++;
			continue;
		}
		render_row(dest-row_top, row, max(row_top-top,0), row_top-y, line_color[row], line_bg_color[row]);
		y = row_top;
	}
	//I think this can be simplified:
	if(dir)
		draw_message_bg(0         , top-row_top-1, 0b0000000000000000); //             0, new_scroll+NFA-rowtop-1
	else
		draw_message_bg(NFA+bottom, NFA-1        , 0b0000000000000000); //NFA+new_scroll,            NFA       -1 //there are problems with end < start here...
}

//todo: fix slowdown when scrolling up past top
void change_scroll(VirtualY new_scroll){
	int change = (int)new_scroll-drawn_scroll;
	
	tft.scroll(wrap(tft._scroll - change,NFA));
	
	draw_scrollbar(total_height,new_scroll);
	
	//entire screen needs to be redrawn
	if(change>=NFA || -change>=NFA)
		redraw(new_scroll      , new_scroll+NFA, NFA+new_scroll, true);
	//scrolled
	else if(change<0)
		redraw(new_scroll      , drawn_scroll  , NFA+new_scroll, false);
	//scrolled other direction
	else if(change>0)
		redraw(drawn_scroll+NFA, new_scroll+NFA, NFA+new_scroll, true);
	
	drawn_scroll = new_scroll;
}

//todo: rooms, \t support, 
