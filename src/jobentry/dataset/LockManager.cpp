/*******************************************************************************************/
/*   QWICS Batch Job Entry System                                                          */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 04.09.2023                                  */
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

#include <iostream>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "LockManager.h"
extern "C" {
#include "../../tpmserver/shm/shmtpm.h"
}

using namespace std;


LockManager *LockManager::lockManager = NULL;


LockManager::LockManager() {
  firstLock.next = NULL;
  firstLock.resourceName[0] = 0x00;
  lastLock = &firstLock;
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
  pthread_mutex_init(&lockManagerMutex,&attr);
}


LockManager::~LockManager() {
}


LockManager *LockManager::getLockManager() {
  if (LockManager::lockManager == NULL) {
    LockManager::lockManager = new LockManager();
  }

  return LockManager::lockManager;
}


struct Lock *LockManager::findLock(char *resourceName) {
  struct Lock *lock = firstLock.next;
  
  while (lock != NULL) {
    if (strcmp(lock->resourceName,resourceName) == 0) {
      return lock;
    }
    
    lock = lock->next;
  }

  return NULL;
}


void LockManager::getLock(char *resourceName, int type) {
  struct Lock *lock;

//cout << "getLock " << resourceName << endl;  
  pthread_mutex_lock(&lockManagerMutex);

  lock = findLock(resourceName);
  if (lock == NULL) {
    lock = (Lock*)sharedMalloc(10,sizeof(struct Lock));
    sprintf(lock->resourceName,"%s",resourceName);
    lock->type = type;
    lock->usage = 1;
    lock->typeUsage = 1;
    semaphore_init(&lock->isExclusive);
    semaphore_init(&lock->typeChange);
    semaphore_decrement(&lock->typeChange);
    lock->next = NULL;
    lock->prev = lastLock; 
    lastLock = lock;
  } else {
    lock->usage++;

    if (lock->type == type) {
      lock->typeUsage++;
    }
  }

  pthread_mutex_unlock(&lockManagerMutex);

  if (lock->type != type) {
    int poll = 1;
    do {
      semaphore_down(&lock->typeChange);
      pthread_mutex_lock(&lockManagerMutex);
      if (lock->typeUsage == 0) {
        lock->type = type;
        lock->typeUsage++;
        poll = 0;
      }  
      pthread_mutex_unlock(&lockManagerMutex);
    } while (poll);
  }
  
  if (type == LOCK_EXCLUSIVE) {
    semaphore_down(&lock->isExclusive);
  }
}


void LockManager::releaseLock(char *resourceName) {
  struct Lock *lock;
//cout << "releaseLock " << resourceName << endl;  
  
  pthread_mutex_lock(&lockManagerMutex);

  lock = findLock(resourceName);
  if (lock != NULL) {
    lock->usage--;
    lock->typeUsage--;

    if (lock->usage == 0) {
      lock->prev->next = lock->next;
      if (lock->next != NULL) {
        lock->next->prev = lock->prev;
      } else {
        lastLock = lock->prev;
      }

      semaphore_destroy(&lock->isExclusive);     
      semaphore_destroy(&lock->typeChange);     
      sharedFree(lock,sizeof(struct Lock));
 
      pthread_mutex_unlock(&lockManagerMutex);
      return;
    }

    if (lock->type == LOCK_EXCLUSIVE) {
      semaphore_up(&lock->isExclusive);     
    }

    if (lock->typeUsage == 0) {
      semaphore_up(&lock->typeChange);     
    }
  }

  pthread_mutex_unlock(&lockManagerMutex);
}
