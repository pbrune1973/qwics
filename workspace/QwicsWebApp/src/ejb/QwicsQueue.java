/*******************************************************************************************/
/*   QWICS Server Java EE Web Application                                                  */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 21.11.2018                                  */
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

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.HashMap;

import javax.jms.BytesMessage;
import javax.jms.Connection;
import javax.jms.ConnectionFactory;
import javax.jms.Message;
import javax.jms.MessageConsumer;
import javax.jms.MessageProducer;
import javax.jms.Queue;
import javax.jms.Session;
import javax.naming.InitialContext;

import org.qwics.jdbc.msg.QueueWrapper;

public class QwicsQueue implements QueueWrapper {
	private Queue queue = null;
	private Connection con = null;
	private Session sess = null;
	private Message firstMessage = null;

	
	public QwicsQueue(ConnectionFactory cf, Queue queue, Message firstMessage) throws Exception {
		this.queue = queue;
		this.firstMessage = firstMessage;
		con = cf.createConnection();
		sess = con.createSession(false, Session.SESSION_TRANSACTED);		
        con.start();
	}
	
	
	public QwicsQueue(ConnectionFactory cf, Queue queue) throws Exception {
		this(cf,queue,null);
	}
	
	
	@Override
	public void close() throws Exception {
		sess.close();
		con.close();
	}

	
	@Override
	public void get(HashMap<String, Object> msgHeader, char[] msgBody) throws Exception {	
		BytesMessage msg = null;
		if (this.firstMessage != null) {
			msg = (BytesMessage)this.firstMessage;
			this.firstMessage = null;
		} else {
			MessageConsumer receiver = sess.createConsumer(queue);
			msg  = (BytesMessage)receiver.receive();					
			receiver.close();
		}
		try {
			msgHeader.put("MSGTYPE", Integer.parseInt(msg.getJMSType()));
		} catch (Exception e) {			
		}
		msgHeader.put("EXPIRY", (int)msg.getJMSExpiration());
		msgHeader.put("PRIORITY", (int)msg.getJMSPriority());
		msgHeader.put("MSGID", msg.getJMSMessageID());
		msgHeader.put("CORRELID", msg.getJMSCorrelationID());
		try {
			msgHeader.put("REPLYTOQ", ((Queue)msg.getJMSReplyTo()).getQueueName());
		} catch (Exception e) {
		}
		try {
			SimpleDateFormat fmt = new SimpleDateFormat("yyyyMMdd");
			msgHeader.put("PUTDATE", fmt.format(new Date(msg.getJMSTimestamp())));
			fmt = new SimpleDateFormat("HHmmssSS");
			msgHeader.put("PUTTIME", fmt.format(new Date(msg.getJMSTimestamp())));
		} catch (Exception e) {
		}

		int i = 0;
		for (i = 0; i < msgBody.length; i++) {
			msgBody[i] = (char)msg.readByte();
		}
	}

	
	@Override
	public void put(HashMap<String, Object> msgHeader, char[] msgBody) throws Exception {
		MessageProducer sender = sess.createProducer(queue);
		BytesMessage msg = sess.createBytesMessage();
		try {
			msg.setJMSType(msgHeader.get("MSGTYPE").toString());
		} catch (Exception e) {			
		}
		msg.setJMSExpiration((Integer)msgHeader.get("EXPIRY"));
		msg.setJMSPriority((Integer)msgHeader.get("PRIORITY"));
		//msg.setJMSMessageID((String)msgHeader.get("MSGID"));
		msg.setJMSCorrelationID((String)msgHeader.get("CORRELID"));
		try {
			InitialContext ctx = new InitialContext();
    			Queue dest = (Queue)ctx.lookup("java:/jms/queue/"+msgHeader.get("REPLYTOQ"));
    			ctx.close();    		    			
			msg.setJMSReplyTo(dest);
		} catch (Exception e) {
		}
		try {
			SimpleDateFormat fmt = new SimpleDateFormat("yyyyMMdd HHmmssSS");
			msg.setJMSTimestamp(fmt.parse(msgHeader.get("PUTDATE")+" "+msgHeader.get("PUTTIME")).getTime());
		} catch (Exception e) {
		}
		
		int i = 0;
		for (i = 0; i < msgBody.length; i++) {
			msg.writeByte((byte)msgBody[i]);
		}
		
		sender.send(msg);
		sender.close();
	}

}
