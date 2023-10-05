/*******************************************************************************************/
/*   QWICS Batch Job Entry System                                                          */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 05.10.2023                                  */
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

#ifndef _JobClassQueue_h
#define _JobClassQueue_h

#include <pthread.h>
#include "semaphore.h"
#include "../card/JobCard.h"
#include "JCLParser.h"


typedef struct {
 char jobId[13];
 char jobName[9];
 char status;
 JobCard *job; 
 char *fileName;
 Parameters *params;
} JobInfo;


class JobClassQueue {
 private:
  JobInfo *jobs;
  int memQueued;
  int switchLimit;
  char *spoolDir;
  char *jobFile;
  char *queueFile[2];
  int  writeFile;
  int  readFile;
  pthread_mutex_t queueMutex;
  Semaphore entriesThere;
    
 protected:
  char *getJobFileName(char *name);
  virtual JobInfo loadJob(char *name);
  virtual int saveJob(JobInfo job);
         
 public:
  JobClassQueue(char *spoolDir, char *name, int memQueued, int switchLimit);
  ~JobClassQueue();
  
  JobInfo get(char status);
  JobInfo get(char status, char *jobName, char *jobId, int keep);
  int put(JobInfo job);  
};

#endif
