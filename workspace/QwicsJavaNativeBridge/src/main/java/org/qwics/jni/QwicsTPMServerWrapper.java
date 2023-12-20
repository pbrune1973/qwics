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
import java.lang.reflect.Field;
import java.net.Socket;
import java.util.Arrays;
import java.util.HashMap;

public class QwicsTPMServerWrapper extends Socket {
    private static ThreadLocal<QwicsTPMServerWrapper> _instance = new ThreadLocal<QwicsTPMServerWrapper>();

    static {
        try {
            System.loadLibrary("tpmserver");
            initGlobal();
            Runtime.getRuntime().addShutdownHook(new Thread() {
                @Override
                public void run() {
                    QwicsTPMServerWrapper.clearGlobal();
                }
            });
        } catch (Throwable e) {
            e.printStackTrace();
        }
    }

    public static interface ExecutorAction {
            public void perform(QwicsTPMServerWrapper self, String cmd, long fd, int a, int b);
    }

    public static class Executor extends Thread {
        private boolean stop = false;
        private ExecutorAction action;
        private QwicsTPMServerWrapper self;
        private String cmd;
        private long fd;
        private int a;
        private int b;

        public Executor() {
        }

        public synchronized void exec(ExecutorAction action, QwicsTPMServerWrapper self,
                                      String cmd, long fd, int a, int b) {
            this.action = action;
            this.self = self;
            this.cmd = cmd;
            this.fd = fd;
            this.a = a;
            this.b = b;
            this.notify();
        }

        @Override
        public void run() {
            synchronized (this) {
                while (!stop) {
                    try {
                        this.wait();
                    } catch (InterruptedException e) {
                        System.out.println("Executor started");
                    }
                    if (!stop) {
                        action.perform(self,cmd,fd,a,b);
                    }
                }
            }
        }

        public synchronized void cancel() {
            this.stop = true;
            this.notify();
        }
    }

    private Executor executor = new Executor();
    private static HashMap<String,Class> loadModClasses = new HashMap<String,Class>();
    private static HashMap<String,FieldInitializer> loadModInitializers = new HashMap<String,FieldInitializer>();
    private QwicsInputStream inputStream = null;
    private QwicsOutputStream outputStream = null;
    private long fd[] = null;
    private boolean afterQwicslen = false;
    private HashMap<Integer,String> condHandler = new HashMap<Integer,String>();
    private boolean isOpen = false;

