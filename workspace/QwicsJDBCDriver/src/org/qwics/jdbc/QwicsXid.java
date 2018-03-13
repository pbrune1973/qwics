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

import javax.transaction.xa.Xid;

public class QwicsXid implements Xid {
	private byte[] globalTransactionId;
	private byte[] branchQualifier;
	private int formatId;

	public QwicsXid(String xidStr) {
		String parts[] = xidStr.split("\\.");
		globalTransactionId = new byte[parts[0].length() / 2];
		for (int i = 0; i < parts[0].length(); i = i + 2) {
			int j = i / 2;
			if (parts[0].charAt(i) < 65) {
				globalTransactionId[j] = (byte) (parts[0].charAt(i) - 48);
			} else {
				globalTransactionId[j] = (byte) (parts[0].charAt(i) - 65 + 10);
			}
			globalTransactionId[j] = (byte) (globalTransactionId[j] << 4);
			if (parts[0].charAt(i + 1) < 65) {
				globalTransactionId[j] = (byte) (globalTransactionId[j] | (byte)(parts[0]
						.charAt(i + 1) - 48));
			} else {
				globalTransactionId[j] = (byte) (globalTransactionId[j] | (byte)(parts[0]
						.charAt(i + 1) - 65 + 10));
			}
		}
		branchQualifier = new byte[parts[1].length() / 2];
		for (int i = 0; i < parts[1].length(); i = i + 2) {
			int j = i / 2;
			if (parts[1].charAt(i) < 65) {
				branchQualifier[j] = (byte) (parts[1].charAt(i) - 48);
			} else {
				branchQualifier[j] = (byte) (parts[1].charAt(i) - 65 + 10);
			}
			branchQualifier[j] = (byte) (branchQualifier[j] << 4);
			if (parts[1].charAt(i + 1) < 65) {
				branchQualifier[j] = (byte) (branchQualifier[j] | (byte)(parts[1]
						.charAt(i + 1) - 48));
			} else {
				branchQualifier[j] = (byte) (branchQualifier[j] | (byte)(parts[1]
						.charAt(i + 1) - 65 + 10));
			}
		}
		try {
			formatId = Integer.parseInt(parts[2]);
		} catch (Exception e) {
			e.printStackTrace();
		}
	}

	@Override
	public byte[] getBranchQualifier() {
		return this.branchQualifier;
	}

	@Override
	public int getFormatId() {
		return this.getFormatId();
	}

	@Override
	public byte[] getGlobalTransactionId() {
		return this.getGlobalTransactionId();
	}

}
