/*******************************************************************************************/
/*   QWICS Server JNI shared library implementation                                        */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 01.12.2023                                  */
/*                                                                                         */
/*   Copyright (C) 2023 by Philipp Brune  Email: Philipp.Brune@hs-neu-ulm.de               */
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/un.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <errno.h>

#include <libcob.h>
#include "../shm/shmtpm.h"
#include "../env/envconf.h"
#include "../config.h"
#include "../cobexec.h"
#include "org_qwics_jni_QwicsTPMServerWrapper.h"


// Global shared memory area for all workers, as desclared in shmtpm.h
int shmId;
key_t shmKey;
void *shmPtr;

pthread_key_t execVarsKey;
pthread_key_t globVarsKey;


struct callbackFuncType {
    int (*callback)(char *loadmod, void *data);
    JNIEnv *env;
    jobject self;
};

struct memBufferDef {
    jbyteArray var;
    unsigned char *memBuffer;
    int isGlobal;
    int isMapped;
};

struct cobVarData {
    cob_field vars[50];
    int varNum;
    // Used only in globVars, execVars refer to globVars for their buffers
    struct memBufferDef memBuffers[50];
    int bufNum;
};


int execLoadModuleCallback(char *loadmod, void *data) {
    struct callbackFuncType *callbackFunc = (struct callbackFuncType *)data;
    JNIEnv *env = callbackFunc->env;

    jclass wrapperClass = (*env)->GetObjectClass(env, callbackFunc->self);
    jmethodID exec = (*env)->GetMethodID(env, wrapperClass, "execLoadModule", "(Ljava/lang/String;)V");
    (*env)->CallVoidMethod(env, callbackFunc->self, exec, (*env)->NewStringUTF(env,loadmod));
    return 0;
}


static void sig_handler(int signo) {
    printf("Stopping QWICS tpmserver process...\n");
    if (signo == SIGINT) {
        clearExec(1);
    }
}


unsigned char *getMemBuffer(jbyteArray var, JNIEnv *env, struct cobVarData *globVars, int isGlobal) {
printf("getMemBuffer %x %x\n",var,globVars);
    if (globVars == NULL) {
        return NULL;
    }

    int i,j = -1;
    for (i = 0; i < globVars->bufNum; i++) {
        // Remap globals
        if (globVars->memBuffers[i].isGlobal && !globVars->memBuffers[i].isMapped) {
            globVars->memBuffers[i].memBuffer = (unsigned char*)(*env)->GetByteArrayElements(env, globVars->memBuffers[i].var, 0); 
            globVars->memBuffers[i].isMapped = 1;
        }
        if ((*env)->IsSameObject(env,globVars->memBuffers[i].var,var)) {
            // Remember var if already found
            j = i;
        }
    }

    if (j < 0) {
        j = 0;
    }

    for (i = j; i < globVars->bufNum; i++) {
        if ((*env)->IsSameObject(env,globVars->memBuffers[i].var,var)) {
            if (!globVars->memBuffers[i].isMapped) {
                globVars->memBuffers[i].memBuffer = (unsigned char*)(*env)->GetByteArrayElements(env, globVars->memBuffers[i].var, 0); 
                globVars->memBuffers[i].isMapped = 1;
            }
printf("getMemBufferA %x %x %d %d\n",globVars->memBuffers[i].var,globVars->memBuffers[i].memBuffer,i,globVars->bufNum);
            return globVars->memBuffers[i].memBuffer;
        }
    }

    if ((i >= globVars->bufNum) && (globVars->bufNum < 50)) {
        globVars->memBuffers[globVars->bufNum].var = (jbyteArray)(*env)->NewGlobalRef(env,var);
        globVars->memBuffers[globVars->bufNum].memBuffer = (unsigned char*)(*env)->GetByteArrayElements(env, globVars->memBuffers[globVars->bufNum].var, 0); 
        globVars->memBuffers[globVars->bufNum].isMapped = 1;
        globVars->memBuffers[globVars->bufNum].isGlobal = isGlobal;
printf("getMemBufferB  %x %x %d %d\n",globVars->memBuffers[globVars->bufNum].var,globVars->memBuffers[i].memBuffer,i,globVars->bufNum);
        globVars->bufNum++;    
        return globVars->memBuffers[globVars->bufNum-1].memBuffer;
    }

    return NULL;
}


