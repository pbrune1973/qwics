/*
Qwics JDBC Client for Java

Copyright (c) 2018,2019 Philipp Brune    Email: Philipp.Brune@hs-neu-ulm.de

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

import java.sql.Connection;
import java.sql.SQLException;
import java.util.ArrayList;
import java.util.List;

import javax.sql.ConnectionEvent;
import javax.sql.ConnectionEventListener;
import javax.sql.PooledConnection;
import javax.sql.StatementEventListener;

public class QwicsPooledConnection extends QwicsConnection implements PooledConnection {
    List<ConnectionEventListener> connectionEventListeners;
    List<StatementEventListener> statementEventListeners;
    private boolean inUse = false;
    
    public QwicsPooledConnection(String host, int port) {
		super(host, port);
        statementEventListeners = new ArrayList<StatementEventListener>();
        connectionEventListeners = new ArrayList<ConnectionEventListener>();
	}
   
	@Override
	public Connection getConnection() throws SQLException {
		this.inUse = true;
		return this;
	}

	@Override
	public void addConnectionEventListener(ConnectionEventListener listener) {
		connectionEventListeners.add(listener);
	}

	@Override
	public void removeConnectionEventListener(ConnectionEventListener listener) {
		connectionEventListeners.remove(listener);
	}

	@Override
	public void addStatementEventListener(StatementEventListener listener) {
		statementEventListeners.add(listener);
	}

	@Override
	public void removeStatementEventListener(StatementEventListener listener) {
		statementEventListeners.remove(listener);
	}
	
	public void connectionClosed() {
		ConnectionEvent event = new ConnectionEvent(this);
		for (ConnectionEventListener l : connectionEventListeners) {
			l.connectionClosed(event);
		}
	}

	@Override
	public void close() throws SQLException {
		if (inUse) {
			connectionClosed();
			inUse = false;
		} else {
			super.close();
		}
	}

}
