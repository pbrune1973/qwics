/*******************************************************************************************/
/*   QWICS Server JNI shared library implementation                                        */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 17.11.2023                                  */
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

struct callbackFuncType {
    int (*callback)(char *loadmod, void *data);
    JNIEnv *env;
    jobject self;
};


int execLoadModuleCallback(char *loadmod, void *data) {
    struct callbackFuncType *callbackFunc = (struct callbackFuncType *)data;
    JNIEnv *env = callbackFunc->env;

    jclass wrapperClass = (*env)->GetObjectClass(env, callbackFunc->self);
    jmethodID exec = (*env)->GetMethodID(env, wrapperClass, "execLoadModule", "(Ljava/lang/String;)V");
    (*env)->CallVoidMethod(env, callbackFunc->self, exec, (*env)->NewStringUTF(env,loadmod));
}


static void sig_handler(int signo) {
    printf("Stopping QWICS tpmserver process...\n");
    if (signo == SIGINT) {
        clearExec(1);
    }
}


JNIEXPORT void JNICALL Java_org_qwics_jni_QwicsTPMServerWrapper_execCallbackNative(JNIEnv *env, jobject self, jstring cmd, jobject var) {
    const char* cmdStr = (*env)->GetStringUTFChars(env, cmd, NULL);
    cob_field cobvar;
    execCallback((char*)cmdStr,(void*)&cobvar);
    (*env)->ReleaseStringUTFChars(env, cmd, cmdStr);
}


JNIEXPORT jint JNICALL Java_org_qwics_jni_QwicsTPMServerWrapper_readByte(JNIEnv *env, jobject self, jlong fd) {
    return 0;
}


JNIEXPORT jint JNICALL Java_org_qwics_jni_QwicsTPMServerWrapper_writeByte(JNIEnv *env, jobject self, jlong fd, jbyte b) {
    return 0;
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
    struct callbackFuncType callbackFunc;
    callbackFunc.callback = &execLoadModuleCallback;
    callbackFunc.env = env;
    callbackFunc.self = self;
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

    const char* name = (*env)->GetStringUTFChars(env, loadmod, NULL); 
    initExec(1);
    execCallbackInTransaction((char*)name,&_fd,(int)setCommArea,(int)parcnt,(void*)&callbackFunc);
    clearExec(1);
    (*env)->ReleaseStringUTFChars(env, loadmod, name);

    return 0;
}