void releaseMemBuffers(JNIEnv *env, struct cobVarData *globVars) {
    if (globVars == NULL) {
        return;
    }

    int i;
    for (i = 0; i < globVars->bufNum; i++) {
        if (globVars->memBuffers[i].isMapped) {
            (*env)->ReleaseByteArrayElements(env, globVars->memBuffers[i].var, (jbyte*)globVars->memBuffers[i].memBuffer, 0);
            globVars->memBuffers[i].isMapped = 0;
        }
    }
}


JNIEXPORT void JNICALL Java_org_qwics_jni_QwicsTPMServerWrapper_execCallbackNative(JNIEnv *env, jobject self, 
                jstring cmd, jbyteArray var, jint pos, jint len, jint attr) {
    const char* cmdStr = (*env)->GetStringUTFChars(env, cmd, NULL);
    struct cobVarData *execVars = (struct cobVarData*)pthread_getspecific(execVarsKey);
    struct cobVarData *globVars = (struct cobVarData*)pthread_getspecific(globVarsKey);
printf("execCallbackNative %s %x %x %x %d\n",cmdStr,var,execVars,globVars,len);

    if (strstr(cmdStr,"TPMI:SET") != NULL) {
        if ((globVars != NULL) && (len >= 0)) {
            unsigned char* memBuffer = getMemBuffer(var,env,globVars,1);
            if ((globVars->varNum < 50) && (memBuffer != NULL)) {
                cob_field *f = &(globVars->vars[globVars->varNum]);
                globVars->varNum++;

                cob_field_attr *a = (cob_field_attr*)malloc(sizeof(cob_field_attr));
                a->type = attr & 0xFF; 
                a->flags = (attr >> 8) & 0xFF;
                a->digits = (attr >> 16) & 0xFF;
                a->scale = (attr >> 24) & 0xFF; 
                a->pic = NULL;

                f->data = &(memBuffer[pos]);
                f->attr = a;
                f->size = (int)len;

                execCallback((char*)cmdStr,f);
            }
        }
        return;
    }

    if (strstr(cmdStr,"TPMI:EXEC") != NULL) {
        if (execVars != NULL) {
            int i;
            for (i = 0; i < execVars->varNum; i++) {
                free((void*)execVars->vars[i].attr);
            }
            free(execVars);
            execVars = NULL;
        }

        execVars = (struct cobVarData*)malloc(sizeof(struct cobVarData));
        execVars->varNum = 0;
        pthread_setspecific(execVarsKey,execVars);
    }

    if (len < 0) {
        execCallback((char*)cmdStr,NULL);
    } else {
        if (execVars != NULL) {
            unsigned char* memBuffer = getMemBuffer(var,env,globVars,0);
            if ((execVars->varNum < 50) && (memBuffer != NULL)) {
                cob_field *f = &(execVars->vars[execVars->varNum]);
                execVars->varNum++;

                cob_field_attr *a = (cob_field_attr*)malloc(sizeof(cob_field_attr));
                a->type = attr & 0xFF; 
                a->flags = (attr >> 8) & 0xFF;
                a->digits = (attr >> 16) & 0xFF;
                a->scale = (attr >> 24) & 0xFF; 
                a->pic = NULL;

                f->data = &(memBuffer[pos]);
                f->attr = a;
                f->size = (int)len;

                execCallback((char*)cmdStr,f);
            }
        }
    }

    if (strstr(cmdStr,"TPMI:END-EXEC") != NULL) {
        int i;
        for (i = 0; i < execVars->varNum; i++) {
            printf("VAR %d %d %x %d %d\n",i,execVars->vars[i].size,execVars->vars[i].attr->type,execVars->vars[i].attr->digits,execVars->vars[i].attr->scale);
            int j;
            for (j = 0; j < execVars->vars[i].size; j++) {
                printf("%c",(char)execVars->vars[i].data[j]);
            }
            printf("\n\n");
        }

        releaseMemBuffers(env,globVars);

        if (execVars != NULL) {
            int i;
            for (i = 0; i < execVars->varNum; i++) {
                free((void*)execVars->vars[i].attr);
            }
            free(execVars);
        }
        pthread_setspecific(execVarsKey,NULL);
    }
    (*env)->ReleaseStringUTFChars(env, cmd, cmdStr);
}


