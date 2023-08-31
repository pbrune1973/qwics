/*******************************************************************************************/
/*   QWICS Batch Job Entry System                                                          */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 18.08.2023                                  */
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

#include "OutputClass.h"


OutputClass::OutputClass(char *name, 
                         unsigned int numOfWriters,
                         unsigned int maxNumOfWriters,
                         char *spoolDir, 
                         int memQueued, 
                         int switchLimit) {
  int i,l = strlen(name);

  if (l > 8) l = 8;
  this->name[0] = '_';
  for (i = 0; i < l; i++) this->name[i+1] = name[i];  
  this->name[l+1] = 0x00;

  this->numOfInitiators = numOfWriters;
  this->maxNumOfInitiators = maxNumOfWriters;
  this->memLimit = 0L;
  this->cpuLimit = 0L;

  this->initiators = new Initiator*[maxNumOfInitiators];
  this->queue = new OutputClassQueue(spoolDir,this->name,memQueued,switchLimit);

  if ((this->queue != NULL) && (initiators != NULL)) {
    for (i = 0; i < numOfInitiators; i++) {
      initiators[i] = new OutputWriter(queue,0);
      startInitiator(initiators[i]);
    }

    for (i = numOfInitiators; i < maxNumOfInitiators; i++) {
      initiators[i] = NULL;
    }
  }
}


OutputClass::~OutputClass() {
}

  
int OutputClass::addWriter(CardReader *reader, char *jobName, 
                           char *jobId, int keep) {
  int i;

  for (i = numOfInitiators; i < maxNumOfInitiators; i++) {
    if (initiators[i] == NULL) {
      initiators[i] = new OutputWriter(queue,reader,jobName,jobId,keep);
      startInitiator(initiators[i]);
      return 1;
    }
  }

  return 0;
}
