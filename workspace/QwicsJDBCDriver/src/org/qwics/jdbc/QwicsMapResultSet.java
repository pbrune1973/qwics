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
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Calendar;
import java.util.GregorianCalendar;
import java.util.HashMap;
import java.util.Map;

import org.qwics.jdbc.msg.MsgHeaderConverter;
import org.qwics.jdbc.msg.QueueHandler;
import org.qwics.jdbc.msg.QueueManager;
import org.qwics.jdbc.msg.QueueWrapper;
import org.qwics.jdbc.scheduler.ScheduleTimer;
import org.qwics.jdbc.scheduler.TaScheduled;

public class QwicsMapResultSet implements ResultSet, ResultSetMetaData {
	private QwicsConnection conn;
	private boolean closed = false;
	private boolean syncpoint = false;
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
	private ArrayList<HashMap<String, HashMap<String, char[]>>> channelStack = null;
	private static HashMap<String, ArrayList<char[]>> tsQueues = new HashMap<String, ArrayList<char[]>>();
	private static HashMap<String, Integer> tsQueuesLastRead = new HashMap<String, Integer>();
	private static HashMap<String, ArrayList<char[]>> tdQueues = new HashMap<String, ArrayList<char[]>>();
	private static HashMap<String, Integer> tdQueuesLastRead = new HashMap<String, Integer>();

	public QwicsMapResultSet(QwicsConnection conn, long eibCALen, char eibAID) {
		this.conn = conn;
		this.mapCmd = "";
		this.eibCALen = eibCALen;
		this.eibAID = eibAID;
		this.mapValues = conn.getMapValues();
		this.mapNames = conn.getMapNames();
		this.nameIndices = conn.getNameIndices();
		this.channelStack = new ArrayList<HashMap<String, HashMap<String, char[]>>>();
		channelStack.add(new HashMap<String, HashMap<String, char[]>>());
		channelStack.get(0).put("DFHTRANSACTION", new HashMap<String, char[]>());
		try {
			conn.sendCmd("" + eibCALen);
			conn.sendCmd("" + eibAID);
		} catch (Exception e) {
		}
	}

	public static HashMap<String, ArrayList<char[]>> getTsQueues() {
		return tsQueues;
	}

	public static HashMap<String, Integer> getTsQueuesLastRead() {
		return tsQueuesLastRead;
	}

	public static HashMap<String, ArrayList<char[]>> getTdQueues() {
		return tdQueues;
	}