JNIEXPORT jint JNICALL Java_org_qwics_jni_QwicsTPMServerWrapper_readByte(JNIEnv *env, jobject self, jlong fd) {
    int b = 0;
    if (read((int)fd,(void*)&b,1) < 0) {
        b = -1;
    }
    return b;
}


JNIEXPORT jint JNICALL Java_org_qwics_jni_QwicsTPMServerWrapper_writeByte(JNIEnv *env, jobject self, jlong fd, jbyte b) {
    return write((int)fd,(void*)&b,1);
}


JNIEXPORT jlongArray JNICALL Java_org_qwics_jni_QwicsTPMServerWrapper_init(JNIEnv *env, jobject self) {
    int sockets[2];
    jlong fds[2];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) < 0) {
        printf("Error opening internal socket pair\n");
        return NULL;
    }
    fds[0] = (jlong)sockets[0];
    fds[1] = (jlong)sockets[1];
    jlongArray fd = (*env)->NewLongArray(env,2);
    (*env)->SetLongArrayRegion(env,fd,0,2,fds);
    return fd;
}


JNIEXPORT void JNICALL Java_org_qwics_jni_QwicsTPMServerWrapper_clear(JNIEnv *env, jobject self, jlongArray fd) {
    jlong *fds = (*env)->GetLongArrayElements(env, fd, 0);
    close((int)fds[0]);
    close((int)fds[1]);
    (*env)->ReleaseLongArrayElements(env, fd, fds, 0);
}


JNIEXPORT jint JNICALL Java_org_qwics_jni_QwicsTPMServerWrapper_execInTransaction(JNIEnv *env, jobject self, jstring loadmod, jlong fd, jint setCommArea, jint parcnt) {
    struct callbackFuncType *callbackFunc = (struct callbackFuncType*)malloc(sizeof(struct callbackFuncType));
    callbackFunc->callback = &execLoadModuleCallback;
    callbackFunc->env = env;
    callbackFunc->self = (*env)->NewGlobalRef(env,self);
    int _fd = (int)fd;

    // Set signal handler for SIGINT (proper shutdown of server)
    struct sigaction a;
    a.sa_handler = sig_handler;
    a.sa_flags = 0;
    sigemptyset( &a.sa_mask );
    sigaction( SIGINT, &a, NULL );

    // Init shared memory area
    if ((shmPtr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0)) == (void *) -1) {
      fprintf(stderr, "Failed to attach shared memory segment.\n");
      return -1;
    }

    pthread_key_create(&execVarsKey, NULL);
    pthread_setspecific(execVarsKey,NULL);
    pthread_key_create(&globVarsKey, NULL);
    struct cobVarData *globVars = (struct cobVarData*)malloc(sizeof(struct cobVarData));
    globVars->varNum = 0;
    globVars->bufNum = 0;
    pthread_setspecific(globVarsKey,globVars);

    const char* name = (*env)->GetStringUTFChars(env, loadmod, NULL); 
    initExec(1);
    execCallbackInTransaction((char*)name,&_fd,(int)setCommArea,(int)parcnt,(void*)callbackFunc);
    clearExec(1);

    (*env)->DeleteGlobalRef(env,callbackFunc->self);
    free(callbackFunc);
    (*env)->ReleaseStringUTFChars(env, loadmod, name);
    releaseMemBuffers(env,globVars);

    int i;
    for (i = 0; i < globVars->bufNum; i++) {
        (*env)->DeleteGlobalRef(env,globVars->memBuffers[i].var);
    }

    for (i = 0; i < globVars->varNum; i++) {
        free((void*)globVars->vars[i].attr);
    }
    free(globVars);

    pthread_setspecific(globVarsKey,NULL);
    return 0;
}


JNIEXPORT void JNICALL Java_org_qwics_jni_QwicsTPMServerWrapper_execSqlNative(JNIEnv *env, jobject self, jstring sql, jlong fd, jint sendRes, jint sync) {
    int _fd = (int)fd;

    // Init shared memory area
    if ((shmPtr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0)) == (void *) -1) {
      fprintf(stderr, "Failed to attach shared memory segment.\n");
      return;
    }

    const char* sqlStr = (*env)->GetStringUTFChars(env, sql, NULL); 
    initExec(1);
    _execSql((char*)sqlStr, &_fd, sendRes, sync);
    clearExec(1);
    (*env)->ReleaseStringUTFChars(env, sql, sqlStr);
}
