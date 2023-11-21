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

import java.util.Arrays;

public class CobVarResolverImpl implements CobVarResolver {
    private static CobVarResolverImpl instance = null;
    protected byte[] memoryBuffer = null;
    protected int pos = 0;
    protected int len = 0;
    protected int attr = 0;

    protected CobVarResolverImpl() {
    }

    public static CobVarResolverImpl getInstance() {
        if (instance == null) {
            String cl = null;
            try {
                if ((cl = System.getProperty("org.qwics.cobvarresolver")) != null) {
                    Class clazz = Class.forName(cl);
                    instance = (CobVarResolverImpl)clazz.newInstance();
                } else {
                    instance = new CobVarResolverImpl();
                }
            } catch (Exception e) {
                e.printStackTrace();
                instance = new CobVarResolverImpl();
            }
        }
        return instance;
    }

    public static void setInstance(CobVarResolverImpl instance) {
        CobVarResolverImpl.instance = instance;
    }

    @Override
    public void setVar(Object var) {
        // This is a dummy impl. Here, the mapping var to native should be implemented.
    }

    @Override
    public byte[] getMemoryBuffer() {
        return memoryBuffer;
    }

    @Override
    public int getPos() {
        return pos;
    }

    @Override
    public int getLen() {
        return len;
    }

    @Override
    public int getAttr() {
        return attr;
    }

    @Override
    public String toString() {
        return "CobVarResolverImpl{" +
                "memoryBuffer=" + Arrays.toString(memoryBuffer) +
                ", pos=" + pos +
                ", len=" + len +
                ", attr=" + attr +
                '}';
    }

    protected void setFlags(int flags) {
        attr = attr | (flags << 8);
    }

    protected void setDigits(int digits) {
        attr = attr | (digits << 16);
    }

    protected void setScale(int scale) {
        attr = attr | (scale << 24);
    }
}
