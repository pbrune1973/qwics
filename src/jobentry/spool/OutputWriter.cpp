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

#include <stdio.h>
#include <unistd.h>
#include "OutputWriter.h"


OutputWriter::OutputWriter(JobClassQueue *queue, int stop) : 
              Initiator(queue,stop,0L,0L) {
  this->reader  = NULL;
  this->jobName = NULL;
  this->jobId   = NULL;
  this->keep    = 0;
}


OutputWriter::OutputWriter(JobClassQueue *queue, CardReader *reader, 
                           char *jobName, char *jobId, int keep) : 
              Initiator(queue,1,0L,0L) {
  this->reader  = reader;
  this->jobName = jobName;
  this->jobId   = jobId;
  this->keep    = keep;
}


OutputWriter::~OutputWriter() {
}

  
void OutputWriter::run() {
  char line[256];
  FILE *of;
  int isSysout,counter;
  char currentJobId[13];
 
  pthread_mutex_lock(&runMutex);
  pthread_mutex_lock(&stopMutex);

  do {
    pthread_mutex_unlock(&stopMutex);
    counter = 0;

    if (jobId != NULL) {
      sprintf(currentJobId,"%s.%d",jobId,counter);
      job = queue->get('W',jobName,currentJobId,keep);
      counter++;
    } else {
      job = queue->get('W',jobName,jobId,keep);
    }

    while (job.fileName != NULL) {
      isSysout = 1;
      if ((job.fileName[strlen(job.fileName)-1] == '0') &&
          (job.fileName[strlen(job.fileName)-2] == '.')) {
        isSysout = 0;
      }        
      of = fopen(job.fileName,"r"); 
      if (of != NULL) {
        if (isSysout) {
          fseek(of,sizeof(int),SEEK_SET);
        }
        while ((!feof(of)) && (fgets(line,255,of) != NULL)) {
          if (reader != NULL) {
            reader->writeLine(line);
          }
        }
        fclose(of);
      }

      if (!keep) {
        if (!isSysout) {
          unlink(job.fileName);
        } else {  
          RuntimeContext::deleteSysoutFile(job.fileName);
        }
        delete job.fileName;
        delete job.params;
      }

      if (jobId != NULL) {
        sprintf(currentJobId,"%s.%d",jobId,counter);
        job = queue->get('W',jobName,currentJobId,keep);
        counter++;
      } else {
        job.fileName = NULL;
      }
    }
    pthread_mutex_lock(&stopMutex);
  } while (!stopFlag);

  pthread_mutex_unlock(&stopMutex);
  pthread_mutex_unlock(&runMutex);
}
