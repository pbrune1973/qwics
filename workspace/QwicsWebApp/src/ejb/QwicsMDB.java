/*******************************************************************************************/
/*   QWICS Server Java EE Web Application                                                  */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 23.10.2019                                */
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

import javax.annotation.Resource;
import javax.ejb.ActivationConfigProperty;
import javax.ejb.MessageDriven;
import javax.jms.ConnectionFactory;
import javax.jms.Message;
import javax.jms.MessageListener;
import javax.jms.Queue;
import javax.jms.Topic;
import javax.naming.InitialContext;
import javax.sql.DataSource;

import org.qwics.jdbc.msg.QueueManager;
import org.qwics.jdbc.msg.QueueWrapper;

/**
 * Message-Driven Bean implementation class for: QwicsMDB
 */
@MessageDriven(activationConfig = {
		@ActivationConfigProperty(propertyName = "destinationLookup", propertyValue = "java:/jms/queue/MyQueue"),
		@ActivationConfigProperty(propertyName = "destinationType", propertyValue = "javax.jms.Queue") }, messageListenerInterface = MessageListener.class)
public class QwicsMDB implements MessageListener, QueueManager {
	@Resource(mappedName = "java:/JmsXA")
	private ConnectionFactory cf;

	@Resource(mappedName="java:jboss/datasources/QwicsDS")
	DataSource datasource;

	private Connection con;
	private CallableStatement call;
	private ResultSet maps;
	private Message triggerMessage = null;

	public QwicsMDB() {
	}

	public QueueWrapper getQueue(String name, int type, int opts) {
		try {
			InitialContext ctx = new InitialContext();
			if (type == 1) {
				// Queue
				Queue queue = (Queue) ctx.lookup("java:/jms/queue/" + name);
				ctx.close();
				if ((triggerMessage != null) && triggerMessage.getJMSDestination().equals(queue)) {
					return new QwicsQueue(cf, queue, triggerMessage);
				} else {
					return new QwicsQueue(cf, queue);
				}
			}
			if (type == 8) {
				// Topic
				Topic topic = (Topic) ctx.lookup("java:/jms/topic/" + name);
				ctx.close();
				return new QwicsTopic(cf, topic);
			}
		} catch (Exception e) {
		}
		return null;
	}

	public void onMessage(Message message) {
		try {
			triggerMessage = message;
			con = datasource.getConnection();
			call = con.prepareCall("PROGRAM QPUBCBL");
			maps = call.executeQuery();
			maps.updateString("QNAME", "MyQueue");
			maps.updateString("ENVDATA", "MyStatQueue");
			maps.updateObject("QMGR", this);
			maps.next();
			try {
				String ac = maps.getString("ABCODE");
				throw new QwicsException("ABEND " + ac);
			} catch (Exception e) {
			}
		} catch (Exception e) {
			throw new QwicsException(e);
		}
	}

}
