/*******************************************************************************************/
/*   QWICS Server JNI shared library implementation                                        */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 13.12.2023                                  */
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
    int (*abendHandler)(void *data);
    JNIEnv *env;
    jobject self;
    int mode;
    int condCode;
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
    jmethodID exec = (*env)->GetMethodID(env, wrapperClass, "execLoadModule", "(Ljava/lang/String;I)V");
    jstring loadmodStr = (*env)->NewStringUTF(env,loadmod);
    (*env)->CallVoidMethod(env, callbackFunc->self, exec, loadmodStr, (jint)callbackFunc->mode);
    (*env)->DeleteLocalRef(env,loadmodStr);
    return 0;
}


int abendCallback(void *data) {
    struct callbackFuncType *callbackFunc = (struct callbackFuncType *)data;
    if (callbackFunc != NULL) {
        JNIEnv *env = callbackFunc->env;

        if (callbackFunc->mode == 17) {
            callbackFunc->mode = 1;
            // Hard abend
            jclass exClass = (*env)->FindClass(env,"org/qwics/jni/Abend");
            if (exClass == NULL ) {
                return (*env)->ThrowNew(env,exClass,NULL);
            }
            return 0;
        }

        jclass wrapperClass = (*env)->GetObjectClass(env, callbackFunc->self);
        jmethodID exec = (*env)->GetMethodID(env, wrapperClass, "abend", "(I;I)V");
        (*env)->CallVoidMethod(env, callbackFunc->self, exec, 
                (jint)callbackFunc->mode,(jint)callbackFunc->condCode);
    }
    return 0;
}


static void sig_handler(int signo) {
    printf("Stopping QWICS tpmserver process...\n");
    if (signo == SIGINT) {
        clearExec(1);
    }
}


unsigned char *getMemBuffer(jbyteArray var, JNIEnv *env, struct cobVarData *globVars, int isGlobal) {
    if (globVars == NULL) {
        return NULL;
    }

    int i = 0,j = -1;
    for (i = 0; i < globVars->bufNum; i++) {
        if ((*env)->IsSameObject(env,globVars->memBuffers[i].var,var)) {
            if (!globVars->memBuffers[i].isMapped) {
                globVars->memBuffers[i].memBuffer = (unsigned char*)(*env)->GetByteArrayElements(env, globVars->memBuffers[i].var, 0); 
                globVars->memBuffers[i].isMapped = 1;
            }
            // Remember var if already found
            j = i;
        } else {
            // Remap globals
            if (globVars->memBuffers[i].isGlobal && !globVars->memBuffers[i].isMapped) {
                globVars->memBuffers[i].memBuffer = (unsigned char*)(*env)->GetByteArrayElements(env, globVars->memBuffers[i].var, 0); 
                globVars->memBuffers[i].isMapped = 1;
            }
        }
    }

    if (j >= 0) {
        return globVars->memBuffers[j].memBuffer;
    }

    if ((i >= globVars->bufNum) && (globVars->bufNum < 50)) {
        globVars->memBuffers[globVars->bufNum].var = (jbyteArray)(*env)->NewGlobalRef(env,var);
        globVars->memBuffers[globVars->bufNum].memBuffer = (unsigned char*)(*env)->GetByteArrayElements(env, globVars->memBuffers[globVars->bufNum].var, 0); 
        globVars->memBuffers[globVars->bufNum].isMapped = 1;
        globVars->memBuffers[globVars->bufNum].isGlobal = isGlobal;
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
            if (globVars->memBuffers[i].isGlobal) {
                // Only commit changes for global vars, do not release buffer
                (*env)->ReleaseByteArrayElements(env, globVars->memBuffers[i].var, (jbyte*)globVars->memBuffers[i].memBuffer, JNI_COMMIT);
            } else {
                (*env)->ReleaseByteArrayElements(env, globVars->memBuffers[i].var, (jbyte*)globVars->memBuffers[i].memBuffer, 0);
                globVars->memBuffers[i].isMapped = 0;
            }
        }
    }
}


