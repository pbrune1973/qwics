/*******************************************************************************************/
/*   QWICS Server COBOL ENQ/DEQ Synchonisation                                               */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 28.08.2019                                  */
/*                                                                                         */
/*   Copyright (C) 2018 by Philipp Brune  Email: Philipp.Brune@hs-neu-ulm.de               */
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
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "enqdeq.h"
#include "../shm/shmtpm.h"
#include "../env/envconf.h"

int max_enqres = -1;

struct enqRes {
  char resource[256];
  int len;
  int lockCount;
  pthread_mutex_t resMutex;
} *enqResources;

pthread_mutex_t enqResourceMutex;

extern void cm(int res);


struct taskLock *createTaskLocks() {
    struct taskLock *taskLocks = (struct taskLock*)malloc(sizeof(struct taskLock)*MAX_ENQRES);
    for (int i = 0; i < MAX_ENQRES; i++) {
      taskLocks[i].count = 0;
      taskLocks[i].type = UOW;
    }
    return taskLocks;
}


void initEnqResources(int initRes) {
  enqResources = sharedMalloc(15,sizeof(struct enqRes)*MAX_ENQRES);
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
  pthread_mutex_init(&enqResourceMutex,&attr);

  if (initRes) {
    for (int i = 0; i < MAX_ENQRES; i++) {
        enqResources[i].resource[0] = 0x00;
        enqResources[i].len = 0;
        enqResources[i].lockCount = 0;
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&enqResources[i].resMutex,&attr);
    }
  }
}


int enq(char *resource, int len, int nosuspend, int type, struct taskLock *taskLocks) {
  cm(pthread_mutex_lock(&enqResourceMutex));
  printf("%s %x %d %d\n","enq ",(unsigned int)resource,len,type);
  int i = 0;
  for (i = 0; i < MAX_ENQRES; i++) {
    int c = 0;
    if (len <= 0) {
      long ptr = (long)resource;
      c = memcmp((char*)&(enqResources[i].resource),(char*)&ptr,sizeof(ptr));
    } else {
      c = memcmp((char*)&(enqResources[i].resource),resource,len);
    }
    if ((c == 0) && (len == enqResources[i].len)) {
      int r = 0;
      enqResources[i].lockCount++;
      taskLocks[i].count++;
      taskLocks[i].type = type;
      if (nosuspend) {
        if ((r = pthread_mutex_trylock(&enqResources[i].resMutex)) != 0) {
          enqResources[i].lockCount--;
          taskLocks[i].count--;
        }
        cm(pthread_mutex_unlock(&enqResourceMutex));
      } else {
        cm(pthread_mutex_unlock(&enqResourceMutex));
        cm(pthread_mutex_lock(&enqResources[i].resMutex));
      }
      return r;
    }
  }
  for (i = 0; i < MAX_ENQRES; i++) {
    if (enqResources[i].lockCount == 0) {
      if (len <= 0) {
        long ptr = (long)resource;
        memcpy((char*)&(enqResources[i].resource),(char*)&ptr,sizeof(ptr));
      } else {
        memcpy((char*)&(enqResources[i].resource),resource,len);
      }
      enqResources[i].len = len;
      taskLocks[i].count++;
      taskLocks[i].type = type;
      int r = 0;
      if (nosuspend) {
        if ((r = pthread_mutex_trylock(&enqResources[i].resMutex)) != 0) {
          enqResources[i].lockCount--;
          taskLocks[i].count--;
        }
        cm(pthread_mutex_unlock(&enqResourceMutex));
      } else {
        cm(pthread_mutex_unlock(&enqResourceMutex));
        cm(pthread_mutex_lock(&enqResources[i].resMutex));
      }
      return r;
    }
  }
  cm(pthread_mutex_unlock(&enqResourceMutex));
  return -1;
}


int deq(char *resource, int len, int type, struct taskLock *taskLocks) {
  cm(pthread_mutex_lock(&enqResourceMutex));
  printf("%s %x %d %d\n","deq ",(unsigned int)resource,len,type);
  int i = 0;
  for (i = 0; i < MAX_ENQRES; i++) {
    int c = 0;
    if (len <= 0) {
      long ptr = (long)resource;
      c = memcmp((char*)&(enqResources[i].resource),(char*)&ptr,sizeof(ptr));
    } else {
      c = memcmp((char*)&(enqResources[i].resource),resource,len);
    }
    if ((c == 0) && (len == enqResources[i].len)) {
      enqResources[i].lockCount--;
      taskLocks[i].count--;
      cm(pthread_mutex_unlock(&enqResources[i].resMutex));
      cm(pthread_mutex_unlock(&enqResourceMutex));
      return 0;
    }
  }
  cm(pthread_mutex_unlock(&enqResourceMutex));
  return -1;
}


void releaseLocks(int type, struct taskLock *taskLocks) {
  cm(pthread_mutex_lock(&enqResourceMutex));
  int i,j;
  for (i = 0; i < MAX_ENQRES; i++) {
    if (taskLocks[i].type <= type) {
      for (j = 0; j < taskLocks[i].count; j++) {
        enqResources[i].lockCount--;
        cm(pthread_mutex_unlock(&enqResources[i].resMutex));
      }
      taskLocks[i].count = 0;
    }
  }
  cm(pthread_mutex_unlock(&enqResourceMutex));
}
