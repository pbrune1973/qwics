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

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map.Entry;
import java.util.Set;

public class QueueHandler {
	private QueueManager queueManager = null;
	private ArrayList<QueueWrapper> objs = new ArrayList<QueueWrapper>();
	private HashMap<String,Integer> objIndices = new HashMap<String, Integer>();
	
		
	public QueueHandler(QueueManager queueManager) {
		this.queueManager = queueManager;
	}


	public int openQueue(String name, int type, int opts) throws Exception {
		synchronized (this) {
			if (objIndices.containsKey(name)) {
				return objIndices.get(name);
			}
			QueueWrapper q = queueManager.getQueue(name,type,opts);
			for (int i = 0; i < objs.size(); i++) {
				if (objs.get(i) == null) {
					objs.set(i, q);
					objIndices.put(name, i);
					return i;
				}
			}
			objs.add(q);
			int idx = objs.size()-1;
			objIndices.put(name, idx);
			return idx;
		}
	}

	
	public void closeQueue(int obj, int opts) throws Exception {
		synchronized (this) {
			String name = "";
			Set<Entry<String, Integer>> elems = objIndices.entrySet();
			for (Entry<String, Integer> el : elems) {
				if (el.getValue() == obj) {
					name = el.getKey();
					break;
				}
			}
			if (name == null) {
				throw new Exception("Invalid obj index!");
			}
			QueueWrapper q = objs.get(obj);
			q.close();
			objs.set(obj, null);
			objIndices.remove(name,obj);
		}		
	}
	
	
	public QueueWrapper getQueue(int obj) throws Exception {
		synchronized (this) {
			return objs.get(obj);
		}
	}
	
}
