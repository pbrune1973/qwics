/*
Qwics JDBC Client for Java

Copyright (c) 2018 Philipp Brune    Email: Philipp.Brune@qwics.org

This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the Free
Software Foundation; either version 3 of the License, or (at your option)
any later version.

This library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
for more details.

You should have received a copy of the GNU Lesser General Public License along
with this library; if not, see <http://www.gnu.org/licenses/>. 

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:
Redistributions of source code must retain the above copyright notice, this list
of conditions and the following disclaimer.

Redistributions in binary form must reproduce the above copyright notice, this
list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

Neither the name of the driver nor the names of its contributors may not be
used to endorse or promote products derived from this software without specific
prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS  AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/

package org.qwics.jdbc.msg;

import java.util.HashMap;

public class MsgHeaderConverter {
	// Singleton Instance
	private static MsgHeaderConverter converter = null;
	
	
	private MsgHeaderConverter() {
	}
	

	public static MsgHeaderConverter getConverter() {
		if (converter == null) {
			converter = new MsgHeaderConverter();
		}
		return converter;
	}
	
	
	public void intToRaw(int x, char rawData[], int pos) {
		rawData[pos] = (char)(x & 0x000000FF);
		rawData[pos+1] = (char)((x & 0x0000FF00) >> 8);
		rawData[pos+2] = (char)((x & 0x00FF0000) >> 16);
		rawData[pos+3] = (char)((x & 0xFF000000) >> 24);
	}
	
	
	public int rawToInt(char rawData[], int pos) {
		int x = 0;
		x = x | rawData[pos+3];
		x = x << 8;
		x = x | rawData[pos+2];
		x = x << 8;
		x = x | rawData[pos+1];
		x = x << 8;
		x = x | rawData[pos];
		return x;
	}
	
	
	public void stringToRaw(String str, char rawData[], int pos, int len) {
		int l = 0;
		if (str != null) {
			l = str.length();
		}
		if (l > len) l = len;
		for (int i = 0; i < l; i++) {
			rawData[pos+i] = str.charAt(i);
		}
		// Padding with spaces
		if (l < len) {
			for (int i = l; i < len; i++) {
				rawData[pos+i] = ' ';
			}			
		}
	}
	
	
	public String rawToString(char rawData[], int pos, int len) {
		String str = "";
		int i;
		for (i = len-1; i >= 0; i--) {
			if (rawData[pos+i] != ' ') {
				break;
			}
		}
		len = i+1;
		for (i = 0; i < len; i++) {
			str = str + rawData[pos+i];
		}
		return str;
	}
	
	
	public HashMap<String,Object> toMap(char rawData[]) {
		HashMap<String,Object> map = new HashMap<String,Object>();
		map.put("MSGTYPE",rawToInt(rawData,12));
		map.put("EXPIRY",rawToInt(rawData,16));
		map.put("PRIORITY",rawToInt(rawData,40));
		map.put("PERSISTENCE",rawToInt(rawData,44));
		map.put("MSGID",rawToString(rawData,48,24));
		map.put("CORRELID",rawToString(rawData,72,24));
		map.put("REPLYTOQ",rawToString(rawData,100,48));
		map.put("PUTDATE",rawToString(rawData,304,8));
		map.put("PUTTIME",rawToString(rawData,312,8));
		map.put("MSGSEQNUMBER",rawToInt(rawData,348));
		return map;
	}
	
	
	public char[] toRaw(HashMap<String,Object> map, char rawData[]) {
		intToRaw((int)map.get("MSGTYPE"),rawData,12);
		intToRaw((int)map.get("EXPIRY"),rawData,16);
		intToRaw((int)map.get("PRIORITY"),rawData,40);
		intToRaw((int)map.get("PERSISTENCE"),rawData,44);
		stringToRaw((String)map.get("MSGID"),rawData,48,24);
		stringToRaw((String)map.get("CORRELID"),rawData,72,24);
		stringToRaw((String)map.get("REPLYTOQ"),rawData,100,48);
		stringToRaw((String)map.get("PUTDATE"),rawData,304,8);
		stringToRaw((String)map.get("PUTTIME"),rawData,312,8);
		intToRaw((int)map.get("MSGSEQNUMBER"),rawData,348);
		return rawData;		
	}
}
