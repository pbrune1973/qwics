/*
Qwics JDBC Client for Java

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

package org.qwics.jdbc;

import java.sql.ResultSet;
import java.sql.Statement;
import java.util.ArrayList;

import javax.transaction.xa.XAException;
import javax.transaction.xa.XAResource;
import javax.transaction.xa.Xid;

public class QwicsXAResource implements XAResource {
	private QwicsXAConnection conn;
	private int timeout = 0;
	
	public QwicsXAResource(QwicsXAConnection conn) {
		super();
		this.conn = conn;
	}
	
	public static String bytesToHex(byte bytes[]) {
			String result = "";
			for (int i = 0; i < bytes.length; i++) {
				byte b = bytes[i];
				int digit = (b & 0xF0) >> 4;
			    if (digit < 10) {
			    		result = result + digit;
			    } else {
			    		result = result + (char)(digit-10+65);
			    }
				digit = (b & 0x0F);
			    if (digit < 10) {
			    		result = result + digit;
			    } else {
			    		result = result + (char)(digit-10+65);
			    }
			}
			return result;
	}
	
    public static String xidToString(Xid xid) {
    		return "'"+bytesToHex(xid.getGlobalTransactionId())+"."+
    				   bytesToHex(xid.getBranchQualifier())+"."+
    				   xid.getFormatId()+"'";
    }
	
	@Override
	public void commit(Xid xid, boolean onePhase) throws XAException {
		try {
			// System.out.println("XA commit "+xid+" "+onePhase);
			if (onePhase) {
				conn.commit();
				return;
			}
			conn.sendSql("COMMIT PREPARED "+xidToString(xid));
			conn.sendSql("BEGIN");
		} catch (Exception e) {
			throw new XAException(e.toString());
		}
	}

	@Override
	public void end(Xid xid, int flags) throws XAException {
		// System.out.println("XA end "+xid+" "+flags);
		/*
        if (flags != TMSUCCESS && flags != TMSUSPEND && flags != TMFAIL) {
            throw new XAException(XAException.XAER_INVAL);
        }
		try {
			conn.sendSql("COMMIT PREPARED "+xidToString(xid));
			conn.sendSql("BEGIN");
		} catch (Exception e) {
			throw new XAException(e.toString());
		}
		*/
	}

	@Override
	public void forget(Xid xid) throws XAException {
		// TODO Auto-generated method stub
	}

	@Override
	public int getTransactionTimeout() throws XAException {
		return timeout;
	}

	@Override
	public boolean isSameRM(XAResource res) throws XAException {
		return res == this;
	}

	@Override
	public int prepare(Xid xid) throws XAException {
		try {
			conn.sendSql("PREPARE TRANSACTION "+xidToString(xid));
		} catch (Exception e) {
			throw new XAException(e.toString());
		}
		return XA_OK;
	}

	@Override
	public Xid[] recover(int flags) throws XAException {
		ArrayList<Xid> xids = new ArrayList<Xid>();

		if (((flags & TMSTARTRSCAN) == 0) && ((flags & TMENDRSCAN) == 0) && (flags != TMNOFLAGS)) {
            throw new XAException(XAException.XAER_INVAL);
        }

        if ((flags & TMSTARTRSCAN) == 0) {
            return (Xid[])xids.toArray();
        }
        
		try {
			Statement stmt = new QwicsStatement(conn);
			ResultSet r = stmt.executeQuery("SELECT * FROM pg_prepared_xacts");
			while (r.next()) {
				String gid = r.getString("prepared");
				xids.add(new QwicsXid(gid));
			}
			stmt.close();
		} catch (Exception e) {
			throw new XAException(e.toString());
		}
        return (Xid[])xids.toArray();
	}

	@Override
	public void rollback(Xid xid) throws XAException {
		try {
			conn.sendSql("ROLLBACK PREPARED "+xidToString(xid));
			conn.sendSql("BEGIN");
		} catch (Exception e) {
			throw new XAException(e.toString());
		}
	}

	@Override
	public boolean setTransactionTimeout(int timeout) throws XAException {
		this.timeout = timeout;
		return false;
	}

	@Override
	public void start(Xid xid, int flags) throws XAException {
		// System.out.println("XA start "+xid+" "+flags);
/*
		if (flags != TMJOIN && flags != TMRESUME && flags != TMNOFLAGS) {
            throw new XAException(XAException.XAER_INVAL);
        }
        if (flags == TMJOIN) {
            flags = TMRESUME;
        }
*/
	}

}