    private QwicsTPMServerWrapper() {
        fd = init();
        inputStream = new QwicsInputStream(this,fd[0]);
        outputStream = new QwicsOutputStream(this,fd[0]);
        executor.start();
        isOpen = true;
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

    public void execLoadModule(String loadmod, int mode) {
        this.execLoadModule(loadmod,mode,null);
    }

    public void execLoadModule(String loadmod, int mode, Object... args) {
        try {
            Class cl = null;
            synchronized (loadModClasses) {
                cl = loadModClasses.get(loadmod);
            }
            CobVarResolverImpl resolver = CobVarResolverImpl.getInstance();
            resolver.cobmain(cl.getCanonicalName(),args);
        } catch (Abend a) {
        } catch (Throwable e) {
            e.printStackTrace();
        } finally {
            if (mode == 1) {
                outputStream.setWriteThrough(false);
            }
        }
    }

    public void launchClass(String name, int setCommArea, int parCount) {
        try {
            final QwicsTPMServerWrapper _this = this;
            if (!loadModClasses.containsKey(name)) {
                String lmClass = null;
                if ((lmClass = System.getProperty(name)) != null) {
                    defineLoadModClass(name,Class.forName(lmClass));
                }
            }
            System.out.println("launchClass "+loadModClasses.get(name).getCanonicalName());

            outputStream.setWriteThrough(true);
            executor.exec((self,cmd,fd,a,b) -> {
                try {
                    final CobVarResolverImpl _resolver = CobVarResolverImpl.getInstance();
                    _resolver.prepareClassloader();
                    for (Class cl : loadModClasses.values()) {
                        _resolver.registerModule(cl);
                    }
                    self.condHandler.clear();
                    self.setAsInstance();
                    self.execInTransaction(cmd,fd,a,b);
                } catch (Exception e) {
                    //e.printStackTrace();
                }
            },_this,name,this.fd[1],setCommArea,parCount);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private static FieldInitializer createInitializers(Class clazz) throws Exception {
        final CobVarResolverImpl resolver = CobVarResolverImpl.getInstance();
        FieldInitializer initializer = new FieldInitializer();

        Method methods[] = clazz.getDeclaredMethods();
        for (Method m : methods) {
            if (resolver.isInitializer(m.getName())) {
                m.setAccessible(true);
                initializer.addSynchronizer(m);
            }
        }

        Field fields[] = clazz.getDeclaredFields();
        for (Field f: fields) {
            if (resolver.isInitializer(f.getName())) {
                f.setAccessible(true);
                initializer.addValid(f);
            }

            Class fc = f.getDeclaringClass();
            if (fc != null && fc.getCanonicalName().contains("data.")) {
                f.setAccessible(true);
                initializer.addGroupField(f,createInitializers(fc));
            }
        }

        return initializer;
    }

    public static void defineLoadModClass(String loadMod, Class clazz) {
        synchronized (loadModClasses) {
            loadModClasses.put(loadMod, clazz);
        }
        try {
            FieldInitializer initializer = createInitializers(clazz);
            synchronized(loadModInitializers) {
                loadModInitializers.put(clazz.getCanonicalName(), initializer);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
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
        executor.cancel();
        clear(fd);
        isOpen = false;
    }

    @Override
    public boolean isConnected() {
        return isOpen;
    }

    @Override
    public boolean isClosed() {
        return !isOpen;
    }

    public void execCallback(String cmd, Object var) {
        int pos = 0, len = -1, attr = 0, varMode = 0;
        byte[] varBuf = null;
        CobVarResolver varResolver = CobVarResolverImpl.getInstance();

        if (cmd.endsWith("EXEC") || afterQwicslen) {
            if (afterQwicslen) {
                varMode = 1;
            }
            afterQwicslen = false;
            synchronized(loadModInitializers) {
                varResolver.runInitializers(loadModInitializers,0);
            }
        }

        if (cmd.contains("LENGTH")) {
            afterQwicslen = true;
        }

        if (var != null) {
            varResolver.setVar(var);
            varBuf = varResolver.getMemoryBuffer();
            pos = varResolver.getPos();
            len = varResolver.getLen();
            attr = varResolver.getAttr();
        }

        execCallbackNative(cmd,varBuf,pos,len,attr,varMode);
        if (cmd.contains("END-EXEC")) {
            synchronized(loadModInitializers) {
                varResolver.runInitializers(loadModInitializers,1);
            }
        }
    }

    public void doCall(Object loadmod, Object commArea, Object... params) throws Throwable {
        int pos = 0, len = -1, attr = 0;
        byte[] varBuf = null;
        String name = null;
        CobVarResolver varResolver = CobVarResolverImpl.getInstance();

        if (loadmod instanceof String) {
            name = (String)loadmod;
        } else {
            varResolver.setVar(loadmod);
            varBuf = varResolver.getMemoryBuffer();
            pos = varResolver.getPos();
            len = varResolver.getLen();
            name = new String(Arrays.copyOfRange(varBuf,pos,pos+len));
        }
System.out.println("CALL "+name);

        if (doCallNative("CALL:"+name,null,0,-1,0,0) > 0) {
            varResolver.setVar(commArea);
            varBuf = varResolver.getMemoryBuffer();
            pos = varResolver.getPos();
            len = varResolver.getLen();
            attr = varResolver.getAttr();
            doCallNative("COMMAREA",varBuf,pos,len,attr,0);

            for (Object p : params) {
                varResolver.setVar(p);
                varBuf = varResolver.getMemoryBuffer();
                pos = varResolver.getPos();
                len = varResolver.getLen();
                attr = varResolver.getAttr();
                doCallNative("PARAM",varBuf,pos,len,attr,0);
            }

            doCallNative("END-CALL",null,0,-1,0,0);
        } else {
            // Direct in-Java call
            System.out.println("CALL Java "+name);
            execLoadModule(name,0,commArea,params);
        }
    }

    public void execSql(String sql, int sendRes, int sync) {
        System.out.println("execSql "+sql);
        try {
            final QwicsTPMServerWrapper _this = this;
            executor.exec((self,cmd,fd,a,b) -> {
                try {
                    self.setAsInstance();
                    self.execSqlNative(cmd,fd,a,b);
                } catch (Exception e) {
                    //e.printStackTrace();
                }
            },_this,sql,_this.fd[1], sendRes, sync);
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public void abend(int mode, int condCode) throws Throwable {
        CobVarResolver resolver = CobVarResolverImpl.getInstance();

        if (condHandler.containsKey(condCode)) {
            resolver.perform(condHandler.get(condCode));
            return;
        }

        resolver.perform("ABNDHNDL");
    }

    public void setConditionHandler(int condCode, String label) {
        condHandler.put(condCode,label);
    }

    public native void execCallbackNative(String cmd, byte[] var, int pos, int len, int attr, int varMode);
    public native int doCallNative(String cmd, byte[] var, int pos, int len, int attr, int varMode);
    public native int readByte(long fd, int mode);
    public native int writeByte(long fd, byte b);
    public static native void initGlobal();
    public native long[] init();
    public static native void clearGlobal();
    public native void clear(long fd[]);
    public native int execInTransaction(String loadmod, long fd, int setCommArea, int parCount);
    public native void execSqlNative(String sql, long fd, int sendRes, int sync);
}