void releaseAllMemBuffers(JNIEnv *env, struct cobVarData *globVars) {
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


void adjustByteOrder(int i, struct cobVarData *vars) {
    unsigned char hbuf[8]; // Max. 64 Bit
    unsigned char *buf = vars->vars[i].data;
    int len = vars->vars[i].size,j;

//printf("adjustByteOrder %d %d %x\n",i,len,vars->vars[i].attr->type);
    if (vars->vars[i].attr->type == 0x11) {
        memcpy(hbuf,buf,len);
        for (j = 0; j < len; j++) {
            buf[j] = hbuf[len-1-j];
        }
    }
}


JNIEXPORT void JNICALL Java_org_qwics_jni_QwicsTPMServerWrapper_execCallbackNative(JNIEnv *env, jobject self, 
                jstring cmd, jbyteArray var, jint pos, jint len, jint attr, jint varMode) {
    const char* cmdStr = (*env)->GetStringUTFChars(env, cmd, NULL);
    struct cobVarData *execVars = (struct cobVarData*)pthread_getspecific(execVarsKey);
    struct cobVarData *globVars = (struct cobVarData*)pthread_getspecific(globVarsKey);
    //printf("execCallbackNative %s %x %x %x %d %d %d\n",cmdStr,var,execVars,globVars,pos,len,varMode);

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

                execCallback((char*)&cmdStr[5],f); 
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
        execVars->bufNum = 0;
        pthread_setspecific(execVarsKey,execVars);
    }

    if (len < 0) {
        execCallback((char*)&cmdStr[5],NULL);
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
                if (varMode == 0) {
                    f->data = &(memBuffer[pos]);
                } else {
                    execVars->memBuffers[execVars->bufNum].var = (jbyteArray)(*env)->NewGlobalRef(env,var);
                    execVars->memBuffers[execVars->bufNum].memBuffer = (unsigned char*)malloc((int)len);
                    (*env)->GetByteArrayRegion(env, var, (jsize)pos, (jsize)len, (jbyte*)execVars->memBuffers[execVars->bufNum].memBuffer);
                    execVars->memBuffers[execVars->bufNum].isMapped = 1;
                    execVars->memBuffers[execVars->bufNum].isGlobal = 0;
                    f->data = execVars->memBuffers[execVars->bufNum].memBuffer;
                    execVars->bufNum++;
                }
                f->attr = a;
                f->size = (int)len;

                execCallback((char*)&cmdStr[5],f);
            }
        }
    }

    if (strstr(cmdStr,"TPMI:END-EXEC") != NULL) {
        int i;
        for (i = 0; i < execVars->varNum; i++) {
            adjustByteOrder(i,execVars);
            printf("VAR %d %d %x %d %d\n",i,execVars->vars[i].size,execVars->vars[i].attr->type,execVars->vars[i].attr->digits,execVars->vars[i].attr->scale);
            int j;
            for (j = 0; j < execVars->vars[i].size; j++) {
                printf("%x ",execVars->vars[i].data[j]);
            }
            printf("\n\n");
        }

        releaseMemBuffers(env,globVars);

        if (execVars != NULL) {
            for (i = 0; i < execVars->varNum; i++) {
                free((void*)execVars->vars[i].attr);
            }
            for (i = 0; i < execVars->bufNum; i++) {
                if (execVars->memBuffers[i].memBuffer != NULL) {
                    //Does not work without len, pos!! (*env)->SetByteArrayRegion(env, execVars->memBuffers[i].var, (jsize)pos, (jsize)len, (jbyte*)execVars->memBuffers[i].memBuffer);
                    free(execVars->memBuffers[i].memBuffer);
                }
                (*env)->DeleteGlobalRef(env,execVars->memBuffers[i].var);
            }
            free(execVars);
        }
        pthread_setspecific(execVarsKey,NULL);
    }
    (*env)->ReleaseStringUTFChars(env, cmd, cmdStr);
}


JNIEXPORT jint JNICALL Java_org_qwics_jni_QwicsTPMServerWrapper_readByte(JNIEnv *env, jobject self, jlong fd, jint mode) {
    int b = 0;

    int n = recv((int)fd,(void*)&b,1,MSG_DONTWAIT | MSG_PEEK); 
    if (mode == 0) {
        // Blocking mode
        if (read((int)fd,(void*)&b,1) < 0) {
            b = -1;
        }
    } else
    if (mode == 1) {
        // Peek mode
        if (n <= 0) {
            b = -2;
        } else {
            b = n;
        }
    } else 
    if (mode == 2) {
        // Non-blocking mode
        if (n > 0) {
            if (read((int)fd,(void*)&b,1) < 0) {
                b = -1;
            }
        } else {
            b = -1;
        }
    }
    return b;
}


