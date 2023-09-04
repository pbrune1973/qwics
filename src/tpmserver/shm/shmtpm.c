/*******************************************************************************************/
/*   QWICS Server COBOL shared memory handling                                             */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 04.09.2023                                  */
/*                                                                                         */
/*   Copyright (C) 2018 - 2023 by Philipp Brune  Email: Philipp.Brune@hs-neu-ulm.de        */
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
#include <stdlib.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "shmtpm.h"
#include "../env/envconf.h"

int shm_size = -1;
int blocknum = -1;
int blocksize = -1;

unsigned char *blocks;
pthread_mutex_t sharedMallocMutex;

extern void cm(int res);


void initSharedMalloc(int initBlocks) {
    blocks = (unsigned char*)shmPtr;
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&sharedMallocMutex,&attr);

    if (initBlocks) {
      for (int i = 0; i < BLOCKNUM; i++) {
        blocks[i] = 0x00;
      }
    }
}


int findBlock(int size, unsigned char id) {
  int i,beginEmpty = -1;
  int beginBest = -1;
  int sizeBest = BLOCKNUM*BLOCKSIZE;

  cm(pthread_mutex_lock(&sharedMallocMutex));
  for (i = 0; i < BLOCKNUM; i++) {
      if ((id > 0) && (blocks[i] == id)) {
        cm(pthread_mutex_unlock(&sharedMallocMutex));
        return i*BLOCKSIZE;
      }
      if (blocks[i] == 0) {
        if (beginEmpty < 0) {
          beginEmpty = i;
        }
      }
      if ((blocks[i] > 0) && (beginEmpty < i) && (beginEmpty >= 0)) {
        int s = (i-beginEmpty)*BLOCKSIZE;
        if (s >= size) {
            if ((s-size) < sizeBest) {
              beginBest = beginEmpty;
              sizeBest = s-size;
            }
        }
        beginEmpty = -1;
      }
  }
  int s = (i-beginEmpty)*BLOCKSIZE;
  if (s >= size) {
      if ((s-size) < sizeBest) {
        beginBest = beginEmpty;
      }
  }
  if (id <= 0) {
    id = 0xFF;
  }
  for (i = 0; i*BLOCKSIZE < size; i++) {
      blocks[i+beginBest] = id;
  }

  cm(pthread_mutex_unlock(&sharedMallocMutex));
  return beginBest*BLOCKSIZE;
}


int freeBlock(int ptr, int size) {
    if ((ptr % BLOCKSIZE) != 0) {
        return -1;
    }
    int begin = ptr / BLOCKSIZE;
    cm(pthread_mutex_lock(&sharedMallocMutex));
    for (int i = 0; i*BLOCKSIZE < size; i++) {
        blocks[i+begin] = 0x00;
    }
    cm(pthread_mutex_unlock(&sharedMallocMutex));
    return 0;
}


void *sharedMalloc(unsigned char id, int size) {
  return (void*)(shmPtr+BLOCKNUM+findBlock(size,id));
}


int sharedFree(void *ptr, int size) {
  int p = (int)(ptr - BLOCKNUM - shmPtr);
  return freeBlock(p,size);
}

/*
int main(int argc, char *argv[]) {
  initFastMem();
  for (int i = 0; i < BLOCKNUM; i++) {
    printf("%d ",blocks[i]);
  }
  printf("\n");

  int a1 = findBlock(3,1);
  for (int i = 0; i < BLOCKNUM; i++) {
    printf("%d ",blocks[i]);
  }
  printf("\n");

  int a2 = findBlock(2,2);
  for (int i = 0; i < BLOCKNUM; i++) {
    printf("%d ",blocks[i]);
  }
  printf("\n");

  int a3 = findBlock(4,3);
  for (int i = 0; i < BLOCKNUM; i++) {
    printf("%d ",blocks[i]);
  }
  printf("\n");

  int a4 = findBlock(1,4);
  for (int i = 0; i < BLOCKNUM; i++) {
    printf("%d ",blocks[i]);
  }
  printf("\n");

  freeBlock(a2,2);
  for (int i = 0; i < BLOCKNUM; i++) {
    printf("%d ",blocks[i]);
  }
  printf("\n");

  a3 = findBlock(1,5);
  for (int i = 0; i < BLOCKNUM; i++) {
    printf("%d ",blocks[i]);
  }
  printf("\n");

  freeBlock(a4,1);
  for (int i = 0; i < BLOCKNUM; i++) {
    printf("%d ",blocks[i]);
  }
  printf("\n");

  findBlock(1,4);
  for (int i = 0; i < BLOCKNUM; i++) {
    printf("%d ",blocks[i]);
  }
  printf("\n");

  findBlock(1,7);
  for (int i = 0; i < BLOCKNUM; i++) {
    printf("%d ",blocks[i]);
  }
  printf("\n");

  return 0;
}
*/
