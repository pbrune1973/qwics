/*
Qwics JDBC Client for Java

Copyright (c) 2019 Philipp Brune    Email: Philipp.Brune@hs-neu-ulm.de

This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the Free
Software Foundation; either version 3 of the License, or (at your option)
any later version.

This library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
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

package org.qwics.jdbc.scheduler;

public class TaScheduled implements Comparable {
	private String transId;
	private String reqId;
	private char data[];
	private long startAtMillis = 0;

	public TaScheduled(String transId, String reqId, char[] data, String time, boolean isInterval) throws NumberFormatException {
		this.transId = transId;
		this.reqId = reqId;
		this.data = data;

		try {
			int hour = Integer.parseInt(time.substring(0, 2));
			int min = Integer.parseInt(time.substring(2, 4));
			int sec = Integer.parseInt(time.substring(4));
			if ((min < 0) || (min > 59)) {
				throw new NumberFormatException("min");
			}
			if ((sec < 0) || (sec > 59)) {
				throw new NumberFormatException("sec");
			}
			
			long millis = (long) ((hour * 60 + min) * 60 + sec) * 1000;
			if (isInterval) {
				millis = millis + System.currentTimeMillis();
			}
			startAtMillis = millis;
		} catch (Exception e) {
			e.printStackTrace();
			if (e instanceof NumberFormatException) {
				throw e;
			}
		}
	}

	public String getTransId() {
		return transId;
	}

	public void setTransId(String transId) {
		this.transId = transId;
	}

	public String getReqId() {
		return reqId;
	}

	public void setReqId(String reqId) {
		this.reqId = reqId;
	}

	public char[] getData() {
		return data;
	}

	public void setData(char[] data) {
		this.data = data;
	}

	public long getStartAtMillis() {
		return startAtMillis;
	}

	public void setStartAtMillis(long startAtMillis) {
		this.startAtMillis = startAtMillis;
	}

	@Override
	public int compareTo(Object o) {
		TaScheduled ta = (TaScheduled)o;
		if (this.startAtMillis < ta.getStartAtMillis()) {
			return -1;
		}
		if (this.startAtMillis > ta.getStartAtMillis()) {
			return 1;
		}
		return 0;
	}

}