JNIEXPORT jint JNICALL Java_org_qwics_jni_QwicsTPMServerWrapper_writeByte(JNIEnv *env, jobject self, jlong fd, jbyte b) {
    return write((int)fd,(void*)&b,1);
}


JNIEXPORT void JNICALL Java_org_qwics_jni_QwicsTPMServerWrapper_initGlobal(JNIEnv *env, jclass clazz) {
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
      return;
    }

    pthread_key_create(&execVarsKey, NULL);
    pthread_setspecific(execVarsKey,NULL);
    pthread_key_create(&globVarsKey, NULL);
    pthread_setspecific(globVarsKey,NULL);

    initExec(1);
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


JNIEXPORT void JNICALL Java_org_qwics_jni_QwicsTPMServerWrapper_clearGlobal(JNIEnv *env, jclass clazz) {
    clearExec(1);
}    


JNIEXPORT void JNICALL Java_org_qwics_jni_QwicsTPMServerWrapper_clear(JNIEnv *env, jobject self, jlongArray fd) {
    jlong *fds = (*env)->GetLongArrayElements(env, fd, 0);
    close((int)fds[0]);
    close((int)fds[1]);
    (*env)->ReleaseLongArrayElements(env, fd, fds, 0);
}


JNIEXPORT jint JNICALL Java_org_qwics_jni_QwicsTPMServerWrapper_execInTransaction(JNIEnv *env, jobject self, jstring loadmod, jlong fd, jint setCommArea, jint parcnt) {
    jstring loadmodGlob = (*env)->NewGlobalRef(env,loadmod);
    struct callbackFuncType *callbackFunc = (struct callbackFuncType*)malloc(sizeof(struct callbackFuncType));
    callbackFunc->callback = &execLoadModuleCallback;
    callbackFunc->abendHandler = &abendCallback;
    callbackFunc->env = env;
    callbackFunc->self = (*env)->NewGlobalRef(env,self);
    callbackFunc->mode = 1;
    int _fd = (int)fd;

    pthread_setspecific(execVarsKey,NULL);
    struct cobVarData *globVars = (struct cobVarData*)malloc(sizeof(struct cobVarData));
    globVars->varNum = 0;
    globVars->bufNum = 0;
    pthread_setspecific(globVarsKey,globVars);

    const char* name = (*env)->GetStringUTFChars(env, loadmodGlob, NULL); 
    execCallbackInTransaction((char*)name,&_fd,(int)setCommArea,(int)parcnt,(void*)callbackFunc);

    (*env)->DeleteGlobalRef(env,callbackFunc->self);
    free(callbackFunc);
    (*env)->ReleaseStringUTFChars(env, loadmodGlob, name);

    int i;
    for (i = 0; i < globVars->varNum; i++) {
        adjustByteOrder(i,globVars);
    }

    releaseAllMemBuffers(env,globVars);

    for (i = 0; i < globVars->bufNum; i++) {
        (*env)->DeleteGlobalRef(env,globVars->memBuffers[i].var);
    }

    for (i = 0; i < globVars->varNum; i++) {
        free((void*)globVars->vars[i].attr);
    }
    free(globVars);
    pthread_setspecific(globVarsKey,NULL);
    (*env)->DeleteGlobalRef(env,loadmodGlob);
    return 0;
}


JNIEXPORT void JNICALL Java_org_qwics_jni_QwicsTPMServerWrapper_execSqlNative(JNIEnv *env, jobject self, jstring sql, jlong fd, jint sendRes, jint sync) {
    int _fd = (int)fd;
    const char* sqlStr = (*env)->GetStringUTFChars(env, sql, NULL); 
    _execSql((char*)sqlStr, &_fd, sendRes, sync);
    (*env)->ReleaseStringUTFChars(env, sql, sqlStr);
}
