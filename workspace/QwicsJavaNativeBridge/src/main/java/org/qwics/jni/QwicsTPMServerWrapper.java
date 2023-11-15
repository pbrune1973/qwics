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

package org.qwics.jni;

import org.qwics.jni.streams.QwicsInputStream;
import org.qwics.jni.streams.QwicsOutputStream;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.reflect.Method;
import java.net.Socket;
import java.util.HashMap;

public class QwicsTPMServerWrapper extends Socket {
    private static ThreadLocal<QwicsTPMServerWrapper> _instance = new ThreadLocal<QwicsTPMServerWrapper>();

    static {
        System.loadLibrary("libtpmserver");
    }

    private HashMap<String,String> loadModClasses = new HashMap<String,String>();
    private QwicsInputStream inputStream = null;
    private QwicsOutputStream outputStream = null;

    private QwicsTPMServerWrapper() {
        inputStream = new QwicsInputStream(this,0);
        outputStream = new QwicsOutputStream(this,0);
    }

    public static QwicsTPMServerWrapper getWrapper() {
        return new QwicsTPMServerWrapper();
    }

    public static QwicsTPMServerWrapper getInstance() {
        return _instance.get();
    }

    public void setAsInstance() {
        _instance.set(this);
    }

    public void launchClass(String name) {
        try {
            final QwicsTPMServerWrapper _this = this;
            final Class cl = Class.forName(loadModClasses.get(name));
            Method main = cl.getMethod("main",String[].class);
            if (main != null) {
                Thread exec = new Thread() {
                    @Override
                    public void run() {
                        try {
                            _this.setAsInstance();
                            main.invoke(null, new String[0]);
                        } catch (Exception e) {
                            e.printStackTrace();
                        }
                    }
                };
                exec.start();
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public void defineLoadModClass(String loadMod, String className) {
        loadModClasses.put(loadMod,className);
    }

    @Override
    public InputStream getInputStream() {
        return inputStream;
    }

    @Override
    public OutputStream getOutputStream() {
        return outputStream;
    }

    @Override
    public synchronized void close() throws IOException {
        super.close();
    }

    public void execCallback(String cmd, Object var) {
    }

    public native void execCallbackNative(String cmd, Object var);
    public native int readByte(long fd);
    public native int writeByte(long fd, byte b);
}
