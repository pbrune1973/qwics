/*
Qwics JDBC Client for Java

Copyright (c) 2018 Philipp Brune    Email: Philipp.Brune@hs-neu-ulm.de

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

import java.io.InputStream;
import java.io.Reader;
import java.math.BigDecimal;
import java.net.URL;
import java.sql.Array;
import java.sql.Blob;
import java.sql.Clob;
import java.sql.Date;
import java.sql.NClob;
import java.sql.Ref;
import java.sql.ResultSet;
import java.sql.ResultSetMetaData;
import java.sql.RowId;
import java.sql.SQLException;
import java.sql.SQLWarning;
import java.sql.SQLXML;
import java.sql.Statement;
import java.sql.Time;
import java.sql.Timestamp;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.HashMap;
import java.util.Map;

import org.qwics.jdbc.msg.MsgHeaderConverter;
import org.qwics.jdbc.msg.QueueHandler;
import org.qwics.jdbc.msg.QueueManager;
import org.qwics.jdbc.msg.QueueWrapper;

public class QwicsMapResultSet implements ResultSet, ResultSetMetaData {
	private QwicsConnection conn;
	private boolean closed = false;
	private String mapCmd;
	private ArrayList<String> mapNames;
	private ArrayList<String> mapValues;
	private HashMap<String, Integer> nameIndices;
	private long eibCALen = 0;
	private char eibAID = '"';
	private String transId = "";
	private String lastMapName = "";
	private QueueManager queueManager = null;
	private QueueHandler queueHandler = null;
	

	public QwicsMapResultSet(QwicsConnection conn, long eibCALen, char eibAID) {
		this.conn = conn;
		this.mapCmd = "";
		this.eibCALen = eibCALen;
		this.eibAID = eibAID;
		this.mapValues = conn.getMapValues();
		this.mapNames = conn.getMapNames();
		this.nameIndices = conn.getNameIndices();
		try {
			conn.sendCmd("" + eibCALen);
			conn.sendCmd("" + eibAID);
		} catch (Exception e) {
		}
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

	private void readMapValues() throws Exception {
		String name = "";
		while (!"".equals(name = conn.readResult())) {
			if (name.contains("=")) {
				String vals[] = null;
				if (name.startsWith("=")) {
					vals = new String[2];
					vals[0] = lastMapName;
					vals[1] = name.substring(1);
				} else {
					vals = name.split("=", 2);
				}
				if (vals.length > 1) {
					if (vals[1].startsWith("'")) {
						vals[1] = vals[1].substring(1, vals[1].length() - 1);
					}
					putMapValue(vals[0], vals[1]);
				} else {
					putMapValue(vals[0], "");
				}
			} else {
				lastMapName = name;
				if (!"MAP".equals(name) && !"MAPSET".equals(name) && !"FROM".equals(name)) {
					putMapValue(name, "true");
				}
				if ("FROM".equals(name)) {
					name = conn.readResult();
				}
			}
		}
	}

	private void sendMapValues() throws Exception {
		String name = "";
		while (!"".equals(name = conn.readResult())) {
			if (name.contains("=")) {
				String vals[] = null;
				if (name.startsWith("=")) {
					vals = new String[2];
					vals[0] = lastMapName;
					vals[1] = name.substring(1);
				} else {
					vals = name.split("=", 2);
				}
				if (vals.length > 1) {
					if (vals[1].startsWith("'")) {
						vals[1] = vals[1].substring(1, vals[1].length() - 1);
					}
					putMapValue(vals[0], vals[1]);
				} else {
					putMapValue(vals[0], "");
				}
			} else {
				lastMapName = name;
				if (!"MAP".equals(name) && !"MAPSET".equals(name) && !"INTO".equals(name)) {
					try {
						if ("EIBAID".equals(name)) {
							conn.sendCmd("" + eibAID);		
							eibAID =  ' ';
						} else {
							conn.sendCmd(mapValues.get(nameIndices.get(name)));
						}
					} catch (Exception e) {
						conn.sendCmd("");
					}
				}
			}
		}
	}

	@Override
	public boolean next() throws SQLException {
		if (closed)
			return false;
		try {
			while (true) {
				mapCmd = conn.readResult();
				putMapValue("MAP_CMD", mapCmd);
				// System.out.println(mapCmd);
				if (mapCmd.startsWith("STOP")) {
					closed = true;
					return false;
				} else if (mapCmd.startsWith("SEND")) {
					mapNames.clear();
					mapValues.clear();
					nameIndices.clear();
					putMapValue("MAP_CMD", mapCmd);
					readMapValues();
					return true;
				} else if (mapCmd.startsWith("RECEIVE")) {
					sendMapValues();
				} else if (mapCmd.startsWith("RETURN")) {
					String name = "";
					int into = 0;
					while (!"".equals(name = conn.readResult())) {
						if (into == 1) {
							if (name.startsWith("=")) {
								name = name.substring(1);
							}
							try {
								this.eibCALen = Long.parseLong(name);
							} catch (Exception e) {
							}
							into = 0;
						}
						if (into == 2) {
							if (name.startsWith("=")) {
								name = name.substring(1);
							}
							try {
								if (name.startsWith("'")) {
									this.transId = name.substring(1, name.length() - 1);
								}
							} catch (Exception e) {
							}
							into = 0;
						}
						if ("LENGTH".equals(name)) {
							into = 1;
						}
						if ("TRANSID".equals(name)) {
							into = 2;
						}
					}
				} else if (mapCmd.startsWith("XCTL")) {
					while (!"".equals(conn.readResult())) {
					}
					eibCALen = 0;
					conn.sendCmd("" + eibCALen);
					conn.sendCmd("" + eibAID);
				} else if (mapCmd.startsWith("ABEND")) {
					String name = "";					
					while (!"".equals(name = conn.readResult())) {
						if (name.contains("=")) {
							String vals[] = null;
							if (name.startsWith("=")) {
								vals = new String[2];
								vals[0] = lastMapName;
								vals[1] = name.substring(1);
							} 
							if ((vals != null) && (vals.length == 2)) {
								if (vals[1].startsWith("'")) {
									vals[1] = vals[1].substring(1, vals[1].length() - 1);
								}
								putMapValue(vals[0], vals[1]);
							} 
						} else {
							lastMapName = name;
							if (!"ABCODE".equals(name)) {
								putMapValue(name, "true");
							}
						}
					}
				} else if (mapCmd.startsWith("RETRIEVE")) {
					String name = "";
					boolean into = false;
					while (!"".equals(name = conn.readResult())) {
						if (into) {
							try {
								int n = Integer.parseInt(name);
								char startMsg[] = new char[n];
								startMsg[0] = ' ';
								startMsg[1] = ' ';
								startMsg[2] = ' ';
								startMsg[3] = ' ';
								
								startMsg[4] = 0x01;
								startMsg[5] = 0x00;
								startMsg[6] = 0x00;
								startMsg[7] = 0x00;
								
								String qn = getString("QNAME");
								int l = qn.length();
								if (l > 48) l = 48;
								for (int i = 0; i < l; i++) {
									startMsg[i+8] = qn.charAt(i);
								}
								for (int i = l+8; i < n; i++) {
									startMsg[i] = ' ';
								}

								startMsg[168] = 0x00;
								startMsg[169] = 0x00;
								startMsg[170] = 0x00;
								startMsg[171] = 0x00;

								try {
									String data = getString("TRIGGERDATA");
									l = data.length();
									if (l > 64) l = 64;
									for (int i = 0; i < l; i++) {
										startMsg[i+104] = data.charAt(i);
									}
								} catch (Exception e) {
								}

								try {
									String data = getString("ENVDATA");
									l = data.length();
									if (l > 128) l = 128;
									for (int i = 0; i < l; i++) {
										startMsg[i+428] = data.charAt(i);
									}
								} catch (Exception e) {
								}

								try {
									String data = getString("USERDATA");
									l = data.length();
									if (l > 128) l = 128;
									for (int i = 0; i < l; i++) {
										startMsg[i+556] = data.charAt(i);
									}
								} catch (Exception e) {
								}

								conn.sendBuf(startMsg);
							} catch (Exception e) {
								e.printStackTrace();
							}
							into = false;
						}
						if (!into & "INTO".equals(name)) {
							into = true;
						}
					}
				} else if (mapCmd.startsWith("QOPEN")) {
					String name = "";
					int mode = 0;
					int objType = 0;
					int opts = 0;
					String objName = "";
					int obj = 0;
					int compcode = 0;
					int reason = 0;
					while (!"".equals(name = conn.readResult())) {
						try {
							if (mode == 0) {
								objType = Integer.parseInt(name);
							}
							if (mode == 1) {
								objName = name.trim();
							}
							if (mode == 2) {
								opts = Integer.parseInt(name);
								
								try {
									obj = queueHandler.openQueue(objName, objType, opts);
								} catch (Exception e) {
									compcode = 2;
								}

								conn.sendCmd(""+obj);
								conn.sendCmd(""+compcode);
								conn.sendCmd(""+reason);
							}
						} catch (Exception e) {
							e.printStackTrace();
						}
						mode++;
					}
				} else if (mapCmd.startsWith("QCLOSE")) {
					String name = "";
					int mode = 0;
					int opts = 0;
					int obj = 0;
					int compcode = 0;
					int reason = 0;
					while (!"".equals(name = conn.readResult())) {
						try {
							if (mode == 0) {
								obj = Integer.parseInt(name);
							}
							if (mode == 1) {
								opts = Integer.parseInt(name);

								try {
									queueHandler.closeQueue(obj,opts);
								} catch (Exception e) {
									compcode = 2;
								}

								conn.sendCmd(""+compcode);
								conn.sendCmd(""+reason);
							}
						} catch (Exception e) {
							e.printStackTrace();
						}
						mode++;
					}
				} else if (mapCmd.startsWith("QGET")) {
					String name = "";
					int mode = 0;
					int obj = 0;
					char msgDesc[] = new char[364];
					char msgOpts[] = new char[100];
					int msgLen = 0;
					char msgBody[] = null;
					int compcode = 0;
					int reason = 0;
					while (!"".equals(name = conn.readResult())) {
						try {
							if (mode == 0) {
								obj = Integer.parseInt(name);

								conn.readBuf(msgDesc);
								conn.readBuf(msgOpts);
							}
							if (mode == 1) {
								msgLen = Integer.parseInt(name);
								msgBody = new char[msgLen];
								
								try {
									QueueWrapper q = queueHandler.getQueue(obj);
									HashMap<String,Object> msgHeader = MsgHeaderConverter.getConverter().toMap(msgDesc);
									q.get(msgHeader,msgBody);
									MsgHeaderConverter.getConverter().toRaw(msgHeader,msgDesc);
								} catch (Exception e) {
									e.printStackTrace();
									compcode = 2;
								}

								conn.sendBuf(msgDesc);
								conn.sendBuf(msgBody);
								conn.sendCmd(""+msgLen);
								conn.sendCmd(""+compcode);
								conn.sendCmd(""+reason);
							}
						} catch (Exception e) {
							e.printStackTrace();
						}
						mode++;
					}
				} else if (mapCmd.startsWith("QPUT")) {
					String name = "";
					int mode = 0;
					int obj = 0;
					char msgDesc[] = new char[364];
					char msgOpts[] = new char[152];
					int msgLen = 0;
					char msgBody[] = null;
					int compcode = 0;
					int reason = 0;
					while (!"".equals(name = conn.readResult())) {
						try {
							if (mode == 0) {
								obj = Integer.parseInt(name);

								conn.readBuf(msgDesc);
								conn.readBuf(msgOpts);
							}
							if (mode == 1) {
								msgLen = Integer.parseInt(name);
								msgBody = new char[msgLen];
								conn.readBuf(msgBody);
								
								try {
									QueueWrapper q = queueHandler.getQueue(obj);
									HashMap<String,Object> msgHeader = MsgHeaderConverter.getConverter().toMap(msgDesc);
									q.put(msgHeader,msgBody);
									MsgHeaderConverter.getConverter().toRaw(msgHeader,msgDesc);
								} catch (Exception e) {
									e.printStackTrace();
									compcode = 2;
								}

								conn.sendBuf(msgDesc);
								conn.sendCmd(""+compcode);
								conn.sendCmd(""+reason);
							}
						} catch (Exception e) {
							e.printStackTrace();
						}
						mode++;
					}
				} else {
					if (!"".equals(mapCmd)) {
						putMapValue("MAP_CMD", mapCmd);
						while (!"".equals(conn.readResult())) {
						}
					}
				}
			}
		} catch (Exception e) {
			closed = true;
			e.printStackTrace();
			throw new SQLException(e);
		}
	}

	public void putMapValue(String name, String value) {
		Integer i = nameIndices.get(name);
		if (i != null) {
			mapValues.set(i, value);
		} else {
			i = mapNames.size();
			mapNames.add(name);
			mapValues.add(value);
			nameIndices.put(name, i);
		}
	}

	public String getJson() {
		StringBuffer json = new StringBuffer();
		json.append('{');
		for (int i = 0; i < mapNames.size(); i++) {
			if (!mapNames.get(i).equals("MAP") && !mapNames.get(i).equals("MAPSET")) {
				if (i > 0) {
					json.append(',');
				}
				json.append('"');
				json.append(mapNames.get(i));
				json.append('"');
				json.append(':');
				if (!mapNames.get(i).equals("JSON")) {
					json.append('"');
					json.append(mapValues.get(i));
					json.append('"');
				} else {
					json.append(mapValues.get(i));
				}
			}
		}
		json.append('}');
		return json.toString();
	}

	private int getColumnIndex(String columnLabel) {
		if (!nameIndices.containsKey(columnLabel)) {
			mapValues.add("");
			nameIndices.put(columnLabel, mapValues.size() - 1);
		}
		return nameIndices.get(columnLabel);
	}

	@Override
	public void close() throws SQLException {
		// Read remaining data and ignore it
		while (next())
			;
	}

	@Override
	public boolean wasNull() throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public String getString(int columnIndex) throws SQLException {
		return mapValues.get(columnIndex);
	}

	@Override
	public boolean getBoolean(int columnIndex) throws SQLException {
		try {
			if (mapValues.get(columnIndex) != null) {
				return Boolean.parseBoolean(mapValues.get(columnIndex));
			}
		} catch (Exception e) {
			throw new SQLException(e);
		}
		return false;
	}

	@Override
	public byte getByte(int columnIndex) throws SQLException {
		try {
			if (mapValues.get(columnIndex) != null) {
				return Byte.parseByte(mapValues.get(columnIndex));
			}
		} catch (Exception e) {
			throw new SQLException(e);
		}
		return 0;
	}

	@Override
	public short getShort(int columnIndex) throws SQLException {
		try {
			if (mapValues.get(columnIndex) != null) {
				return Short.parseShort(mapValues.get(columnIndex));
			}
		} catch (Exception e) {
			throw new SQLException(e);
		}
		return 0;
	}

	@Override
	public int getInt(int columnIndex) throws SQLException {
		try {
			if (mapValues.get(columnIndex) != null) {
				return Integer.parseInt(mapValues.get(columnIndex));
			}
		} catch (Exception e) {
			throw new SQLException(e);
		}
		return 0;
	}

	@Override
	public long getLong(int columnIndex) throws SQLException {
		try {
			if (mapValues.get(columnIndex) != null) {
				return Long.parseLong(mapValues.get(columnIndex));
			}
		} catch (Exception e) {
			throw new SQLException(e);
		}
		return 0;
	}

	@Override
	public float getFloat(int columnIndex) throws SQLException {
		try {
			if (mapValues.get(columnIndex) != null) {
				return Float.parseFloat(mapValues.get(columnIndex));
			}
		} catch (Exception e) {
			throw new SQLException(e);
		}
		return 0;
	}

	@Override
	public double getDouble(int columnIndex) throws SQLException {
		try {
			if (mapValues.get(columnIndex) != null) {
				return Double.parseDouble(mapValues.get(columnIndex));
			}
		} catch (Exception e) {
			throw new SQLException(e);
		}
		return 0;
	}

	@Override
	public BigDecimal getBigDecimal(int columnIndex, int scale) throws SQLException {
		try {
			if (mapValues.get(columnIndex) != null) {
				return new BigDecimal(mapValues.get(columnIndex));
			}
		} catch (Exception e) {
			throw new SQLException(e);
		}
		return null;
	}

	@Override
	public byte[] getBytes(int columnIndex) throws SQLException {
		try {
			if (mapValues.get(columnIndex) != null) {
				return mapValues.get(columnIndex).getBytes();
			}
		} catch (Exception e) {
			throw new SQLException(e);
		}
		return null;
	}

	@Override
	public Date getDate(int columnIndex) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Time getTime(int columnIndex) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Timestamp getTimestamp(int columnIndex) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public InputStream getAsciiStream(int columnIndex) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public InputStream getUnicodeStream(int columnIndex) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public InputStream getBinaryStream(int columnIndex) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public String getString(String columnLabel) throws SQLException {
		if (columnLabel.equals("JSON")) {
			return getJson();
		}
		if (columnLabel.equals("TRANSID")) {
			return transId;
		}
		if (columnLabel.equals("EIBAID")) {
			return "" + eibAID;
		}
		try {
			return getString(nameIndices.get(columnLabel));
		} catch (Exception e) {
			throw new SQLException(e);
		}
	}

	@Override
	public boolean getBoolean(String columnLabel) throws SQLException {
		return getBoolean(nameIndices.get(columnLabel));
	}

	@Override
	public byte getByte(String columnLabel) throws SQLException {
		return getByte(nameIndices.get(columnLabel));
	}

	@Override
	public short getShort(String columnLabel) throws SQLException {
		return getShort(nameIndices.get(columnLabel));
	}

	@Override
	public int getInt(String columnLabel) throws SQLException {
		return getInt(nameIndices.get(columnLabel));
	}

	@Override
	public long getLong(String columnLabel) throws SQLException {
		if (columnLabel.equals("EIBCALEN")) {
			return eibCALen;
		}
		return getLong(nameIndices.get(columnLabel));
	}

	@Override
	public float getFloat(String columnLabel) throws SQLException {
		return getFloat(nameIndices.get(columnLabel));
	}

	@Override
	public double getDouble(String columnLabel) throws SQLException {
		return getDouble(nameIndices.get(columnLabel));
	}

	@Override
	public BigDecimal getBigDecimal(String columnLabel, int scale) throws SQLException {
		return getBigDecimal(nameIndices.get(columnLabel));
	}

	@Override
	public byte[] getBytes(String columnLabel) throws SQLException {
		return getBytes(nameIndices.get(columnLabel));
	}

	@Override
	public Date getDate(String columnLabel) throws SQLException {
		return getDate(nameIndices.get(columnLabel));
	}

	@Override
	public Time getTime(String columnLabel) throws SQLException {
		return getTime(nameIndices.get(columnLabel));
	}

	@Override
	public Timestamp getTimestamp(String columnLabel) throws SQLException {
		return getTimestamp(nameIndices.get(columnLabel));
	}

	@Override
	public InputStream getAsciiStream(String columnLabel) throws SQLException {
		return getAsciiStream(nameIndices.get(columnLabel));
	}

	@Override
	public InputStream getUnicodeStream(String columnLabel) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public InputStream getBinaryStream(String columnLabel) throws SQLException {
		// TODO Auto-generated method stub
		return null;
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
	public String getCursorName() throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public ResultSetMetaData getMetaData() throws SQLException {
		return this;
	}

	@Override
	public Object getObject(int columnIndex) throws SQLException {
		try {
			if (mapValues.get(columnIndex) != null) {
				return (Object) mapValues.get(columnIndex);
			}
		} catch (Exception e) {
			throw new SQLException(e);
		}
		return null;
	}

	@Override
	public Object getObject(String columnLabel) throws SQLException {
		return getObject(nameIndices.get(columnLabel));
	}

	@Override
	public int findColumn(String columnLabel) throws SQLException {
		return nameIndices.get(columnLabel);
	}

	@Override
	public Reader getCharacterStream(int columnIndex) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Reader getCharacterStream(String columnLabel) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public BigDecimal getBigDecimal(int columnIndex) throws SQLException {
		try {
			if (mapValues.get(columnIndex) != null) {
				return new BigDecimal(mapValues.get(columnIndex));
			}
		} catch (Exception e) {
			throw new SQLException(e);
		}
		return null;
	}

	@Override
	public BigDecimal getBigDecimal(String columnLabel) throws SQLException {
		return getBigDecimal(nameIndices.get(columnLabel));
	}

	@Override
	public boolean isBeforeFirst() throws SQLException {
		if (mapCmd.equals("") && !closed) {
			return true;
		}
		return false;
	}

	@Override
	public boolean isAfterLast() throws SQLException {
		return closed;
	}

	@Override
	public boolean isFirst() throws SQLException {
		return true;
	}

	@Override
	public boolean isLast() throws SQLException {
		return false;
	}

	@Override
	public void beforeFirst() throws SQLException {
		throw new SQLException("Not implented!");
	}

	@Override
	public void afterLast() throws SQLException {
		throw new SQLException("Not implented!");
	}

	@Override
	public boolean first() throws SQLException {
		throw new SQLException("Not implented!");
	}

	@Override
	public boolean last() throws SQLException {
		throw new SQLException("Not implented!");
	}

	@Override
	public int getRow() throws SQLException {
		return 0;
	}

	@Override
	public boolean absolute(int row) throws SQLException {
		throw new SQLException("Not implented!");
	}

	@Override
	public boolean relative(int rows) throws SQLException {
		throw new SQLException("Not implented!");
	}

	@Override
	public boolean previous() throws SQLException {
		throw new SQLException("Not implented!");
	}

	@Override
	public void setFetchDirection(int direction) throws SQLException {
		throw new SQLException("Not implented!");
	}

	@Override
	public int getFetchDirection() throws SQLException {
		throw new SQLException("Not implented!");
	}

	@Override
	public void setFetchSize(int rows) throws SQLException {
		throw new SQLException("Not implented!");
	}

	@Override
	public int getFetchSize() throws SQLException {
		throw new SQLException("Not implented!");
	}

	@Override
	public int getType() throws SQLException {
		throw new SQLException("Not implented!");
	}

	@Override
	public int getConcurrency() throws SQLException {
		throw new SQLException("Not implented!");
	}

	@Override
	public boolean rowUpdated() throws SQLException {
		throw new SQLException("Not implented!");
	}

	@Override
	public boolean rowInserted() throws SQLException {
		throw new SQLException("Not implented!");
	}

	@Override
	public boolean rowDeleted() throws SQLException {
		throw new SQLException("Not implented!");
	}

	@Override
	public void updateNull(int columnIndex) throws SQLException {
	}

	@Override
	public void updateBoolean(int columnIndex, boolean x) throws SQLException {
		mapValues.set(columnIndex, "" + x);
	}

	@Override
	public void updateByte(int columnIndex, byte x) throws SQLException {
		mapValues.set(columnIndex, "" + x);
	}

	@Override
	public void updateShort(int columnIndex, short x) throws SQLException {
		mapValues.set(columnIndex, "" + x);
	}

	@Override
	public void updateInt(int columnIndex, int x) throws SQLException {
		mapValues.set(columnIndex, "" + x);
	}

	@Override
	public void updateLong(int columnIndex, long x) throws SQLException {
		mapValues.set(columnIndex, "" + x);
	}

	@Override
	public void updateFloat(int columnIndex, float x) throws SQLException {
		mapValues.set(columnIndex, "" + x);
	}

	@Override
	public void updateDouble(int columnIndex, double x) throws SQLException {
		mapValues.set(columnIndex, "" + x);
	}

	@Override
	public void updateBigDecimal(int columnIndex, BigDecimal x) throws SQLException {
		mapValues.set(columnIndex, "" + x);
	}

	@Override
	public void updateString(int columnIndex, String x) throws SQLException {
		mapValues.set(columnIndex, x);
	}

	@Override
	public void updateBytes(int columnIndex, byte[] x) throws SQLException {
		//mapValues.set(columnIndex, "" + x);
	}

	@Override
	public void updateDate(int columnIndex, Date x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateTime(int columnIndex, Time x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateTimestamp(int columnIndex, Timestamp x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateAsciiStream(int columnIndex, InputStream x, int length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateBinaryStream(int columnIndex, InputStream x, int length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateCharacterStream(int columnIndex, Reader x, int length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateObject(int columnIndex, Object x, int scaleOrLength) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateObject(int columnIndex, Object x) throws SQLException {
		mapValues.set(columnIndex, "" + x);
	}

	@Override
	public void updateNull(String columnLabel) throws SQLException {
		// TODO Auto-generated method stub
	}

	@Override
	public void updateBoolean(String columnLabel, boolean x) throws SQLException {
		mapValues.set(getColumnIndex(columnLabel), "" + x);
	}

	@Override
	public void updateByte(String columnLabel, byte x) throws SQLException {
		mapValues.set(getColumnIndex(columnLabel), "" + x);
	}

	@Override
	public void updateShort(String columnLabel, short x) throws SQLException {
		mapValues.set(getColumnIndex(columnLabel), "" + x);
	}

	@Override
	public void updateInt(String columnLabel, int x) throws SQLException {
		mapValues.set(getColumnIndex(columnLabel), "" + x);
	}

	@Override
	public void updateLong(String columnLabel, long x) throws SQLException {
		mapValues.set(getColumnIndex(columnLabel), "" + x);
	}

	@Override
	public void updateFloat(String columnLabel, float x) throws SQLException {
		mapValues.set(getColumnIndex(columnLabel), "" + x);
	}

	@Override
	public void updateDouble(String columnLabel, double x) throws SQLException {
		mapValues.set(getColumnIndex(columnLabel), "" + x);
	}

	@Override
	public void updateBigDecimal(String columnLabel, BigDecimal x) throws SQLException {
		mapValues.set(getColumnIndex(columnLabel), "" + x);
	}

	@Override
	public void updateString(String columnLabel, String x) throws SQLException {
		if (columnLabel.equals("EIBAID")) {
			eibAID = x.charAt(0);
			return;
		}
		putMapValue(columnLabel, x);
	}

	@Override
	public void updateBytes(String columnLabel, byte[] x) throws SQLException {
		// mapValues.set(getColumnIndex(columnLabel), "" + x);
	}

	@Override
	public void updateDate(String columnLabel, Date x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateTime(String columnLabel, Time x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateTimestamp(String columnLabel, Timestamp x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateAsciiStream(String columnLabel, InputStream x, int length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateBinaryStream(String columnLabel, InputStream x, int length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateCharacterStream(String columnLabel, Reader reader, int length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateObject(String columnLabel, Object x, int scaleOrLength) throws SQLException {
		if ("QMGR".equals(columnLabel)) {
			if (queueHandler == null) {
				this.queueManager = (QueueManager)x;
				this.queueHandler = new QueueHandler(queueManager);
			}
			return;
		}
		mapValues.set(getColumnIndex(columnLabel), "" + x);
	}

	@Override
	public void updateObject(String columnLabel, Object x) throws SQLException {
		if ("QMGR".equals(columnLabel)) {
			if (queueHandler == null) {
				this.queueManager = (QueueManager)x;
				this.queueHandler = new QueueHandler(queueManager);
			}
			return;
		}
		mapValues.set(getColumnIndex(columnLabel), "" + x);
	}

	@Override
	public void insertRow() throws SQLException {
		throw new SQLException("Not implented!");
	}

	@Override
	public void updateRow() throws SQLException {
		throw new SQLException("Not implented!");
	}

	@Override
	public void deleteRow() throws SQLException {
		throw new SQLException("Not implented!");
	}

	@Override
	public void refreshRow() throws SQLException {
		throw new SQLException("Not implented!");
	}

	@Override
	public void cancelRowUpdates() throws SQLException {
		throw new SQLException("Not implented!");
	}

	@Override
	public void moveToInsertRow() throws SQLException {
		throw new SQLException("Not implented!");
	}

	@Override
	public void moveToCurrentRow() throws SQLException {
		throw new SQLException("Not implented!");
	}

	@Override
	public Statement getStatement() throws SQLException {
		throw new SQLException("Not implented!");
	}

	@Override
	public Object getObject(int columnIndex, Map<String, Class<?>> map) throws SQLException {
		return getObject(columnIndex);
	}

	@Override
	public Ref getRef(int columnIndex) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Blob getBlob(int columnIndex) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Clob getClob(int columnIndex) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Array getArray(int columnIndex) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Object getObject(String columnLabel, Map<String, Class<?>> map) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Ref getRef(String columnLabel) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Blob getBlob(String columnLabel) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Clob getClob(String columnLabel) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Array getArray(String columnLabel) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Date getDate(int columnIndex, Calendar cal) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Date getDate(String columnLabel, Calendar cal) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Time getTime(int columnIndex, Calendar cal) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Time getTime(String columnLabel, Calendar cal) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Timestamp getTimestamp(int columnIndex, Calendar cal) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Timestamp getTimestamp(String columnLabel, Calendar cal) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public URL getURL(int columnIndex) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public URL getURL(String columnLabel) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public void updateRef(int columnIndex, Ref x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateRef(String columnLabel, Ref x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateBlob(int columnIndex, Blob x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateBlob(String columnLabel, Blob x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateClob(int columnIndex, Clob x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateClob(String columnLabel, Clob x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateArray(int columnIndex, Array x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateArray(String columnLabel, Array x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public RowId getRowId(int columnIndex) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public RowId getRowId(String columnLabel) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public void updateRowId(int columnIndex, RowId x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateRowId(String columnLabel, RowId x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public int getHoldability() throws SQLException {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public boolean isClosed() throws SQLException {
		// TODO Auto-generated method stub
		return this.closed;
	}

	@Override
	public void updateNString(int columnIndex, String nString) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateNString(String columnLabel, String nString) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateNClob(int columnIndex, NClob nClob) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateNClob(String columnLabel, NClob nClob) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public NClob getNClob(int columnIndex) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public NClob getNClob(String columnLabel) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public SQLXML getSQLXML(int columnIndex) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public SQLXML getSQLXML(String columnLabel) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public void updateSQLXML(int columnIndex, SQLXML xmlObject) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateSQLXML(String columnLabel, SQLXML xmlObject) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public String getNString(int columnIndex) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public String getNString(String columnLabel) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Reader getNCharacterStream(int columnIndex) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Reader getNCharacterStream(String columnLabel) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public void updateNCharacterStream(int columnIndex, Reader x, long length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateNCharacterStream(String columnLabel, Reader reader, long length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateAsciiStream(int columnIndex, InputStream x, long length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateBinaryStream(int columnIndex, InputStream x, long length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateCharacterStream(int columnIndex, Reader x, long length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateAsciiStream(String columnLabel, InputStream x, long length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateBinaryStream(String columnLabel, InputStream x, long length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateCharacterStream(String columnLabel, Reader reader, long length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateBlob(int columnIndex, InputStream inputStream, long length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateBlob(String columnLabel, InputStream inputStream, long length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateClob(int columnIndex, Reader reader, long length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateClob(String columnLabel, Reader reader, long length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateNClob(int columnIndex, Reader reader, long length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateNClob(String columnLabel, Reader reader, long length) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateNCharacterStream(int columnIndex, Reader x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateNCharacterStream(String columnLabel, Reader reader) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateAsciiStream(int columnIndex, InputStream x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateBinaryStream(int columnIndex, InputStream x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateCharacterStream(int columnIndex, Reader x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateAsciiStream(String columnLabel, InputStream x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateBinaryStream(String columnLabel, InputStream x) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateCharacterStream(String columnLabel, Reader reader) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateBlob(int columnIndex, InputStream inputStream) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateBlob(String columnLabel, InputStream inputStream) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateClob(int columnIndex, Reader reader) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateClob(String columnLabel, Reader reader) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateNClob(int columnIndex, Reader reader) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public void updateNClob(String columnLabel, Reader reader) throws SQLException {
		// TODO Auto-generated method stub

	}

	@Override
	public <T> T getObject(int columnIndex, Class<T> type) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public <T> T getObject(String columnLabel, Class<T> type) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public int getColumnCount() throws SQLException {
		return 0;
	}

	@Override
	public boolean isAutoIncrement(int column) throws SQLException {
		return false;
	}

	@Override
	public boolean isCaseSensitive(int column) throws SQLException {
		return true;
	}

	@Override
	public boolean isSearchable(int column) throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public boolean isCurrency(int column) throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public int isNullable(int column) throws SQLException {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public boolean isSigned(int column) throws SQLException {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public int getColumnDisplaySize(int column) throws SQLException {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public String getColumnLabel(int column) throws SQLException {
		return mapNames.get(column);
	}

	@Override
	public String getColumnName(int column) throws SQLException {
		return mapNames.get(column);
	}

	@Override
	public String getSchemaName(int column) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public int getPrecision(int column) throws SQLException {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public int getScale(int column) throws SQLException {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public String getTableName(int column) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public String getCatalogName(int column) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public int getColumnType(int column) throws SQLException {
		// TODO Auto-generated method stub
		return 0;
	}

	@Override
	public String getColumnTypeName(int column) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public boolean isReadOnly(int column) throws SQLException {
		return false;
	}

	@Override
	public boolean isWritable(int column) throws SQLException {
		return true;
	}

	@Override
	public boolean isDefinitelyWritable(int column) throws SQLException {
		return true;
	}

	@Override
	public String getColumnClassName(int column) throws SQLException {
		// TODO Auto-generated method stub
		return null;
	}

}
