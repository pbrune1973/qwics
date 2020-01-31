/*******************************************************************************************/
/*   QWICS Server Java EE Web Application                                                  */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 31.01.2020                                  */
/*                                                                                         */
/*   Copyright (C) 2019 by Philipp Brune  Email: Philipp.Brune@hs-neu-ulm.de               */
/*                                                                                         */
/*   This file is part of of the QWICS Server project.                                     */
/*                                                                                         */
/*   QWICS Server is free software: you can redistribute it and/or modify it under the     */
/*   terms of the GNU General Public License as published by the Free Software Foundation, */
/*   either version 3 of the License, or (at your option) any later version.               */
/*   It is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;       */
/*   without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR      */
/*   PURPOSE.  See the GNU General Public License for more details.                        */
/*                                                                                         */
/*   You should have received a copy of the GNU General Public License                     */
/*   along with this project. If not, see <http://www.gnu.org/licenses/>.                  */
/*******************************************************************************************/

package ws;

import java.sql.CallableStatement;
import java.sql.Connection;
import java.sql.ResultSet;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Random;

import javax.annotation.Resource;
import javax.ejb.LocalBean;
import javax.ejb.Stateless;
import javax.ejb.TransactionAttribute;
import javax.ejb.TransactionAttributeType;
import javax.ejb.TransactionManagement;
import javax.ejb.TransactionManagementType;
import javax.sql.DataSource;
import javax.transaction.UserTransaction;
import javax.ws.rs.GET;
import javax.ws.rs.POST;
import javax.ws.rs.Path;
import javax.ws.rs.PathParam;
import javax.ws.rs.Produces;
import javax.ws.rs.core.MediaType;

@Stateless
@LocalBean
@TransactionManagement(TransactionManagementType.BEAN)
@Path("ta")
public class QwicsWS {
	@Resource
	private UserTransaction utx;

	@Resource(mappedName = "java:jboss/datasources/QwicsDS")
	DataSource datasource;

	private Connection con;
	private CallableStatement call;
	private ResultSet maps;
	private static HashMap<String, String> transactionProgNames = new HashMap<String, String>();
	private String conId = ""; // Unique identifier for the QWICsconnection per EJB instance
	private ArrayList<String> logQueues = new ArrayList<String>(); 

	public QwicsWS() {
		// Create unique identifier for the QWICsconnection of this EJB instance
		Random rand = new Random();
		conId = "";
		for (int i = 0; i < 10; i++) {
			char c = (char) (65 + rand.nextInt(25));
			conId = conId + c;
		}
	}

	private void logQueue(String qName) {
		try {
			List<char[]> q = (List<char[]>) maps.getObject("TDQUEUES LIST "+qName);
			if (q != null) {
				int i = 0;
				for (char[] msg : q) {
					System.err.printf("%s%s%d",qName," ",i);
					String s = "";
					for (int n = 0; n < msg.length; n++) {
						System.err.printf(" %x",(byte)msg[n]);
						s = s + msg[n];
						if (n > 0 && n < msg.length-1 && (n % 16) == 0) {
							System.err.printf(" %s\n%s%s%d",s,qName," ",i);
							s = "";
						}
					}
					System.err.printf(" %s\n",s);
					System.err.printf("\n");
					i++;
				}						
			}
		} catch (Exception ex) {
			ex.printStackTrace();
		}						
	}
	
	public boolean nextSend() throws Exception {
		boolean isSend = false;
		do {
			isSend = maps.next();
	
			if (isSend && maps.getBoolean("SYNCPOINT")) {
				if (!maps.getBoolean("ROLLBACK")) {
					try {
						utx.commit();
						utx.begin();
						con.close();
						con = datasource.getConnection(conId, "");
						maps.updateString("SYNCPOINTRESULT", "COMMIT");
					} catch (Exception e) {
						e.printStackTrace();
						maps.updateString("SYNCPOINTRESULT", "ROLLBACK");
						utx.rollback();
						utx.begin();
						con.close();
						con = datasource.getConnection(conId, "");
					}
				} else {
					maps.updateString("SYNCPOINTRESULT", "ROLLBACK");
					utx.rollback();
					utx.begin();
					con.close();
					con = datasource.getConnection(conId, "");
				}
				continue;
			}
			if (isSend && "SEND".equals(maps.getString("MAP_CMD"))) {
				return true;
			}
			String program = "";
			if (!isSend) {
				// RETURN
				for (String qn : logQueues) {
					logQueue(qn);
				}
				String transId = maps.getString("TRANSID");
				// System.out.println("Return with transId "+transId);
				program = transactionProgNames.get(transId);
				// System.out.println("Program name "+program);
				try {
					utx.commit();
					utx.begin();
					con.close();
					con = datasource.getConnection(conId, "");
				} catch (Exception e) {
					e.printStackTrace();
					utx.rollback();
					utx.begin();
					con.close();
					con = datasource.getConnection(conId, "");
					return false;
				}
				if ((transId != null) && (transId.length() > 0)) {
					String aidStr = maps.getString("EIBAID");
					long calen = maps.getLong("EIBCALEN");
					call.close();
					call = con.prepareCall("PROGRAM " + program);
					call.setLong("EIBCALEN", calen);
					call.setString("EIBAID", aidStr);
					maps = call.executeQuery();
				} else {
					return false;
				}
			}
		} while (!isSend);
		return true;
	}

	@POST
	@Path("define/{transId}/{program}")
	@Produces(MediaType.APPLICATION_JSON)
	@TransactionAttribute(TransactionAttributeType.REQUIRED)
	public boolean defineTransId(@PathParam("transId") String transId, @PathParam("program") String program) {
		if ((transId.length() == 4) && (program.length() <= 8)) {
			transactionProgNames.put(transId.toUpperCase(), program.toUpperCase());
			return true;
		}
		return false;
	}

	@GET
	@Path("call/{transId}")
	@Produces(MediaType.APPLICATION_JSON)
	@TransactionAttribute(TransactionAttributeType.REQUIRES_NEW)
	public boolean callTransId(@PathParam("transId") String transId) {
		String program = transactionProgNames.get(transId);
		if (program == null) {
			return false;
		}
		try {
			utx.begin();
			con = datasource.getConnection(conId, "");
			call = con.prepareCall("PROGRAM " + program);
			maps = call.executeQuery();
			while (nextSend()) {
			}
			utx.commit();
		} catch (Exception e) {
			try {
				utx.rollback();
			} catch (Exception ex) {
				ex.printStackTrace();
			}
			return false;
		} finally {
			try {
				con.close();
			} catch (Exception ex) {
				ex.printStackTrace();
				return false;
			}
		}
		return true;
	}

	@POST
	@Path("logqueue/{qName}")
	@Produces(MediaType.APPLICATION_JSON)
	@TransactionAttribute(TransactionAttributeType.REQUIRED)
	public boolean defineLogQueue(@PathParam("qName") String qName) {
		logQueues.add(qName.toUpperCase());
		return true;
	}

}