	public static HashMap<String, Integer> getTdQueuesLastRead() {
		return tdQueuesLastRead;
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
		int len = -1, size = 0;
		boolean useInto = false;
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
					if ("LENGTH".equals(vals[0])) {
						try {
							len = Integer.parseInt(vals[1]);
						} catch (Exception e) {
							e.printStackTrace();
						}
					} else if ("SIZE".equals(vals[0])) {
						try {
							size = Integer.parseInt(vals[1]);
						} catch (Exception e) {
							e.printStackTrace();
						}
					} else
						putMapValue(vals[0], vals[1]);
				} else {
					putMapValue(vals[0], "");
				}
			} else {
				lastMapName = name;
				if ("INTO".equals(name)) {
					useInto = true;
				}
				if (!"MAP".equals(name) && !"MAPSET".equals(name) && !"INTO".equals(name) && !"LENGTH".equals(name)
						&& !"SIZE".equals(name)) {
					try {
						if ("EIBAID".equals(name)) {
							conn.sendCmd("" + eibAID);
							eibAID = ' ';
						} else {
							conn.sendCmd(mapValues.get(nameIndices.get(name)));
						}
					} catch (Exception e) {
						conn.sendCmd("");
					}
				}
			}
		}
		if (useInto) {
			int resp = 0, resp2 = 0;
			if (len < 0) {
				len = 0;
			}
			if (len < size) {
				resp = 22;
			}
			char buf[] = new char[size];
			try {
				String input = getString("TERMINPUT");
				if (input != null) {
					int l = 0;
					if (input.length() < buf.length) {
						l = input.length();
					} else {
						l = buf.length;
					}
					for (int i = 0; i < l; i++) {
						buf[i] = input.charAt(i);
					}
				}
			} catch (Exception e) {
			}
			conn.sendBuf(buf);

			conn.sendCmd("" + resp);
			conn.sendCmd("" + resp2);

		}
	}

	@Override
	public boolean next() throws SQLException {
		if (closed)
			return false;
		try {
			if (syncpoint) {
				syncpoint = false;
				putMapValue("SYNCPOINT", "false");
				if ("ROLLBACK".equals(getString("SYNCPOINTRESULT")) && !getBoolean("ROLLBACK")) {
					conn.sendCmd("END-SYNCPOINT ROLLBACK");
				} else {
					conn.sendCmd("END-SYNCPOINT");
				}
				putMapValue("ROLLBACK", "false");
			}
			while (true) {
				int resp = 0;
				int resp2 = 0;
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
					channelStack.remove(channelStack.size() - 1);
				} else if (mapCmd.startsWith("XCTL")) {
					String name = "";
					String channel = "";
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
								if ("CHANNEL".equals(vals[0])) {
									channel = vals[1].trim();
								}
							}
						} else {
							lastMapName = name;
						}
					}
					HashMap<String, HashMap<String, char[]>> channels = channelStack.get(channelStack.size() - 1);
					channelStack.add(new HashMap<String, HashMap<String, char[]>>());
					channelStack.get(channelStack.size() - 1).put("DFHTRANSACTION", channels.get("DFHTRANSACTION"));
					if (!"".equals(channel)) {
						HashMap<String, char[]> chn = channels.get(channel);
						if (chn != null) {
							channelStack.get(channelStack.size() - 1).put(channel, chn);
							channelStack.get(channelStack.size() - 1).put("current", chn);
						}
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
					System.err.println("ABEND: ABCODE=" + getString("ABOCDE"));
				} else if (mapCmd.startsWith("SYNCPOINT")) {
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
							if ("ROLLBACK".equals(name)) {
								putMapValue(name, "true");
							}
						}
					}
					putMapValue("SYNCPOINT", "true");
					syncpoint = true;
					return true;
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
								if (l > 48)
									l = 48;
								for (int i = 0; i < l; i++) {
									startMsg[i + 8] = qn.charAt(i);
								}
								for (int i = l + 8; i < n; i++) {
									startMsg[i] = ' ';
								}

								startMsg[168] = 0x00;
								startMsg[169] = 0x00;
								startMsg[170] = 0x00;
								startMsg[171] = 0x00;

								try {
									String data = getString("TRIGGERDATA");
									l = data.length();
									if (l > 64)
										l = 64;
									for (int i = 0; i < l; i++) {
										startMsg[i + 104] = data.charAt(i);
									}
								} catch (Exception e) {
								}

								try {
									String data = getString("ENVDATA");
									l = data.length();
									if (l > 128)
										l = 128;
									for (int i = 0; i < l; i++) {
										startMsg[i + 428] = data.charAt(i);
									}
								} catch (Exception e) {
								}

								try {
									String data = getString("USERDATA");
									l = data.length();
									if (l > 128)
										l = 128;
									for (int i = 0; i < l; i++) {
										startMsg[i + 556] = data.charAt(i);
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

								conn.sendCmd("" + obj);
								conn.sendCmd("" + compcode);
								conn.sendCmd("" + reason);
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
									queueHandler.closeQueue(obj, opts);
								} catch (Exception e) {
									compcode = 2;
								}

								conn.sendCmd("" + compcode);
								conn.sendCmd("" + reason);
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
									HashMap<String, Object> msgHeader = MsgHeaderConverter.getConverter()
											.toMap(msgDesc);
									q.get(msgHeader, msgBody);
									MsgHeaderConverter.getConverter().toRaw(msgHeader, msgDesc);
								} catch (Exception e) {
									e.printStackTrace();
									compcode = 2;
								}

								conn.sendBuf(msgDesc);
								conn.sendBuf(msgBody);
								conn.sendCmd("" + msgLen);
								conn.sendCmd("" + compcode);
								conn.sendCmd("" + reason);
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
									HashMap<String, Object> msgHeader = MsgHeaderConverter.getConverter()
											.toMap(msgDesc);
									q.put(msgHeader, msgBody);
									MsgHeaderConverter.getConverter().toRaw(msgHeader, msgDesc);
								} catch (Exception e) {
									e.printStackTrace();
									compcode = 2;
								}

								conn.sendBuf(msgDesc);
								conn.sendCmd("" + compcode);
								conn.sendCmd("" + reason);
							}
						} catch (Exception e) {
							e.printStackTrace();
						}
						mode++;
					}
				} else if (mapCmd.startsWith("PUT")) {
					String name = "";
					String container = "";
					String channel = "";
					int len = -1, size = 0;
					boolean append = false;
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
								if ("FLENGTH".equals(vals[0])) {
									try {
										len = Integer.parseInt(vals[1]);
									} catch (Exception e) {
										e.printStackTrace();
									}
									if (len < 0) {
										resp = 22;
										resp2 = 1;
									}
								}
								if ("SIZE".equals(vals[0])) {
									try {
										size = Integer.parseInt(vals[1]);
									} catch (Exception e) {
										e.printStackTrace();
									}
								}
								if ("CONTAINER".equals(vals[0])) {
									container = vals[1].trim();
								}
								if ("CHANNEL".equals(vals[0])) {
									channel = vals[1].trim();
								}
							}
						} else {
							lastMapName = name;
							if ("APPEND".equals(name)) {
								append = true;
							}
						}
					}
					if (len < 0) {
						len = size;
					}
					if (len >= 0) {
						HashMap<String, HashMap<String, char[]>> channels = channelStack.get(channelStack.size() - 1);
						HashMap<String, char[]> chn = null;
						if ("".equals(channel)) {
							chn = channels.get("current");
						} else {
							chn = channels.get(channel);
							if (chn == null) {
								chn = new HashMap<String, char[]>();
								channels.put(channel, chn);
							}
						}
						char[] buf = new char[len];
						conn.readBuf(buf);
						if (append) {
							char oldBuf[] = chn.get(container);
							if (oldBuf != null) {
								int l = oldBuf.length;
								oldBuf = Arrays.copyOf(oldBuf, l + buf.length);
								for (int i = 0; i < buf.length; i++) {
									oldBuf[i + l] = buf[i];
								}
								buf = oldBuf;
							}
						}
						chn.put(container, buf);
					}
					conn.sendCmd("" + resp);
					conn.sendCmd("" + resp2);
				} else if (mapCmd.startsWith("GET")) {
					String name = "";
					String container = "";
					String channel = "";
					int len = -1, size = 0;
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
								if ("FLENGTH".equals(vals[0])) {
									try {
										len = Integer.parseInt(vals[1]);
									} catch (Exception e) {
										e.printStackTrace();
									}
									if (len < 0) {
										resp = 22;
										resp2 = 1;
									}
								}
								if ("SIZE".equals(vals[0])) {
									try {
										size = Integer.parseInt(vals[1]);
									} catch (Exception e) {
										e.printStackTrace();
									}
								}
								if ("CONTAINER".equals(vals[0])) {
									container = vals[1].trim();
								}
								if ("CHANNEL".equals(vals[0])) {
									channel = vals[1].trim();
								}
							}
						} else {
							lastMapName = name;
						}
					}
					if (len < 0) {
						len = size;
					}
					HashMap<String, HashMap<String, char[]>> channels = channelStack.get(channelStack.size() - 1);
					HashMap<String, char[]> chn = null;
					if ("".equals(channel)) {
						chn = channels.get("current");
					} else {
						chn = channels.get(channel);
					}
					char[] buf = null;
					if (chn == null) {
						buf = new char[len];
						resp = 122;
						resp2 = 2;
					} else {
						buf = chn.get(container);
						if (buf == null) {
							buf = new char[len];
							resp = 110;
							resp2 = 10;
						}
					}
					if ((len > 0) && (len < buf.length)) {
						buf = Arrays.copyOf(buf, len);
						resp = 22;
						resp2 = 11;
					}
					conn.sendBuf(buf);
					conn.sendCmd("" + resp);
					conn.sendCmd("" + resp2);
				} else if (mapCmd.startsWith("LINK")) {
					String name = "";
					String channel = "";
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
								if ("CHANNEL".equals(vals[0])) {
									channel = vals[1].trim();
								}
							}
						} else {
							lastMapName = name;
						}
					}
					HashMap<String, HashMap<String, char[]>> channels = channelStack.get(channelStack.size() - 1);
					channelStack.add(new HashMap<String, HashMap<String, char[]>>());
					channelStack.get(channelStack.size() - 1).put("DFHTRANSACTION", channels.get("DFHTRANSACTION"));
					if (!"".equals(channel)) {
						HashMap<String, char[]> chn = channels.get(channel);
						if (chn != null) {
							channelStack.get(channelStack.size() - 1).put(channel, chn);
							channelStack.get(channelStack.size() - 1).put("current", chn);
						}
					}
				} else if (mapCmd.startsWith("WRITEQ")) {
					String name = "";
					String queue = "";
					int item = -1;
					int len = -1, size = 0;
					boolean rewrite = false;
					boolean td = false;
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
								if ("LENGTH".equals(vals[0])) {
									try {
										len = Integer.parseInt(vals[1]);
									} catch (Exception e) {
										e.printStackTrace();
									}
								}
								if ("SIZE".equals(vals[0])) {
									try {
										size = Integer.parseInt(vals[1]);
									} catch (Exception e) {
										e.printStackTrace();
									}
								}
								if ("QUEUE".equals(vals[0])) {
									queue = vals[1].trim();
								}
								if ("ITEM".equals(vals[0])) {
									try {
										item = Integer.parseInt(vals[1]);
									} catch (Exception e) {
										e.printStackTrace();
									}
								}
							}
						} else {
							lastMapName = name;
							if ("TD".equals(name)) {
								td = true;
							}
							if ("REWRITE".equals(name)) {
								rewrite = true;
							}
						}
					}
					if (len < 0) {
						len = size;
					}
					if ((len < 0) || (len > 32768)) {
						resp = 22;
					}
					if (!td) {
						char[] buf = new char[len];
						conn.readBuf(buf);
						if (!"".equals(queue) && (resp == 0)) {
							synchronized (tsQueues) {
								ArrayList<char[]> q = tsQueues.get(queue);
								if (q == null) {
									q = new ArrayList<char[]>();
									tsQueues.put(queue, q);
									tsQueuesLastRead.put(queue, -1);
								}
								if ((item > 0) && (q.size() > 0)) {
									if ((item <= q.size()) && rewrite) {
										q.set(item - 1, buf);
									} else {
										resp = 26;
									}
								} else {
									q.add(buf);
									item = q.size();
									if (item > 32768) {
										resp = 26;
									}
								}
							}
						} else {
							resp = 16;
						}
					} else {
						char[] buf = new char[len];
						conn.readBuf(buf);
						if (!"".equals(queue) && (resp == 0)) {
							synchronized (tdQueues) {
								ArrayList<char[]> q = tdQueues.get(queue);
								if (q == null) {
									q = new ArrayList<char[]>();
									tdQueues.put(queue, q);
									tdQueuesLastRead.put(queue, -1);
								}
								q.add(buf);
								item = q.size();
							}
						} else {
							resp = 16;
						}
					}

					conn.sendCmd("" + item);
					conn.sendCmd("" + resp);
					conn.sendCmd("" + resp2);
				} else if (mapCmd.startsWith("READQ")) {
					String name = "";
					String queue = "";
					int item = -1;
					int len = -1, size = 0;
					boolean next = false;
					boolean td = false;
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
								if ("LENGTH".equals(vals[0])) {
									try {
										len = Integer.parseInt(vals[1]);
									} catch (Exception e) {
										e.printStackTrace();
									}
								}
								if ("SIZE".equals(vals[0])) {
									try {
										size = Integer.parseInt(vals[1]);
									} catch (Exception e) {
										e.printStackTrace();
									}
								}
								if ("QUEUE".equals(vals[0])) {
									queue = vals[1].trim();
								}
								if ("ITEM".equals(vals[0])) {
									try {
										item = Integer.parseInt(vals[1]);
									} catch (Exception e) {
										e.printStackTrace();
									}
								}
							}
						} else {
							lastMapName = name;
							if ("TD".equals(name)) {
								td = true;
							}
							if ("NEXT".equals(name)) {
								next = true;
							}
						}
					}
					if (len < 0) {
						len = 0;
					}
					if (len < size) {
						resp = 22;
					}
					char buf[] = new char[size];
					if (!td) {
						if (!"".equals(queue) && (resp == 0)) {
							synchronized (tsQueues) {
								ArrayList<char[]> q = tsQueues.get(queue);
								if (q != null) {
									if (next) {
										item = tsQueuesLastRead.get(queue) + 1;
										if (item >= tsQueues.size()) {
											resp = 26;
										} else {
											tsQueuesLastRead.put(queue, item);
											buf = tsQueues.get(queue).get(item);
											item++;
										}
									} else if ((item >= 1) && (item <= q.size())) {
										buf = tsQueues.get(queue).get(item - 1);
										tsQueuesLastRead.put(queue, item - 1);
									} else {
										resp = 26;
									}
								} else {
									resp = 44;
								}
							}
						} else {
							resp = 16;
						}
					} else {
						if (!"".equals(queue) && (resp == 0)) {
							synchronized (tdQueues) {
								ArrayList<char[]> q = tdQueues.get(queue);
								if (q != null) {
									item = tdQueuesLastRead.get(queue) + 1;
									if (item >= tdQueues.size()) {
										resp = 23;
									} else {
										buf = tdQueues.get(queue).get(item);
										tdQueues.get(queue).remove(item);
										tdQueuesLastRead.put(queue, item - 1);
										item++;
									}
								} else {
									resp = 44;
								}
								if (resp == 0) {
									conn.sendBuf(buf);
								}
							}
						} else {
							resp = 16;
						}
					}

					conn.sendBuf(buf);
					conn.sendCmd("" + item);
					conn.sendCmd("" + resp);
					conn.sendCmd("" + resp2);
				} else if (mapCmd.startsWith("DELETEQ")) {
					String name = "";
					String queue = "";
					boolean td = false;
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
								if ("QUEUE".equals(vals[0])) {
									queue = vals[1].trim();
								}
							}
						} else {
							lastMapName = name;
							if ("TD".equals(name)) {
								td = true;
							}
						}
					}
					if (!td) {
						if (!"".equals(queue) && (resp == 0)) {
							synchronized (tsQueues) {
								ArrayList<char[]> q = tsQueues.get(queue);
								if (q != null) {
									tsQueues.remove(queue);
								} else {
									resp = 44;
								}
							}
						} else {
							resp = 16;
						}
					} else {
						if (!"".equals(queue) && (resp == 0)) {
							synchronized (tdQueues) {
								ArrayList<char[]> q = tdQueues.get(queue);
								if (q != null) {
									tdQueues.remove(queue);
								} else {
									resp = 44;
								}
							}
						} else {
							resp = 16;
						}
					}

					conn.sendCmd("" + resp);
					conn.sendCmd("" + resp2);
				} else if (mapCmd.startsWith("ASSIGN")) {
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
								if ("SYSID".equals(vals[0])) {
									conn.sendCmd("QWIC");
								} else {
									char buf[] = new char[2048];
									Arrays.fill(buf, (char) 0);
									conn.sendBuf(buf);
									conn.sendCmd("");
								}
							}
						} else {
							lastMapName = name;
						}
					}

					conn.sendCmd("" + resp);
					conn.sendCmd("" + resp2);
				} else if (mapCmd.startsWith("FORMATTIME")) {
					String name = "";
					String dateSep = "", timeSep = "";
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
								if ("DATESEP".equals(vals[0])) {
									dateSep = "" + vals[1].charAt(0);
								} else if ("TIMESEP".equals(vals[0])) {
									timeSep = "" + vals[1].charAt(0);
								} else if ("ABSTIME".equals(vals[0])) {
									long millis = System.currentTimeMillis();
									GregorianCalendar cal = new GregorianCalendar(1900, 0, 1, 0, 0, 0);
									long offset = cal.getTimeInMillis();
									millis = millis - offset;
									conn.sendCmd("" + millis);
								} else if ("YEAR".equals(vals[0])) {
									GregorianCalendar cal = new GregorianCalendar();
									conn.sendCmd("" + cal.getTime().getYear());
								} else if ("TIME".equals(vals[0])) {
									GregorianCalendar cal = new GregorianCalendar();
									SimpleDateFormat fmt = new SimpleDateFormat("HH" + timeSep + "mm" + timeSep + "ss");
									System.out.println(cal.getTime() + " " + fmt.format(cal.getTime()));
									conn.sendCmd(fmt.format(cal.getTime()));
								} else if ("YYMMDD".equals(vals[0])) {
									GregorianCalendar cal = new GregorianCalendar();
									SimpleDateFormat fmt = new SimpleDateFormat("yy" + dateSep + "MM" + dateSep + "dd");
									System.out.println(cal.getTime() + " " + fmt.format(cal.getTime()));
									conn.sendCmd(fmt.format(cal.getTime()));
								} else if ("DDMMYY".equals(vals[0])) {
									GregorianCalendar cal = new GregorianCalendar();
									SimpleDateFormat fmt = new SimpleDateFormat("dd" + dateSep + "MM" + dateSep + "yy");
									System.out.println(cal.getTime() + " " + fmt.format(cal.getTime()));
									conn.sendCmd(fmt.format(cal.getTime()));
								} else {
									char buf[] = new char[2048];
									Arrays.fill(buf, (char) 0);
									conn.sendBuf(buf);
									conn.sendCmd("");
								}
							}
						} else {
							lastMapName = name;
						}
					}

					conn.sendCmd("" + resp);
					conn.sendCmd("" + resp2);
				} else if (mapCmd.startsWith("ASKTIME")) {
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
								if ("ABSTIME".equals(vals[0])) {
									long millis = System.currentTimeMillis();
									GregorianCalendar cal = new GregorianCalendar(1900, 0, 1, 0, 0, 0);
									long offset = cal.getTimeInMillis();
									millis = millis - offset;
									conn.sendCmd("" + millis);
								} else {
									char buf[] = new char[2048];
									Arrays.fill(buf, (char) 0);
									conn.sendBuf(buf);
									conn.sendCmd("");
								}
							}
						} else {
							lastMapName = name;
						}
					}

					conn.sendCmd("" + resp);
					conn.sendCmd("" + resp2);
				} else if (mapCmd.startsWith("INQUIRE")) {
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
								if ("CONNECTST".equals(vals[0])) {
									conn.sendCmd("690");
								} else {
									char buf[] = new char[2048];
									Arrays.fill(buf, (char) 0);
									conn.sendBuf(buf);
									conn.sendCmd("");
								}
							}
						} else {
							lastMapName = name;
						}
					}

					conn.sendCmd("" + resp);
					conn.sendCmd("" + resp2);
				} else if (mapCmd.startsWith("START") || mapCmd.startsWith("CANCEL")) {
					String transId = "";
					String reqId = "";
					char from[] = null;
					boolean hasFrom = false;
					int len = -1, size = 0;
					String interval = "000000";
					String time = "";

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
								if ("TRANSID".equals(vals[0])) {
									transId = vals[1];
								}
								if ("REQID".equals(vals[0])) {
									reqId = vals[1];
								}
								if ("TIME".equals(vals[0])) {
									time = vals[1];
								}
								if ("INTERVAL".equals(vals[0])) {
									interval = vals[1];
								}
								if ("LENGTH".equals(vals[0])) {
									try {
										len = Integer.parseInt(vals[1]);
									} catch (Exception e) {
										e.printStackTrace();
									}
									if (len <= 0) {
										resp = 22;
									}
								}
								if ("SIZE".equals(vals[0])) {
									try {
										size = Integer.parseInt(vals[1]);
									} catch (Exception e) {
										e.printStackTrace();
									}
								}
								if ("FROM".equals(vals[0])) {
									hasFrom = true;
								}
							}
						} else {
							lastMapName = name;
						}
					}

					if (!"".equals(transId)) {
						if (!"".equals(reqId) && (resp == 0)) {
							if (hasFrom) {
								int l;
								if ((len >= 0) && (len < size)) {
									l = len;
								} else {
									l = size;
								}
								from = new char[l];
								conn.readBuf(from);
							}
							TaScheduled ta = null;

							try {
								if (!"".equals(time)) {
									ta = new TaScheduled(transId, reqId, from, time, false);
								} else {
									ta = new TaScheduled(transId, reqId, from, interval, true);
								}
							} catch (NumberFormatException e) {
								resp = 16;
								if ("min".equals(e.getMessage())) {
									resp2 = 5;
								}
								if ("sec".equals(e.getMessage())) {
									resp2 = 6;
								}
							}

							if (mapCmd.startsWith("START")) {
								ScheduleTimer.getScheduleTimer().scheduleTa(ta);
							} else {
								if (ScheduleTimer.getScheduleTimer().cancelTa(ta) < 0) {
								}
							}
						} else {
							if (resp == 0) {
								resp = 16;
							}
						}
					} else {
						resp = 28;
					}

					conn.sendCmd("" + resp);
					conn.sendCmd("" + resp2);
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
		// mapValues.set(columnIndex, "" + x);
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
		if ((columnLabel != null) && columnLabel.startsWith("CHANNEL") && (x instanceof char[])) {
			String p[] = columnLabel.split(":");
			if (p.length == 3) {
				String channel = p[1];
				String container = p[2];
				HashMap<String, HashMap<String, char[]>> channels = channelStack.get(channelStack.size() - 1);
				HashMap<String, char[]> chn = null;
				if ("".equals(channel)) {
					chn = channels.get("current");
				} else {
					chn = channels.get(channel);
					if (chn == null) {
						chn = new HashMap<String, char[]>();
						channels.put(channel, chn);
					}
				}
				chn.put(container, (char[])x);				
			}
			return;
		}
		if ("QMGR".equals(columnLabel)) {
			if (queueHandler == null) {
				this.queueManager = (QueueManager) x;
				this.queueHandler = new QueueHandler(queueManager);
			}
			return;
		}
		mapValues.set(getColumnIndex(columnLabel), "" + x);
	}

	@Override
	public void updateObject(String columnLabel, Object x) throws SQLException {
		if ((columnLabel != null) && columnLabel.startsWith("CHANNEL") && (x instanceof char[])) {
			String p[] = columnLabel.split(":");
			if (p.length == 3) {
				String channel = p[1];
				String container = p[2];
				HashMap<String, HashMap<String, char[]>> channels = channelStack.get(channelStack.size() - 1);
				HashMap<String, char[]> chn = null;
				if ("".equals(channel)) {
					chn = channels.get("current");
				} else {
					chn = channels.get(channel);
					if (chn == null) {
						chn = new HashMap<String, char[]>();
						channels.put(channel, chn);
					}
				}
				chn.put(container, (char[])x);				
			}
			return;
		}
		if ("QMGR".equals(columnLabel)) {
			if (queueHandler == null) {
				this.queueManager = (QueueManager) x;
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
