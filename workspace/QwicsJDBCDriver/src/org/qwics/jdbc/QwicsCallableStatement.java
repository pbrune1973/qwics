/*
Qwics JDBC Client for Java

Copyright (c) 2018-2020 Philipp Brune    Email: Philipp.Brune@hs-neu-ulm.de  

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

package org.qwics.jdbc;

import java.io.InputStream;
import java.io.Reader;
import java.math.BigDecimal;
import java.net.URL;
import java.sql.Array;
import java.sql.Blob;
import java.sql.CallableStatement;
import java.sql.Clob;
import java.sql.Connection;
import java.sql.Date;
import java.sql.NClob;
import java.sql.ParameterMetaData;
import java.sql.Ref;
import java.sql.ResultSet;
import java.sql.ResultSetMetaData;
import java.sql.RowId;
import java.sql.SQLException;
import java.sql.SQLWarning;
import java.sql.SQLXML;
import java.sql.Time;
import java.sql.Timestamp;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Map;

import javax.sql.ConnectionEvent;
import javax.sql.ConnectionEventListener;

public class QwicsCallableStatement implements CallableStatement, ConnectionEventListener {
	private QwicsConnection conn;
	private String sql;
	private String preparedSql;
	private String sqlParts[] = new String[0];
	private ArrayList<Object> params = new ArrayList<Object>();
	private ResultSet resultSet = null;
	private int updateCount = 0;
	private boolean closed = false;
	private long eibCALen = 0;
	private char eibAID = '"';
	private char[] commArea = null;

	public QwicsCallableStatement(QwicsConnection conn) {
		this.conn = conn;
		this.sql = "";
		if (conn instanceof QwicsPooledConnection) {
			((QwicsPooledConnection) conn).addConnectionEventListener(this);
		}
	}

	public QwicsCallableStatement(QwicsConnection conn, String sql) {
		this.conn = conn;
		this.sql = sql;
		if (conn instanceof QwicsPooledConnection) {
			((QwicsPooledConnection) conn).addConnectionEventListener(this);
		}
		boolean verb = false;
		int count = 0;
		for (int i = 0; i < sql.length(); i++) {
			if (sql.charAt(i) == '\'') {
				verb = !verb;
			}
			if (!verb && (sql.charAt(i) == '?')) {
				count++;
			}
		}
		sqlParts = new String[count + 1];
		int j = 0, k = -1;
		verb = false;
		for (int i = 0; i < sql.length(); i++) {
			if (sql.charAt(i) == '\'') {
				verb = !verb;
			}
			if (!verb && (sql.charAt(i) == '?')) {
				sqlParts[j] = sql.substring(k + 1, i);
				k = i;
				j++;
			}
		}
		sqlParts[j] = sql.substring(k + 1);
		for (int i = 0; i < count; i++) {
			params.add(null);
		}
		prepareSql();
	}

	public void prepareSql() {
		StringBuffer buf = new StringBuffer();
		int n = sqlParts.length - 1;
		if (n > 0) {
			for (int i = 0; i < n; i++) {
				buf.append(sqlParts[i]);
				if (params.get(i) instanceof String) {
					buf.append("'");
					buf.append(params.get(i));
					buf.append("'");
				} else {
					buf.append(params.get(i));
				}
			}
			buf.append(sqlParts[n]);
			preparedSql = buf.toString();
		} else {
			preparedSql = sql;
		}
	}

	@Override
	public ResultSet executeQuery() throws SQLException {
		if (preparedSql.startsWith("PROGRAM ")) {
			conn.sendCmd(preparedSql);
			try {
				String resp = conn.readResult();
				if ("OK".equals(resp)) {
					String progId = preparedSql.substring(8).trim();
					resultSet = new QwicsMapResultSet(conn, eibCALen, eibAID, progId);
					return resultSet;
				}
			} catch (Exception e) {
				throw new SQLException(e);
			}
			throw new SQLException("PROGRAM LOAD ERROR");
		} else if (preparedSql.startsWith("CAPROG ")) {
			conn.sendCmd(preparedSql);
			try {
				String resp = conn.readResult();
				if ("COMMAREA".equals(resp)) {
					conn.sendBuf(commArea);
					resp = conn.readResult();
				}
				if ("OK".equals(resp)) {
					String progId = preparedSql.substring(7).trim();
					resultSet = new QwicsMapResultSet(conn, eibCALen, eibAID, progId);
					return resultSet;
				}
			} catch (Exception e) {
				throw new SQLException(e);
			}
			throw new SQLException("PROGRAM LOAD ERROR");
		} else {
			conn.sendSql(preparedSql);
			try {
				String resp = conn.readResult();
				if ("OK".equals(resp)) {
					resultSet = new QwicsResultSet(conn);
					return resultSet;
				}
			} catch (Exception e) {
				throw new SQLException(e);
			}
			throw new SQLException("SQL ERROR");
		}
	}

	@Override
	public int executeUpdate() throws SQLException {
		conn.sendSql(preparedSql);
		try {
			String resp = conn.readResult();
			if (resp.startsWith("OK:")) {
				updateCount = Integer.parseInt(resp.substring(3));
				return updateCount;
			}
		} catch (Exception e) {
			throw new SQLException(e);
		}
		throw new SQLException("SQL ERROR");
	}

	@Override
	public void setNull(int parameterIndex, int sqlType) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setBoolean(int parameterIndex, boolean x) throws SQLException {
		params.set(parameterIndex - 1, x);
		prepareSql();
	}

	@Override
	public void setByte(int parameterIndex, byte x) throws SQLException {
		params.set(parameterIndex - 1, x);
		prepareSql();
	}

	@Override
	public void setShort(int parameterIndex, short x) throws SQLException {
		params.set(parameterIndex - 1, x);
		prepareSql();
	}

	@Override
	public void setInt(int parameterIndex, int x) throws SQLException {
		params.set(parameterIndex - 1, x);
		prepareSql();
	}

	@Override
	public void setLong(int parameterIndex, long x) throws SQLException {
		params.set(parameterIndex - 1, x);
		prepareSql();
	}

	@Override
	public void setFloat(int parameterIndex, float x) throws SQLException {
		params.set(parameterIndex - 1, x);
		prepareSql();
	}

	@Override
	public void setDouble(int parameterIndex, double x) throws SQLException {
		params.set(parameterIndex - 1, x);
		prepareSql();
	}

	@Override
	public void setBigDecimal(int parameterIndex, BigDecimal x) throws SQLException {
		params.set(parameterIndex - 1, x);
		prepareSql();
	}

	@Override
	public void setString(int parameterIndex, String x) throws SQLException {
		params.set(parameterIndex - 1, x);
		prepareSql();
	}

	@Override
	public void setBytes(int parameterIndex, byte[] x) throws SQLException {
		params.set(parameterIndex - 1, x);
		prepareSql();
	}

	@Override
	public void setDate(int parameterIndex, Date x) throws SQLException {
		params.set(parameterIndex - 1, x);
		prepareSql();
	}

	@Override
	public void setTime(int parameterIndex, Time x) throws SQLException {
		params.set(parameterIndex - 1, x);
		prepareSql();
	}

	@Override
	public void setTimestamp(int parameterIndex, Timestamp x) throws SQLException {
		params.set(parameterIndex - 1, x);
		prepareSql();
	}

	@Override
	public void setAsciiStream(int parameterIndex, InputStream x, int length) throws SQLException {
		params.set(parameterIndex - 1, x);
		prepareSql();
	}

	@Override
	public void setUnicodeStream(int parameterIndex, InputStream x, int length) throws SQLException {
		params.set(parameterIndex - 1, x);
		prepareSql();
	}

	@Override
	public void setBinaryStream(int parameterIndex, InputStream x, int length) throws SQLException {
		params.set(parameterIndex - 1, x);
		prepareSql();
	}

	@Override
	public void clearParameters() throws SQLException {
		int n = sqlParts.length - 1;
		for (int i = 0; i < n; i++) {
			params.set(i, null);
		}
		prepareSql();
	}

	@Override
	public void setObject(int parameterIndex, Object x, int targetSqlType) throws SQLException {
		params.set(parameterIndex - 1, x);
		prepareSql();
	}

	@Override
	public void setObject(int parameterIndex, Object x) throws SQLException {
		params.set(parameterIndex - 1, x);
		prepareSql();
	}

	@Override
	public boolean execute() throws SQLException {
		conn.sendSql(preparedSql);
		try {
			String resp = conn.readResult();
			if (resp.startsWith("OK")) {
				return true;
			}
		} catch (Exception e) {
			throw new SQLException(e);
		}
		return false;
	}

	@Override
	public void addBatch() throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setCharacterStream(int parameterIndex, Reader reader, int length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setRef(int parameterIndex, Ref x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setBlob(int parameterIndex, Blob x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setClob(int parameterIndex, Clob x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setArray(int parameterIndex, Array x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public ResultSetMetaData getMetaData() throws SQLException {
		return resultSet.getMetaData();
	}

	@Override
	public void setDate(int parameterIndex, Date x, Calendar cal) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setTime(int parameterIndex, Time x, Calendar cal) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setTimestamp(int parameterIndex, Timestamp x, Calendar cal) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setNull(int parameterIndex, int sqlType, String typeName) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setURL(int parameterIndex, URL x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public ParameterMetaData getParameterMetaData() throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public void setRowId(int parameterIndex, RowId x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setNString(int parameterIndex, String value) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setNCharacterStream(int parameterIndex, Reader value, long length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setNClob(int parameterIndex, NClob value) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setClob(int parameterIndex, Reader reader, long length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setBlob(int parameterIndex, InputStream inputStream, long length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setNClob(int parameterIndex, Reader reader, long length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setSQLXML(int parameterIndex, SQLXML xmlObject) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setObject(int parameterIndex, Object x, int targetSqlType, int scaleOrLength) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setAsciiStream(int parameterIndex, InputStream x, long length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setBinaryStream(int parameterIndex, InputStream x, long length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setCharacterStream(int parameterIndex, Reader reader, long length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setAsciiStream(int parameterIndex, InputStream x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setBinaryStream(int parameterIndex, InputStream x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setCharacterStream(int parameterIndex, Reader reader) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setNCharacterStream(int parameterIndex, Reader value) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setClob(int parameterIndex, Reader reader) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setBlob(int parameterIndex, InputStream inputStream) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setNClob(int parameterIndex, Reader reader) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public ResultSet executeQuery(String sql) throws SQLException {
		this.sql = sql;
		this.preparedSql = sql;
		if (preparedSql.startsWith("PROGRAM ")) {
			conn.sendCmd(preparedSql);
			try {
				String resp = conn.readResult();
				if ("OK".equals(resp)) {
					String progId = preparedSql.substring(8).trim();
					resultSet = new QwicsMapResultSet(conn, eibCALen, eibAID, progId);
					return resultSet;
				}
			} catch (Exception e) {
				throw new SQLException(e);
			}
			throw new SQLException("PROGRAM LOAD ERROR");
		} else {
			conn.sendSql(preparedSql);
			try {
				String resp = conn.readResult();
				if ("OK".equals(resp)) {
					resultSet = new QwicsResultSet(conn);
					return resultSet;
				}
			} catch (Exception e) {
				throw new SQLException(e);
			}
			throw new SQLException("SQL ERROR");
		}
	}

	@Override
	public int executeUpdate(String sql) throws SQLException {
		this.sql = sql;
		this.preparedSql = sql;
		conn.sendSql(sql);
		try {
			String resp = conn.readResult();
			if (resp.startsWith("OK:")) {
				updateCount = Integer.parseInt(resp.substring(3));
				return updateCount;
			}
		} catch (Exception e) {
			throw new SQLException(e);
		}
		throw new SQLException("SQL ERROR");
	}

	@Override
	public void close() throws SQLException {
		resultSet = null;
		closed = true;
	}

	@Override
	public int getMaxFieldSize() throws SQLException {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public void setMaxFieldSize(int max) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public int getMaxRows() throws SQLException {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public void setMaxRows(int max) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setEscapeProcessing(boolean enable) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public int getQueryTimeout() throws SQLException {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public void setQueryTimeout(int seconds) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void cancel() throws SQLException {
		// TODO Auto-generated method stub

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
	public void setCursorName(String name) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public boolean execute(String sql) throws SQLException {
		this.sql = sql;
		this.preparedSql = sql;
		conn.sendSql(preparedSql);
		try {
			String resp = conn.readResult();
			if (resp.startsWith("OK")) {
				return true;
			}
		} catch (Exception e) {
			throw new SQLException(e);
		}
		return false;
	}

	@Override
	public ResultSet getResultSet() throws SQLException {
		return resultSet;
	}

	@Override
	public int getUpdateCount() throws SQLException {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public boolean getMoreResults() throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public void setFetchDirection(int direction) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public int getFetchDirection() throws SQLException {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public void setFetchSize(int rows) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public int getFetchSize() throws SQLException {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public int getResultSetConcurrency() throws SQLException {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public int getResultSetType() throws SQLException {
		return 0;
	}

	@Override
	public void addBatch(String sql) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void clearBatch() throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public int[] executeBatch() throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Connection getConnection() throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public boolean getMoreResults(int current) throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public ResultSet getGeneratedKeys() throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public int executeUpdate(String sql, int autoGeneratedKeys) throws SQLException {
		return executeUpdate(sql);
	}

	@Override
	public int executeUpdate(String sql, int[] columnIndexes) throws SQLException {
		return executeUpdate(sql);
	}

	@Override
	public int executeUpdate(String sql, String[] columnNames) throws SQLException {
		return executeUpdate(sql);
	}

	@Override
	public boolean execute(String sql, int autoGeneratedKeys) throws SQLException {
		return execute(sql);
	}

	@Override
	public boolean execute(String sql, int[] columnIndexes) throws SQLException {
		return execute(sql);
	}

	@Override
	public boolean execute(String sql, String[] columnNames) throws SQLException {
		return execute(sql);
	}

	@Override
	public int getResultSetHoldability() throws SQLException {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public boolean isClosed() throws SQLException {
		return closed;
	}

	@Override
	public void setPoolable(boolean poolable) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public boolean isPoolable() throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public void closeOnCompletion() throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public boolean isCloseOnCompletion() throws SQLException {
		// TODO Auto-generated method stub
		return false;
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
	public void setClob(String parameterName, Clob x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setAsciiStream(String parameterName, InputStream x, long length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setBinaryStream(String parameterName, InputStream x, long length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setCharacterStream(String parameterName, Reader reader, long length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setAsciiStream(String parameterName, InputStream x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setBinaryStream(String parameterName, InputStream x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setCharacterStream(String parameterName, Reader reader) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setNCharacterStream(String parameterName, Reader value) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setClob(String parameterName, Reader reader) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setBlob(String parameterName, InputStream inputStream) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setNClob(String parameterName, Reader reader) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public <T> T getObject(int parameterIndex, Class<T> type) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public <T> T getObject(String parameterName, Class<T> type) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public void registerOutParameter(int parameterIndex, int sqlType) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void registerOutParameter(int parameterIndex, int sqlType, int scale) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public boolean wasNull() throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public String getString(int parameterIndex) throws SQLException {
		try {
			return (String) params.get(parameterIndex);
		} catch (Exception e) {
			throw new SQLException(e);
		}
	}

	@Override
	public boolean getBoolean(int parameterIndex) throws SQLException {
		try {
			return (boolean) params.get(parameterIndex);
		} catch (Exception e) {
			throw new SQLException(e);
		}
	}

	@Override
	public byte getByte(int parameterIndex) throws SQLException {
		try {
			return (byte) params.get(parameterIndex);
		} catch (Exception e) {
			throw new SQLException(e);
		}
	}

	@Override
	public short getShort(int parameterIndex) throws SQLException {
		try {
			return (short) params.get(parameterIndex);
		} catch (Exception e) {
			throw new SQLException(e);
		}
	}

	@Override
	public int getInt(int parameterIndex) throws SQLException {
		try {
			return (int) params.get(parameterIndex);
		} catch (Exception e) {
			throw new SQLException(e);
		}
	}

	@Override
	public long getLong(int parameterIndex) throws SQLException {
		try {
			return (long) params.get(parameterIndex);
		} catch (Exception e) {
			throw new SQLException(e);
		}
	}

	@Override
	public float getFloat(int parameterIndex) throws SQLException {
		try {
			return (float) params.get(parameterIndex);
		} catch (Exception e) {
			throw new SQLException(e);
		}
	}

	@Override
	public double getDouble(int parameterIndex) throws SQLException {
		try {
			return (double) params.get(parameterIndex);
		} catch (Exception e) {
			throw new SQLException(e);
		}
	}

	@Override
	public BigDecimal getBigDecimal(int parameterIndex, int scale) throws SQLException {
		try {
			return (BigDecimal) params.get(parameterIndex);
		} catch (Exception e) {
			throw new SQLException(e);
		}
	}

	@Override
	public byte[] getBytes(int parameterIndex) throws SQLException {
		try {
			return (byte[]) params.get(parameterIndex);
		} catch (Exception e) {
			throw new SQLException(e);
		}
	}

	@Override
	public Date getDate(int parameterIndex) throws SQLException {
		try {
			return (Date) params.get(parameterIndex);
		} catch (Exception e) {
			throw new SQLException(e);
		}
	}

	@Override
	public Time getTime(int parameterIndex) throws SQLException {
		try {
			return (Time) params.get(parameterIndex);
		} catch (Exception e) {
			throw new SQLException(e);
		}
	}

	@Override
	public Timestamp getTimestamp(int parameterIndex) throws SQLException {
		try {
			return (Timestamp) params.get(parameterIndex);
		} catch (Exception e) {
			throw new SQLException(e);
		}
	}

	@Override
	public Object getObject(int parameterIndex) throws SQLException {
		try {
			return (Object) params.get(parameterIndex);
		} catch (Exception e) {
			throw new SQLException(e);
		}
	}

	@Override
	public BigDecimal getBigDecimal(int parameterIndex) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Object getObject(int parameterIndex, Map<String, Class<?>> map) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Ref getRef(int parameterIndex) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Blob getBlob(int parameterIndex) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Clob getClob(int parameterIndex) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Array getArray(int parameterIndex) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Date getDate(int parameterIndex, Calendar cal) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Time getTime(int parameterIndex, Calendar cal) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Timestamp getTimestamp(int parameterIndex, Calendar cal) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public void registerOutParameter(int parameterIndex, int sqlType, String typeName) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void registerOutParameter(String parameterName, int sqlType) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void registerOutParameter(String parameterName, int sqlType, int scale) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void registerOutParameter(String parameterName, int sqlType, String typeName) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public URL getURL(int parameterIndex) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public void setURL(String parameterName, URL val) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setNull(String parameterName, int sqlType) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setBoolean(String parameterName, boolean x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setByte(String parameterName, byte x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setShort(String parameterName, short x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setInt(String parameterName, int x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setLong(String parameterName, long x) throws SQLException {
		if ("EIBCALEN".equals(parameterName)) {
			eibCALen = x;
		}
	}

	@Override
	public void setFloat(String parameterName, float x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setDouble(String parameterName, double x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setBigDecimal(String parameterName, BigDecimal x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setString(String parameterName, String x) throws SQLException {
		if ("EIBAID".equals(parameterName) && (x != null)) {
			eibAID = x.charAt(0);
		}
	}

	@Override
	public void setBytes(String parameterName, byte[] x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setDate(String parameterName, Date x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setTime(String parameterName, Time x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setTimestamp(String parameterName, Timestamp x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setAsciiStream(String parameterName, InputStream x, int length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setBinaryStream(String parameterName, InputStream x, int length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setObject(String parameterName, Object x, int targetSqlType, int scale) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setObject(String parameterName, Object x, int targetSqlType) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setObject(String parameterName, Object x) throws SQLException {
		if ("COMMAREA".equals(parameterName) && (x != null) && (x instanceof char[])) {
			commArea = (char[]) x;
		}
	}

	@Override
	public void setCharacterStream(String parameterName, Reader reader, int length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setDate(String parameterName, Date x, Calendar cal) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setTime(String parameterName, Time x, Calendar cal) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setTimestamp(String parameterName, Timestamp x, Calendar cal) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setNull(String parameterName, int sqlType, String typeName) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public String getString(String parameterName) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public boolean getBoolean(String parameterName) throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public byte getByte(String parameterName) throws SQLException {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public short getShort(String parameterName) throws SQLException {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public int getInt(String parameterName) throws SQLException {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public long getLong(String parameterName) throws SQLException {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public float getFloat(String parameterName) throws SQLException {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public double getDouble(String parameterName) throws SQLException {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public byte[] getBytes(String parameterName) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Date getDate(String parameterName) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Time getTime(String parameterName) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Timestamp getTimestamp(String parameterName) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Object getObject(String parameterName) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public BigDecimal getBigDecimal(String parameterName) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Object getObject(String parameterName, Map<String, Class<?>> map) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Ref getRef(String parameterName) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Blob getBlob(String parameterName) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Clob getClob(String parameterName) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Array getArray(String parameterName) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Date getDate(String parameterName, Calendar cal) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Time getTime(String parameterName, Calendar cal) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Timestamp getTimestamp(String parameterName, Calendar cal) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public URL getURL(String parameterName) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public RowId getRowId(int parameterIndex) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public RowId getRowId(String parameterName) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public void setRowId(String parameterName, RowId x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setNString(String parameterName, String value) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setNCharacterStream(String parameterName, Reader value, long length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setNClob(String parameterName, NClob value) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setClob(String parameterName, Reader reader, long length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setBlob(String parameterName, InputStream inputStream, long length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void setNClob(String parameterName, Reader reader, long length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public NClob getNClob(int parameterIndex) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public NClob getNClob(String parameterName) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public void setSQLXML(String parameterName, SQLXML xmlObject) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public SQLXML getSQLXML(int parameterIndex) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public SQLXML getSQLXML(String parameterName) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public String getNString(int parameterIndex) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public String getNString(String parameterName) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Reader getNCharacterStream(int parameterIndex) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Reader getNCharacterStream(String parameterName) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Reader getCharacterStream(int parameterIndex) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Reader getCharacterStream(String parameterName) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public void setBlob(String parameterName, Blob x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void connectionClosed(ConnectionEvent event) {
		try {
			this.close();
		} catch (SQLException e) {
		}
	}

	@Override
	public void connectionErrorOccurred(ConnectionEvent event) {
		// TODO Auto-generated method stub
	}

}
