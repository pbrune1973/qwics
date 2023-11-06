/*******************************************************************************************/
/*   QWICS Batch Job Entry System                                                          */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 06.11.2023                                  */
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
#include <unistd.h>
#include "Initiator.h"
#include "SpoolingSystem.h"


Initiator::Initiator(JobClassQueue *queue, int stop, 
                     unsigned long memLimit, unsigned long cpuLimit) {
cout << "Initiator created " << queue << endl;
  this->queue = queue;
  this->stopFlag = stop;
  this->memLimit = memLimit;
  this->cpuLimit = cpuLimit;
  pthread_mutex_init(&stopMutex,NULL);
  pthread_mutex_init(&runMutex,NULL);
}


Initiator::~Initiator() {
  this->stop();
}

  
void Initiator::run() {
  RuntimeContext *context;
  JobInfo logFileInfo;
  char *msgClass = NULL;
cout << "Initiator started " << endl;
  pthread_mutex_lock(&runMutex);
  pthread_mutex_lock(&stopMutex);
cout << "Initiator started 2 " << queue << endl;

  do {
    pthread_mutex_unlock(&stopMutex);
    context = new RuntimeContext();

    job = queue->get('W');
    cout << "Initiator started 3 " << job.job << endl;
    if (job.job != NULL) {
      context->workingDir = SpoolingSystem::spoolingSystem->workingDir;
      context->jobId = job.jobId;
      context->memLimit = memLimit;
      context->cpuLimit = cpuLimit;
      context->userId = getuid();
      sprintf(logFileInfo.jobName,"%s",job.jobName);
      logFileInfo.fileName = context->openLog(context->getNextMsgId(logFileInfo.jobId));
      logFileInfo.status = 'W';
      logFileInfo.job = NULL;

      job.job->setRuntimeContext(context);
      try {
        job.job->execute();
      } catch (int e) {
        context->writeLog(0,"EXECUTION ABORTED");
      }
      context->closeLog();
      logFileInfo.params = job.job->getParameters()->getCopy();
      if ((msgClass = job.job->getParameters()->getValue("MSGCLASS",0)) != NULL) {
        char mc[10];
        sprintf(mc,"%s%s","_",msgClass);
        SpoolingSystem::spoolingSystem->submitToClass(logFileInfo,mc);
      } else {
        SpoolingSystem::spoolingSystem->submitToClass(logFileInfo,"_SYSOUT");
      }

      if (job.job != NULL) delete job.job;
      job.job = NULL;
    }
    delete context;
    pthread_mutex_lock(&stopMutex);
  } while (!stopFlag);

  pthread_mutex_unlock(&stopMutex);
  pthread_mutex_unlock(&runMutex);
}


void Initiator::stop() {
  pthread_mutex_lock(&stopMutex);
  stopFlag = 1;
  pthread_mutex_unlock(&stopMutex);
  pthread_mutex_lock(&runMutex);
  pthread_mutex_unlock(&runMutex);
}


void *runInitiator(void *initiator) {
  ((Initiator*)initiator)->run();
  return NULL;
}


unsigned long startInitiator(Initiator *initiator) {
  pthread_t threadId;
  pthread_create(&threadId,NULL,runInitiator,(void*)initiator);
  return (long)threadId;
}
