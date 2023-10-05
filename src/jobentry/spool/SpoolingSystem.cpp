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

#include <iostream>
#include <fstream>
#include "SpoolingSystem.h"

using namespace std;


SpoolingSystem *SpoolingSystem::spoolingSystem = NULL;


void SpoolingSystem::create(char *configFile, char *spoolDir, char *workingDir) {
  if (SpoolingSystem::spoolingSystem == NULL) {
    SpoolingSystem::spoolingSystem = new SpoolingSystem(configFile,
                                                        spoolDir,
                                                        workingDir);
  }
}


void SpoolingSystem::destroy() {
  if (SpoolingSystem::spoolingSystem != NULL) {
    delete SpoolingSystem::spoolingSystem;
  }
}


SpoolingSystem::SpoolingSystem(char *configFile, char *spoolDir, char *workingDir) {
  int i;
  ifstream cf(configFile);
  char     name[9];
  unsigned long memLimit;
  unsigned long cpuLimit;
  unsigned int numOfInitiators;
  unsigned int maxNumOfInitiators;
  int memQueued;
  int switchLimit;

  this->spoolDir   = spoolDir;
  this->workingDir = workingDir;

  for (i = 0; i < 80; i++) {
    jobClasses[i] = NULL;
  }
  numOfJobClasses = 0;

  cf >> name;
  while ((strcmp(name,"") != 0) && (strcmp(name,"OUTPUT") != 0)) {
    cf >> memLimit;
    cf >> cpuLimit;
    cf >> numOfInitiators;
    cf >> maxNumOfInitiators;
    cf >> memQueued;
    cf >> switchLimit;

    addJobClass(name,memLimit,cpuLimit,numOfInitiators,maxNumOfInitiators,
                memQueued,switchLimit);

    cf >> name;
  }

  if (strcmp(name,"OUTPUT") == 0) {
    cf >> name;

    while ((strcmp(name,"") != 0) && (strcmp(name,"END-OUTPUT") != 0)) {
      cf >> maxNumOfInitiators;
      cf >> memQueued;
      cf >> switchLimit;

      addOutputClass(name,1,maxNumOfInitiators,
                     memQueued,switchLimit);

      cf >> name;
    }
  }
  cf.close();

  jobIdCounter = 0;  
  sprintf(this->counterFile,"%s/JOBCOUNT",this->spoolDir);
  FILE *cntf = fopen(counterFile,"rb");
  if (cntf != NULL) {
    fseek(cntf,0L,SEEK_SET);
    fread((void*)&jobIdCounter,sizeof(unsigned long),1,cntf);
    fclose(cntf);
  }

  pthread_mutex_init(&classMutex,NULL);
}


SpoolingSystem::~SpoolingSystem() {
  int i;

  pthread_mutex_lock(&classMutex);

  for (i = 0; i < 80; i++) {
    if (jobClasses[i] != NULL) {
      delete jobClasses[i];
    }
  }

  pthread_mutex_unlock(&classMutex);
}


int SpoolingSystem::addJobClass(char *name, 
                                unsigned long memLimit,
                                unsigned long cpuLimit,
                                unsigned int numOfInitiators,
                                unsigned int maxNumOfInitiators,
                                int memQueued, 
                                int switchLimit) {
  int i;

  pthread_mutex_lock(&classMutex);

  for (i = 0; i < 40; i++) {
    if (jobClasses[i] == NULL) {
      jobClasses[i] = new JobClass(name,
                                   numOfInitiators,maxNumOfInitiators,
                                   memLimit,cpuLimit,
                                   spoolDir,memQueued,switchLimit);
      numOfJobClasses++;
      pthread_mutex_unlock(&classMutex);
      return 1;
    }
  }

  pthread_mutex_unlock(&classMutex);
  return 0;
}


int SpoolingSystem::addOutputClass(char *name, 
                                   unsigned int numOfWriters,
                                   unsigned int maxNumOfWriters,
                                   int memQueued, 
                                   int switchLimit) {
  int i;

  pthread_mutex_lock(&classMutex);

  for (i = 40; i < 80; i++) {
    if (jobClasses[i] == NULL) {
      jobClasses[i] = new OutputClass(name,
                                      numOfWriters,maxNumOfWriters,
                                      spoolDir,memQueued,switchLimit);
      numOfJobClasses++;
      pthread_mutex_unlock(&classMutex);
      return 1;
    }
  }

  pthread_mutex_unlock(&classMutex);
  return 0;
}


