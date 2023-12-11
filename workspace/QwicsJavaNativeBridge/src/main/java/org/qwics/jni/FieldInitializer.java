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

import java.lang.reflect.AccessibleObject;
import java.lang.reflect.Method;
import java.lang.reflect.Field;
import java.util.ArrayList;


public class FieldInitializer {
    private ArrayList<Method> synchronizers = new ArrayList<Method>();
    private ArrayList<Field> valids = new ArrayList<Field>();
    private ArrayList<Field> groupFields = new ArrayList<Field>();
    private ArrayList<FieldInitializer> groupFieldInitializers = new ArrayList<FieldInitializer>();

    public FieldInitializer() {
    }

    public void addSynchronizer(Method m) {
        synchronizers.add(m);
    }

    public void addValid(Field f) {
        valids.add(f);
    }

    public void addGroupField(Field f, FieldInitializer initializer) {
        groupFields.add(f);
        groupFieldInitializers.add(initializer);
    }

    public void apply(Object obj, int mode) throws Exception {
        if (mode == 0) {
            for (Method m : synchronizers) {
                m.invoke(obj);
            }
        }
        if (mode == 1) {
            for (Field f : valids) {
                f.setBoolean(obj,false);
            }
        }
        int i = 0;
        for (Field f : groupFields) {
            Object groupField = f.get(obj);
            groupFieldInitializers.get(i).apply(groupField,mode);
            i++;
        }
    }
}
