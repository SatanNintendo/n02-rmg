#pragma once

#ifndef min
#define min(a,b) ((a<b)? a:b)
#endif

#include <memory>
#include <stdlib.h>
#include <string.h>

class k_framecache {
public:
	char * buffer;
	int pos;
	int size;
	k_framecache(){
		pos = 0;
		buffer = 0;
		size = 0;
	}
	k_framecache(char * buf, int len){
		pos = size = 0;
		buffer = 0;
		if (len > 0) {
			buffer = (char*)malloc(len);
			if (buffer != 0) {
				memcpy(buffer, buf, len);
				pos = size = len;
			}
		}
	}
	~k_framecache(){
		if (buffer != 0) {
			free(buffer);
			buffer=0;
			pos=0;
			size=0;
		}
	}
	void put_data(void * data, int datalen) {
		if(datalen > 0) {			
			ensure_sized(pos+datalen);
			if (buffer == 0 || size < pos + datalen)
				return; // allocation failed, drop the data instead of crashing
			memcpy(buffer + pos, data, datalen);
			pos += datalen;
		}
	}
	int peek_data(char * datab, int len){
		int x;
		if ((x = min(len, pos)) > 0) {
			memcpy(datab, buffer, x);
			return x;
		}
		return 0;
	}
	void ensure_sized(int datalen) {
		if (buffer == 0){
			size = datalen * 6;
			buffer = (char*)malloc(size);
			if (buffer == 0)
				size = 0;
		} else {
			if (datalen > size) {
				char * grown = (char*)realloc(buffer, datalen * 2);
				if (grown != 0) {
					buffer = grown;
					size = datalen * 2;
				}
			}
		}
	}
	int get_data(char * datab, int len){
		int x;
		if ((x = min(len, pos)) > 0) {
			memcpy(datab, buffer, x);
			pos -= x;
			memmove(buffer, buffer+x, pos); // regions overlap
			return x;
		}
		return 0;
	}
	void reset(){
		pos = 0;
	}
	void to_string(char * pre){
		//char xxx[200];
		//OutputHex(xxx, buffer, pos, 0, 0);
		//kprintf("k_frame(%x)::%s: %s, si=%i, le=%i", this, pre, xxx, size, pos);
	}
};