int SpoolingSystem::removeJobClass(char *name) {
  int i;

  pthread_mutex_lock(&classMutex);

  for (i = 0; i < 80; i++) {
    if ((jobClasses[i] != NULL) && (strcmp(jobClasses[i]->getName(),name) == 0)) {
      delete jobClasses[i];
      jobClasses[i] = NULL;
      numOfJobClasses--;
      pthread_mutex_unlock(&classMutex);
      return 1;
    }
  }

  pthread_mutex_unlock(&classMutex);
  return 0;
}


JobClass *SpoolingSystem::getJobClass(char *name) {
  int i;

  for (i = 0; i < 80; i++) {
    if ((jobClasses[i] != NULL) && (strcmp(jobClasses[i]->getName(),name) == 0)) {
      return jobClasses[i];
    }
  }

  return NULL;
}


JobClass *SpoolingSystem::getJobClassEx(char *name) {
  int i;

  pthread_mutex_lock(&classMutex);

  for (i = 0; i < 80; i++) {
    if ((jobClasses[i] != NULL) && (strcmp(jobClasses[i]->getName(),name) == 0)) {
      pthread_mutex_unlock(&classMutex);
      return jobClasses[i];
    }
  }

  pthread_mutex_unlock(&classMutex);
  return NULL;
}


unsigned long SpoolingSystem::getNewId() {
  unsigned long id;
  FILE *cntf;
  
  pthread_mutex_lock(&classMutex);

  id = jobIdCounter % 100000;
  jobIdCounter++;

  cntf = fopen(counterFile,"wb");
  if (cntf != NULL) {
    fseek(cntf,0L,SEEK_SET);
    fwrite((void*)&jobIdCounter,sizeof(unsigned long),1,cntf);
    fclose(cntf);
  }

  pthread_mutex_unlock(&classMutex);
  return id;
}


int SpoolingSystem::submit(JobCard *job, char *jobId) {
  char *name = job->getParameters()->getValue("CLASS",0);
  if (name == NULL) return -1;

  pthread_mutex_lock(&classMutex);

  JobClass *cls = getJobClass(name);
  if (cls == NULL) {
    pthread_mutex_unlock(&classMutex);
    return -1;
  }

  unsigned long id = jobIdCounter % 100000;
  jobIdCounter++;

  FILE *cntf = fopen(counterFile,"wb");
  if (cntf != NULL) {
    fseek(cntf,0L,SEEK_SET);
    fwrite((void*)&jobIdCounter,sizeof(unsigned long),1,cntf);
    fclose(cntf);
  }
  
  if (id < 10) {
    sprintf(newJob.jobId,"JOB0000%d",id);
  } else  
  if (id < 100) {
    sprintf(newJob.jobId,"JOB000%d",id);
  } else  
  if (id < 1000) {
    sprintf(newJob.jobId,"JOB00%d",id);
  } else  
  if (id < 10000) {
    sprintf(newJob.jobId,"JOB0%d",id);
  } else {
    sprintf(newJob.jobId,"JOB%d",id);
  }
  sprintf(newJob.jobName,"%s",job->name);
  newJob.job = job;
  newJob.status = 'W';
  sprintf(jobId,"%s",newJob.jobId);
  newJob.params = NULL;
  newJob.fileName = NULL;

  int r =  cls->submit(newJob);  
  pthread_mutex_unlock(&classMutex);
  return r;
}


int SpoolingSystem::submitToClass(JobInfo job, char *className) {
  if (className == NULL) return -1;

  pthread_mutex_lock(&classMutex);

  JobClass *cls = getJobClass(className);
  if (cls == NULL) {
    pthread_mutex_unlock(&classMutex);
    return -1; 
  }
  int r =  cls->submit(job);  

  pthread_mutex_unlock(&classMutex);
  return r;
}
