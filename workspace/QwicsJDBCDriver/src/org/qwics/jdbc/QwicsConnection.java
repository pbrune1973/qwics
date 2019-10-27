/*
Qwics JDBC Client for Java

Copyright (c) 2018,2019 Philipp Brune    Email: Philipp.Brune@hqwics.org

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

package org.qwics.jdbc;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.net.Socket;
import java.sql.Array;
import java.sql.Blob;
import java.sql.CallableStatement;
import java.sql.Clob;
import java.sql.Connection;
import java.sql.DatabaseMetaData;
import java.sql.NClob;
import java.sql.PreparedStatement;
import java.sql.SQLClientInfoException;
import java.sql.SQLException;
import java.sql.SQLWarning;
import java.sql.SQLXML;
import java.sql.Savepoint;
import java.sql.Statement;
import java.sql.Struct;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.Properties;
import java.util.Random;
import java.util.concurrent.Executor;

public class QwicsConnection implements Connection {
	private String host;
	private int port;
	private Socket socket;
	private BufferedWriter socketWriter;
	private BufferedReader socketReader;
	private boolean closed = true;
	private DatabaseMetaData metaData;
	// Arrays for connection-specific RECEIVE values
	private ArrayList<String> mapNames = new ArrayList<String>();
	private ArrayList<String> mapValues = new ArrayList<String>();
	private HashMap<String, Integer> nameIndices = new HashMap<String, Integer>();
	private String conId = "";

	private void createConId() {
		Random rand = new Random();
		conId = "";
		for (int i = 0; i < 10; i++) {
			char c = (char)(65+rand.nextInt(25));
			conId = conId + c;
		}
	}

	public QwicsConnection(String host, int port) {
		this.host = host;
		this.port = port;
		metaData = new QwicsDatabaseMetaData(host,port,this);
		createConId();
	}

	public void open() throws Exception {
		try {
			socket = new Socket(host, port);
			socketWriter = new BufferedWriter(new OutputStreamWriter(
					socket.getOutputStream()));
			socketReader = new BufferedReader(new InputStreamReader(
					socket.getInputStream()));
			sendSql("BEGIN");
			closed = false;
		} catch (Exception e) {
			e.printStackTrace();
			throw e;
		}
	}

	public int sendSql(String sql) throws SQLException {
		try {
			socketWriter.write("sql " + sql);
			socketWriter.newLine();
			socketWriter.flush();
		} catch (Exception e) {
			e.printStackTrace();
			throw new SQLException(e);
		}
		return 0;
	}

	public int sendCmd(String cmd) throws SQLException {
		try {
			socketWriter.write(cmd);
			socketWriter.newLine();
			socketWriter.flush();
		} catch (Exception e) {
			e.printStackTrace();
			throw new SQLException(e);
		}
		return 0;
	}

	public int sendBuf(char buf[]) throws Exception {
		try {
			socketWriter.write(buf);
			socketWriter.flush();
		} catch (Exception e) {
			e.printStackTrace();
			throw e;
		}
		return 0;
	}

	public int readBuf(char buf[]) throws Exception {
		try {
			int off = 0;
			int l = buf.length;
			int n = -1;
			do {
				n = socketReader.read(buf,off,l);
				off = off + n;
				l = l - n;
			} while ((off < buf.length) && (n >= 0)); 	
		} catch (Exception e) {
			e.printStackTrace();
			throw e;
		}
		return 0;
	}

	public String readResult() throws Exception {
		String line = socketReader.readLine(); 
		if (line == null) {
			throw new Exception("Connection to tpmserver lost");
		}
		return line;
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
	public Statement createStatement() throws SQLException {
		return new QwicsStatement(this);
	}

	@Override
	public PreparedStatement prepareStatement(String sql) throws SQLException {
		return new QwicsStatement(this, sql);
	}

	@Override
	public CallableStatement prepareCall(String sql) throws SQLException {
		return new QwicsCallableStatement(this, sql);
	}

	@Override
	public String nativeSQL(String sql) throws SQLException {
		return sql;
	}

	@Override
	public void setAutoCommit(boolean autoCommit) throws SQLException {
	}

	@Override
	public boolean getAutoCommit() throws SQLException {
		return false;
	}

	@Override
	public void commit() throws SQLException {
		this.sendSql("COMMIT");
		this.sendSql("BEGIN");
	}

	@Override
	public void rollback() throws SQLException {
		this.sendSql("ROLLBACK");
		this.sendSql("BEGIN");
	}

	@Override
	public synchronized void close() throws SQLException {
		if (!closed) {
			sendCmd("quit");				
		}		
		try {
			closed = true;
			socketWriter.close();
			socketReader.close();
			socket.close();				
		} catch (Exception e) {
			e.printStackTrace();
			throw new SQLException(e);
		}
	}

	@Override
	public synchronized boolean isClosed() throws SQLException {
		return closed;
	}

	@Override
	public DatabaseMetaData getMetaData() throws SQLException {
		return metaData;
	}

	@Override
	public void setReadOnly(boolean readOnly) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public boolean isReadOnly() throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public void setCatalog(String catalog) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public String getCatalog() throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public void setTransactionIsolation(int level) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public int getTransactionIsolation() throws SQLException {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public SQLWarning getWarnings() throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public void clearWarnings() throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public Statement createStatement(int resultSetType, int resultSetConcurrency)
			throws SQLException {
		return new QwicsStatement(this);
	}

	@Override
	public PreparedStatement prepareStatement(String sql, int resultSetType,
			int resultSetConcurrency) throws SQLException {
		return new QwicsStatement(this,sql);
	}

	@Override
	public CallableStatement prepareCall(String sql, int resultSetType,
			int resultSetConcurrency) throws SQLException {
		return new QwicsCallableStatement(this,sql);
	}

	@Override
	public Map<String, Class<?>> getTypeMap() throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public void setTypeMap(Map<String, Class<?>> map) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setHoldability(int holdability) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public int getHoldability() throws SQLException {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public Savepoint setSavepoint() throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Savepoint setSavepoint(String name) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public void rollback(Savepoint savepoint) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void releaseSavepoint(Savepoint savepoint) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public Statement createStatement(int resultSetType,
			int resultSetConcurrency, int resultSetHoldability)
			throws SQLException {
		return new QwicsStatement(this);
	}

	@Override
	public PreparedStatement prepareStatement(String sql, int resultSetType,
			int resultSetConcurrency, int resultSetHoldability)
			throws SQLException {
		return new QwicsStatement(this,sql);
	}

	@Override
	public CallableStatement prepareCall(String sql, int resultSetType,
			int resultSetConcurrency, int resultSetHoldability)
			throws SQLException {
		return new QwicsCallableStatement(this, sql);
	}

	@Override
	public PreparedStatement prepareStatement(String sql, int autoGeneratedKeys)
			throws SQLException {
		return new QwicsStatement(this,sql);
	}

	@Override
	public PreparedStatement prepareStatement(String sql, int[] columnIndexes)
			throws SQLException {
		return new QwicsStatement(this,sql);
	}

	@Override
	public PreparedStatement prepareStatement(String sql, String[] columnNames)
			throws SQLException {
		return new QwicsStatement(this,sql);
	}

	@Override
	public Clob createClob() throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Blob createBlob() throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public NClob createNClob() throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public SQLXML createSQLXML() throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public boolean isValid(int timeout) throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public void setClientInfo(String name, String value)
			throws SQLClientInfoException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setClientInfo(Properties properties)
			throws SQLClientInfoException {
		// TODO Auto-generated method stub

	}

	@Override
	public String getClientInfo(String name) throws SQLException {
		if ("conId".equals(name)) {
			return conId;
		}
		return null;
	}

	@Override
	public Properties getClientInfo() throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Array createArrayOf(String typeName, Object[] elements)
			throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Struct createStruct(String typeName, Object[] attributes)
			throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public void setSchema(String schema) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public String getSchema() throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public void abort(Executor executor) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setNetworkTimeout(Executor executor, int milliseconds)
			throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public int getNetworkTimeout() throws SQLException {
		// TODO Auto-generated method stub
		return 0;
	}

	public ArrayList<String> getMapNames() {
		return mapNames;
	}

	public ArrayList<String> getMapValues() {
		return mapValues;
	}

	public HashMap<String, Integer> getNameIndices() {
		return nameIndices;
	}
	
}
