/*******************************************************************************************/
/*   QWICS Batch Job Entry System                                                          */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 31.08.2023                                  */
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

#include "JobClass.h"


JobClass::JobClass() {
}


JobClass::JobClass(char *name, 
                   unsigned int numOfInitiators,
                   unsigned int maxNumOfInitiators,
                   unsigned long memLimit,
                   unsigned long cpuLimit,
                   char *spoolDir, 
                   int memQueued, 
                   int switchLimit) {
  int i,l = strlen(name);

  if (l > 8) l = 8;
  for (i = 0; i < l; i++) this->name[i] = name[i];  
  this->name[l] = 0x00;

  this->numOfInitiators = numOfInitiators;
  this->maxNumOfInitiators = maxNumOfInitiators;
  this->memLimit = memLimit;
  this->cpuLimit = cpuLimit;

  this->initiators = new Initiator*[maxNumOfInitiators];
  this->queue = new JobClassQueue(spoolDir,this->name,memQueued,switchLimit);

  if ((this->queue != NULL) && (initiators != NULL)) {
    for (i = 0; i < numOfInitiators; i++) {
      initiators[i] = new Initiator(queue,0,memLimit,cpuLimit);
      startInitiator(initiators[i]);
    }

    for (i = numOfInitiators; i < maxNumOfInitiators; i++) {
      initiators[i] = NULL;
    }
  }
}


JobClass::~JobClass() {
  int i;

  if (initiators != NULL) {
    for (i = 0; i < maxNumOfInitiators; i++) {
      if (initiators[i] != NULL) delete initiators[i];
    }
  }
  if (queue != NULL) delete queue; 
  if (initiators != NULL) delete initiators; 
} 
 

char *JobClass::getName() {
  return name;
}


int JobClass::addInitiator(int stop) {
  int i;

  for (i = numOfInitiators; i < maxNumOfInitiators; i++) {
    if (initiators[i] == NULL) {
      initiators[i] = new Initiator(queue,stop,memLimit,cpuLimit);
      startInitiator(initiators[i]);
      return 1;
    }
  }

  return 0;
}


int JobClass::removeInitiator() {
  int i;

  for (i = numOfInitiators; i < maxNumOfInitiators; i++) {
    if (initiators[i] != NULL) {
      initiators[i]->stop();
      delete initiators[i];
      initiators[i] = NULL;
      return 1;
    }
  }

  return 0;
}


int JobClass::submit(JobInfo job) {
  return queue->put(job);
}
