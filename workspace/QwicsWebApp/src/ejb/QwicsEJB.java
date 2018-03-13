/*******************************************************************************************/
/*   QWICS Server Java EE Web Application                                             */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 12.03.2018                                  */
/*                                                                                         */
/*   Copyright (C) 2018 by Philipp Brune  Email: Philipp.Brune@hs-neu-ulm.de               */
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

package ejb;

import java.sql.CallableStatement;
import java.sql.Connection;
import java.sql.ResultSet;
import java.util.HashMap;

import javax.annotation.Resource;
import javax.ejb.LocalBean;
import javax.ejb.Stateful;
import javax.ejb.TransactionAttribute;
import javax.ejb.TransactionAttributeType;
import javax.ejb.TransactionManagement;
import javax.ejb.TransactionManagementType;
import javax.sql.DataSource;
import javax.transaction.UserTransaction;
import javax.websocket.OnClose;
import javax.websocket.OnMessage;
import javax.websocket.Session;
import javax.websocket.server.ServerEndpoint;


@Stateful
@LocalBean
@ServerEndpoint("/endpoints/qwicsejb")
@TransactionManagement(TransactionManagementType.BEAN)
public class QwicsEJB {
	@Resource
	private UserTransaction utx;
	
//	@PersistenceContext( unitName = "QWICS" )
//	private EntityManager em;
	
	@Resource(mappedName="java:jboss/datasources/QwicsDS") 
	DataSource datasource;	
	
	private int state = 0;
	private boolean isValue = false;
	private String name = "";
	private String value = "";
	private Connection con;
	private CallableStatement call;
	private ResultSet maps;
	private HashMap<String,String> transactionProgNames = new HashMap<String,String>();

		
	public QwicsEJB() {
		this.transactionProgNames.put("MM01", "MN1APP");
		this.transactionProgNames.put("AMSQ", "MN1SQL");
		this.transactionProgNames.put("CQU1", "CQ2UPD");
	}


	public boolean nextSend(Session session) throws Exception {
		boolean isSend = false;
		do {
			isSend = maps.next();
			if (isSend && "SEND".equals(maps.getString("MAP_CMD"))) {
				session.getBasicRemote().sendText(maps.getString("JSON"));
				return true;
			}			
			String program = "";
			if (!isSend) {
				// RETURN
				String transId = maps.getString("TRANSID");
				System.out.println("Return with transId "+transId);
				program = transactionProgNames.get(transId);
				try {
					utx.commit();
					utx.begin();
					con = datasource.getConnection(con.getClientInfo("conId"),"");
				} catch (Exception e) {
					utx.rollback();
					utx.begin();
					con = datasource.getConnection(con.getClientInfo("conId"),"");
					return false;
				}
				if ((transId != null) && (transId.length() > 0)) {
					String aidStr = maps.getString("EIBAID");
					long calen = maps.getLong("EIBCALEN");
					call.close();
					call = con.prepareCall("PROGRAM "+program);
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
	
	
	@OnMessage
	@TransactionAttribute(TransactionAttributeType.REQUIRED)
	public void onMessage(Session session, String msg) {
		System.out.println(msg+" "+state);
		System.out.println("OK");
		try {		
			if ((state == 0) && msg.startsWith("GETMAP")) {
				String program = msg.substring(7);
				utx.begin();
				con = datasource.getConnection();
				call = con.prepareCall("PROGRAM "+program);
				maps = call.executeQuery();
				if (nextSend(session)) {
					state = 1;
				} else {
					state = 0; // EOT
				}
				return;
			}
			if ((state == 1) && "GETMAP".equals(msg)) {
				if (nextSend(session)) {
					state = 1;
				} else {
					state = 0; // EOT
				}
				return;
			}
			if ((state == 1) && "SENDDATA".equals(msg)) {
				state = 2;
				isValue = false;
				return;
			}
			if ((state == 2) && "ENDDATA".equals(msg)) {
				state = 1;
				return;
			}
			if (state == 2) {
				if (!isValue) {
					name = msg;
					isValue = true;					
				} else {
					value = msg;
					System.out.println(name+" "+value);
					maps.updateString(name, value);
					isValue = false;					
				}
			}
		} catch (Exception e) {
			e.printStackTrace();
		}
	}
	
	@OnClose
	@TransactionAttribute(TransactionAttributeType.REQUIRED)
	public void onClose(Session session) {
		try {
			utx.commit();
		} catch (Exception e) {
			try {
				utx.rollback(); 
			} catch (Exception ex) {
			}
		}
		try {
			maps.close();
			call.close();
			con.close();
		} catch (Exception e) {
		}
	}

}
