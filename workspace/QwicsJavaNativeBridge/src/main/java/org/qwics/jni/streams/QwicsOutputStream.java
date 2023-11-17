/*
Qwics TPM integration via JNI (experimental)

Copyright (c) 2023 Philipp Brune    Email: Philipp.Brune@hs-neu-ulm.de

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

package org.qwics.jni.streams;

import org.qwics.jni.QwicsTPMServerWrapper;

import java.io.IOException;
import java.io.OutputStream;

public class QwicsOutputStream extends OutputStream {
    private QwicsTPMServerWrapper wrapper = null;
    private long fd = 0;
    private String lineBuf = "";
    private int parcnt = 0;
    private boolean writeThrough = false;

    public QwicsOutputStream(QwicsTPMServerWrapper wrapper, long fd) {
        this.wrapper = wrapper;
        this.fd = fd;
    }

    @Override
    public void write(int b) throws IOException {
        if (!writeThrough) {
            if (b == '\n' || b == '\r' || lineBuf.length() >= 80) {
                System.out.println(lineBuf);

                if (lineBuf.startsWith("sql")) {
                    String sql = lineBuf.substring(4);
                    wrapper.execSql(sql, 1, 0);
                }
                if (lineBuf.startsWith("PROGRAM")) {
                    wrapper.launchClass(lineBuf.substring(8).trim(), 0, parcnt);
                }
                if (lineBuf.startsWith("PROGRAM")) {
                    wrapper.launchClass(lineBuf.substring(8).trim(), 0, parcnt);
                }
                if (lineBuf.startsWith("CAPROG")) {
                    wrapper.launchClass(lineBuf.substring(7).trim(), 1, parcnt);
                }
                if (lineBuf.startsWith("parcnt")) {
                    try {
                        parcnt = Integer.parseInt(lineBuf.substring(7).trim());
                    } catch (NumberFormatException e) {
                        parcnt = 0;
                    }
                }
                if (lineBuf.startsWith("quit")) {
                    wrapper.execSql("COMMIT", 0, 0);
                }

                lineBuf = "";
                return;
            } else {
                lineBuf = lineBuf + (char) b;
            }
        } else {
            if (wrapper.writeByte(fd,(byte)b) < 0) {
                throw new IOException("Error writing byte to fd "+fd);
            };
        }
    }

    public void setWriteThrough(boolean writeThrough) {
        this.writeThrough = writeThrough;
    }
}
