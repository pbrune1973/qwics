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

import java.io.PrintWriter;
import java.sql.Connection;
import java.sql.SQLException;
import java.sql.SQLFeatureNotSupportedException;
import java.util.logging.Logger;

import javax.sql.ConnectionPoolDataSource;
import javax.sql.DataSource;
import javax.sql.PooledConnection;

public class QwicsDataSource implements DataSource, ConnectionPoolDataSource {
	private int loginTimeout;
	private String url = "jdbc:qwics:localhost:8000";
	protected String host;
	protected int port;

	public QwicsDataSource(String url) throws SQLException {
		setUrl(url);
	}

	public QwicsDataSource() throws SQLException {
		setUrl(url);
	}

	public void setUrl(String url) throws SQLException {
		this.url = url;
		String parts[] = url.split(":");
		if (parts.length != 4) {
			throw new SQLException("Invalid jdbc connect URL");
		}
		if (!parts[0].equals("jdbc")) {
			throw new SQLException("Invalid jdbc connect URL");
		}
		if (!parts[1].equals("qwics")) {
			throw new SQLException("Invalid jdbc connect URL");
		}
		host = parts[2];
		port = 0;
		try {
			port = Integer.parseInt(parts[3]);
		} catch (Exception e) {
			throw new SQLException("Invalid jdbc connect URL");
		}	
	}
	
	@Override
	public PrintWriter getLogWriter() throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public void setLogWriter(PrintWriter out) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setLoginTimeout(int seconds) throws SQLException {
		loginTimeout = seconds;
	}

	@Override
	public int getLoginTimeout() throws SQLException {
		return loginTimeout;
	}

	@Override
	public Logger getParentLogger() throws SQLFeatureNotSupportedException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public <T> T unwrap(Class<T> iface) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public boolean isWrapperFor(Class<?> iface) throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public Connection getConnection() throws SQLException {
		QwicsConnection con = new QwicsConnection(host, port);
		try {
			con.open();
		} catch (Exception e) {
			throw new SQLException(e);
		}
		return con;
	}

	@Override
	public Connection getConnection(String username, String password)
			throws SQLException {
		return this.getConnection();
	}

	@Override
	public PooledConnection getPooledConnection() throws SQLException {
		QwicsPooledConnection con = new QwicsPooledConnection(host, port);
		try {
			con.open();
		} catch (Exception e) {
			throw new SQLException(e);
		}
		return con;
	}

	@Override
	public PooledConnection getPooledConnection(String user, String password)
			throws SQLException {
		return this.getPooledConnection();
	}

}
