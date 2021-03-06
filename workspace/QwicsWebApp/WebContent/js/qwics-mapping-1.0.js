/*
QWICS JavaScript Library for Map UI

Copyright (c) 2018 Philipp Brune    Email: Philipp.Brune@hs-neu-ulm.de  

This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the Free
Software Foundation; either version 3 of the License, or (at your option)
any later version.

This library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
for more details.

You should have received a copy of the GNU Lesser General Public License along
with this library; if not, If not, see <http://www.gnu.org/licenses/>. 

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

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
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

var mapTagetId;
var endpoint;

function jsonCallback(json) {
	showMap(json);
}


function mapHandler(event) {
    showMap(JSON.parse(event.data));    
}


function connect(url,program,defines) {
    endpoint = new WebSocket(url);
    endpoint.onmessage = mapHandler;
    endpoint.onopen = function (event) {
    		defines(endpoint);
        endpoint.send("GETMAP "+program); 
    }; 
}


var fields;


function trimValue(v) {
	if (typeof v == 'string') {
		return v.trim();
	}
	return v;
}


function showMap(map) {
    var i;
    var mapFull = map;
    map = map.JSON;
    fields = map.fields;
    
    $(mapTargetId).empty();
    var form = '<form>';
    for (i = 0; i < fields.length; i++) {
    		var group = '<div class="form-group">';
    		if (fields[i].name=="") {
    			group = group + '<label>'+fields[i].value+'</label>'    			
        		if ((i < (fields.length-1)) && (fields[i+1].name != "")) {
        			// Next field is data field
        			var ro = " readonly ";
        			if ((fields[i+1].attr & 4) > 0) {
        				ro = "";
        			} 
        			group = group +
        			'<input type="text" class="form-control" name="'+fields[i+1].name+'" id="'+
        			fields[i+1].name+'" size="'+fields[i+1].length+'" value="'+trimValue(mapFull[fields[i+1].name])+'"'+ro+'/>';
        			i++;
        		}
    		} else {
    			var ro = " readonly ";
    			if ((fields[i].attr & 4) > 0) {
    				ro = "";
    			} 
    			group = group +
    			'<input type="text" class="form-control" name="'+fields[i].name+'" id="'+
    			fields[i].name+'" size="'+fields[i].length+'" value="'+trimValue(mapFull[fields[i].name])+'"'+ro+'/>';    			
    		}
    		group = group +'</div>';
    		form = form + group;    		
    		/*    	
        if ((fields[i].attr & 4) > 0) {
            // Input field
            $(mapTargetId).append('<div style="position:absolute;top:'+((fields[i].y-1)*25)+'px;left:'+((fields[i].x-1)*15)+'px;height:20px;width:'+(fields[i].length*15)+'px;"><input type="text" class="mapinput" name="'+fields[i].name+'" id="'+fields[i].name+'" style="width:100%;" value="'+fields[i].value+'"/></div>');        
        } else {
            $(mapTargetId).append('<div style="position:absolute;top:'+((fields[i].y-1)*25)+'px;left:'+((fields[i].x-1)*15)+'px;height:20px;width:'+(fields[i].length*15)+'px;">'+fields[i].value+'</div>');        
        }
*/
    }
    form = form + '</form>';
    $(mapTargetId).append(form);
    
    $(mapTargetId).append('<button id="mapEnter" type="button" class="btn btn-primary">Ok</button>');
	$('#mapEnter').click(function() {
		endpoint.send("SENDDATA");
		endpoint.send("EIBAID");
		endpoint.send("\"");
	    for (i = 0; i < fields.length; i++) {
	        if (fields[i].name != "") {
	        		endpoint.send(fields[i].name);
        			endpoint.send($('#'+fields[i].name).val());
	        }
	    }
		endpoint.send("ENDDATA");
        endpoint.send("GETMAP"); 
	});

	$(mapTargetId).append('&nbsp;&nbsp;<button id="mapF9" class="btn btn-primary">Save</button>');
	$('#mapF9').click(function() {
		endpoint.send("SENDDATA");
		endpoint.send("EIBAID");
		endpoint.send("9");
	    for (i = 0; i < fields.length; i++) {
	        if (fields[i].name != "") {
	        		endpoint.send(fields[i].name);
        			endpoint.send($('#'+fields[i].name).val());
	        }
	    }
		endpoint.send("ENDDATA");
        endpoint.send("GETMAP"); 
	});

	$(mapTargetId).append('&nbsp;&nbsp;<button id="mapF3" class="btn btn-primary">F3</button>');
	$('#mapF3').click(function() {
		endpoint.send("SENDDATA");
		endpoint.send("EIBAID");
		endpoint.send("3");
	    for (i = 0; i < fields.length; i++) {
	        if (fields[i].name != "") {
	        		endpoint.send(fields[i].name);
        			endpoint.send($('#'+fields[i].name).val());
	        }
	    }
		endpoint.send("ENDDATA");
        endpoint.send("GETMAP"); 
	});

	$(mapTargetId).append('&nbsp;&nbsp;<button id="mapF12" class="btn btn-primary">F12</button>');
	$('#mapF12').click(function() {
		endpoint.send("SENDDATA");
		endpoint.send("EIBAID");
		endpoint.send("@");
	    for (i = 0; i < fields.length; i++) {
	        if (fields[i].name != "") {
	        		endpoint.send(fields[i].name);
        			endpoint.send($('#'+fields[i].name).val());
	        }
	    }
		endpoint.send("ENDDATA");
        endpoint.send("GETMAP"); 
	});
}


function loadMap(mapUrl,program,id,defines) {
	mapTargetId = id;
    connect(mapUrl,program,defines);
}
