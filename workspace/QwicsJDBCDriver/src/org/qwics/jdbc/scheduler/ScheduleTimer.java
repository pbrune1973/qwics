/*
Qwics JDBC Client for Java

Copyright (c) 2019,2020 Philipp Brune    Email: Philipp.Brune@hs-neu-ulm.de

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

package org.qwics.jdbc.scheduler;

import java.util.ArrayList;
import java.util.HashMap;

import org.qwics.jdbc.QwicsMapResultSet;

public class ScheduleTimer extends Thread {
	private static ScheduleTimer scheduleTimer = new ScheduleTimer();

	private ArrayList<TaScheduled> scheduledTa = new ArrayList<TaScheduled>();
	private boolean terminated = false;
	private TaStarter taStarter = new TaStarter();

	private ScheduleTimer() {
		super();
		this.start();
	}

	public static ScheduleTimer getScheduleTimer() {
		return scheduleTimer;
	}

	@Override
	public void run() {
		do {
			synchronized (scheduledTa) {
				long now = System.currentTimeMillis();

				while ((scheduledTa.size() > 0) && (scheduledTa.get(0).getStartAtMillis() <= now)) {
					if (scheduledTa.get(0).getData() != null) {
						HashMap<String, ArrayList<char[]>> tsQueues = QwicsMapResultSet.getTsQueues();
						synchronized (tsQueues) {
							String queue = scheduledTa.get(0).getReqId();
							ArrayList<char[]> q = tsQueues.get(queue);
							if (q == null) {
								q = new ArrayList<char[]>();
								tsQueues.put(queue, q);
								QwicsMapResultSet.getTsQueuesLastRead().put(queue, -1);
							}
							q.add(scheduledTa.get(0).getData());
						}
					}
					if (taStarter != null) {
						taStarter.start(scheduledTa.get(0).getTransId(),scheduledTa.get(0).getReqId());						
					}
					scheduledTa.remove(0);
				}
			}
			if (scheduledTa.size() > 0) {
				long waitFor = scheduledTa.get(0).getStartAtMillis() - System.currentTimeMillis();
				try {
					sleep(waitFor);
				} catch (InterruptedException e) {
				}
			} else {
				try {
					sleep(12 * 3600000L);
				} catch (InterruptedException e) {
				}
			}
		} while (!terminated);
	}

	public void terminate() {
		terminated = true;
		this.interrupt();
	}

	public void scheduleTa(TaScheduled ta) {
		synchronized (scheduledTa) {
			long prev = -1;
			if (scheduledTa.size() > 0) {
				prev = scheduledTa.get(0).getStartAtMillis();
			}
			scheduledTa.add(ta);
			scheduledTa.sort(null);
			if (scheduledTa.get(0).getStartAtMillis() != prev) {
				this.interrupt();
			}
		}
	}

	public int cancelTa(TaScheduled ta) {
		synchronized (scheduledTa) {
			for (int i = 0; i < scheduledTa.size(); i++) {
				if ((ta.getReqId().equals(scheduledTa.get(i).getReqId()))
						&& (ta.getTransId().equals(scheduledTa.get(i).getTransId()))) {
					scheduledTa.remove(i);
					this.interrupt();
					return 0;
				}
			}
		}
		return -1;
	}

	public void setTaStarter(TaStarter taStarter) {
		this.taStarter = taStarter;
	}

}
