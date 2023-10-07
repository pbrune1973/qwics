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

#include <stdio.h>
#include <iostream>
#include <errno.h>
#include <unistd.h>
#include "JobClassQueue.h"

using namespace std;


JobClassQueue::JobClassQueue(char *spoolDir, char *name, int memQueued, int switchLimit) {
  int l1 = strlen(spoolDir), l2 = strlen(name), l = l1+l2+2;
  int i;
  char flags[2];
  FILE *qf = NULL;

  this->switchLimit = switchLimit;

  jobs = new JobInfo[memQueued];
  this->memQueued = memQueued;
  for (i = 0; i < memQueued; i++) {
    jobs[i].status = 'X';
  }

  this->spoolDir = new char[l1+1];
  this->jobFile = new char[l1+14];
  this->queueFile[0] = new char[l+1];
  this->queueFile[1] = new char[l+1];
  for (i = 0; i < l1; i++) {
    this->spoolDir[i]     = spoolDir[i];
    this->queueFile[0][i] = spoolDir[i];
    this->queueFile[1][i] = spoolDir[i];
  }
  this->spoolDir[i] = 0x00;
  this->queueFile[0][i] = '/';
  this->queueFile[1][i] = '/';

  for (i = 0; i < l2; i++) {
    this->queueFile[0][i+l1+1] = name[i];
    this->queueFile[1][i+l1+1] = name[i];
  }
  this->queueFile[0][i+l1+1] = '0';
  this->queueFile[1][i+l1+1] = '1';
  this->queueFile[0][i+l1+2] = 0x00;
  this->queueFile[1][i+l1+2] = 0x00;

  this->readFile = -1;
  this->writeFile = -1;

  if ((qf = fopen(this->queueFile[0],"rb")) != NULL) {
    fseek(qf,0L,SEEK_SET);
    fread(flags,2,1,qf);  
    if (flags[0] == 'R') this->readFile = 0;
    if (flags[1] == 'W') this->writeFile = 0;
    fclose(qf);
  }

  if ((this->writeFile < 0) || (this->readFile < 0)) {
    if ((qf = fopen(this->queueFile[1],"rb")) != NULL) {
      fseek(qf,0L,SEEK_SET);
      fread(flags,2,1,qf);  
      if (flags[0] == 'R') this->readFile = 1;
      if (flags[1] == 'W') this->writeFile = 1;
      fclose(qf);
    }
  }

  if ((this->writeFile < 0) || (this->readFile < 0)) {
    if ((qf = fopen(this->queueFile[0],"wb")) != NULL) {
      flags[0] = 'R';
      flags[1] = 'W';
      fseek(qf,0L,SEEK_SET);
      fwrite(flags,2,1,qf);  
      this->readFile = 0;
      this->writeFile = 0;
      fclose(qf);
    }
  }
  
  pthread_mutex_init(&queueMutex,NULL);
  semaphore_init(&entriesThere);
  semaphore_decrement(&entriesThere);
}


JobClassQueue::~JobClassQueue() {
  if (jobs != NULL) delete jobs;
  if (queueFile[0] != NULL) delete queueFile[0];
  if (queueFile[1] != NULL) delete queueFile[1];
  if (spoolDir != NULL) delete spoolDir;
  if (jobFile != NULL) delete jobFile;
  semaphore_destroy(&entriesThere);
}


char *JobClassQueue::getJobFileName(char *name) {
  int i,l = strlen(spoolDir),l2 = strlen(name);

  for (i = 0; i < l; i++) jobFile[i] = spoolDir[i];
  jobFile[l] = '/';
  for (i = 0; i < l2; i++) jobFile[i+l+1] = name[i];
  jobFile[l+l2+1] = 0x00;

  return jobFile;
}


JobInfo JobClassQueue::loadJob(char *name) {
 JobInfo job;
 FILE *jf = NULL;

 if ((jf = fopen(getJobFileName(name),"r")) != NULL) {
   fseek(jf,0L,SEEK_SET);
   fread(&job,sizeof(JobInfo),1,jf);
   fseek(jf,(long)sizeof(JobInfo),SEEK_SET);
   JCLParser *p = new JCLParser(jf);
   job.job = p->parse();
   delete p;
   fclose(jf);
 }

 return job;   
}

  
int JobClassQueue::saveJob(JobInfo job) {
 FILE *jf = NULL;

 if ((jf = fopen(getJobFileName(job.jobId),"w")) != NULL) {
   fwrite(&job,sizeof(JobInfo),1,jf);
   // fseek(jf,(long)sizeof(JobInfo),SEEK_SET);
   job.job->print(jf);
   fclose(jf);
 } else {
   return -1;
 }

 return 0;   
}

  
JobInfo JobClassQueue::get(char status) {
  FILE *wf = NULL, *qf = NULL;
  JobInfo job;
  long pos = -1L;
  int i,count,xCount;
  char flags[2];

  job.job = NULL;
  job.params = NULL;
  job.fileName = NULL;
cout << "get " << status << " " << this <<endl;

cout << "get entries " << semaphore_value(&entriesThere) << endl;
  semaphore_down(&entriesThere);
cout << "get continue " << endl;
  pthread_mutex_lock(&queueMutex);
cout << "get " << readFile << " " << writeFile << " " << queueFile[readFile] << endl;

  wf = fopen(queueFile[readFile],"r+");
cout << "get " << wf << endl;
  if (wf == NULL) {
    pthread_mutex_unlock(&queueMutex);
    return(job);
  } 
  fseek(wf,2L,SEEK_SET);
  xCount = 0;
  count = 0;
  do {
    if (fread(&job,23,1,wf) > 0) {
      if (job.status == 'X') xCount++;
      if (job.status == status) {
        pos = ftell(wf)-23;
      }

cout << count << " " << job.jobId << " " << job.status << " " << pos << endl;
      count++;
    }
  } while (!feof(wf) && !ferror(wf) && (pos < 0));

  if ((xCount >= this->switchLimit) && (this->writeFile == this->readFile)) {
    this->writeFile = 1 - this->readFile;

    if ((qf = fopen(this->queueFile[this->writeFile],"wb")) != NULL) {
      flags[0] = ' ';
      flags[1] = 'W';
      // fseek(qf,0L,SEEK_SET);
      fwrite(flags,2,1,qf);  
      fclose(qf);
    }

    flags[0] = 'R';
    flags[1] = ' ';
    fseek(wf,0L,SEEK_SET);
    fwrite(flags,2,1,wf);  
  }

  if (pos >= 0) {
    job.status = 'X';
    fseek(wf,(long)pos,SEEK_SET);
    fwrite(&job,23,1,wf);
    fclose(wf);

    for (i = 0; i < memQueued; i++) {
      if (strcmp(jobs[i].jobId,job.jobId) == 0) {
        jobs[i].status = 'X';
        unlink(getJobFileName(job.jobId));
        pthread_mutex_unlock(&queueMutex);
        return(jobs[i]);
      }
    }

    job = loadJob(job.jobId);
    job.status = 'X';
    unlink(getJobFileName(job.jobId));
    pthread_mutex_unlock(&queueMutex);
    return job;
  } else
  if ((xCount == count) && (this->writeFile != this->readFile)) {
    if ((qf = fopen(this->queueFile[this->writeFile],"r+")) != NULL) {
      flags[0] = 'R';
      flags[1] = 'W';
      fseek(qf,0L,SEEK_SET);
      fwrite(flags,2,1,qf);  
      fclose(qf);
    }

    fclose(wf);
    unlink(this->queueFile[this->readFile]);
    this->readFile = this->writeFile;
    pthread_mutex_unlock(&queueMutex);
    return this->get(status);   
  }

  fclose(wf);
  pthread_mutex_unlock(&queueMutex);
}


JobInfo JobClassQueue::get(char status, char *jobName, char *jobId, int keep) {
  FILE *wf = NULL, *qf = NULL;
  JobInfo job;
  long pos = -1L;
  int i,count,xCount,jid_l = 0;
  char flags[2];

  if (jobId != NULL) {
    jid_l = strlen(jobId);
  }

  job.job = NULL;
  job.params = NULL;
  job.fileName = NULL;

cout << "get entries " << semaphore_value(&entriesThere) << endl;
  semaphore_down(&entriesThere);
cout << "get continue " << endl;
  pthread_mutex_lock(&queueMutex);
cout << "get " << readFile << " " << writeFile << " " << queueFile[readFile] << endl;

  wf = fopen(queueFile[readFile],"r+");
cout << "get " << wf << endl;   
  if (wf == NULL) {
    pthread_mutex_unlock(&queueMutex);
    return(job);
  } 
  fseek(wf,2L,SEEK_SET);
  xCount = 0;
  count = 0;
  do {
    if (fread(&job,23,1,wf) > 0) {
      if (job.status == 'X') xCount++;
      if ((job.status == status) && 
          ((jobName == NULL) || (strcmp(jobName,job.jobName) == 0)) && 
          ((jobId == NULL) || (strncmp(job.jobId,jobId,jid_l) == 0))) {
        pos = ftell(wf)-23;
      }

      count++;
    }
  } while (!feof(wf) && !ferror(wf) && (pos < 0));

  if ((xCount >= this->switchLimit) && (this->writeFile == this->readFile)) {
    this->writeFile = 1 - this->readFile;

    if ((qf = fopen(this->queueFile[this->writeFile],"wb")) != NULL) {
      flags[0] = ' ';
      flags[1] = 'W';
      fseek(qf,0L,SEEK_SET);
      fwrite(flags,2,1,qf);  
      fclose(qf);
    }

    flags[0] = 'R';
    flags[1] = ' ';
    fseek(wf,0L,SEEK_SET);
    fwrite(flags,2,1,wf);  
  }

  if (pos >= 0) {
    if (!keep) {
      job.status = 'X';
      fseek(wf,(long)pos,SEEK_SET);
      fwrite(&job,23,1,wf);
    }
    fclose(wf);

    for (i = 0; i < memQueued; i++) {
      if (strcmp(jobs[i].jobId,job.jobId) == 0) {
        if (!keep) {
          jobs[i].status = 'X';
          unlink(getJobFileName(job.jobId));
        }
        pthread_mutex_unlock(&queueMutex);
        return(jobs[i]);
      }
    }

    job = loadJob(job.jobId);
    if (!keep) {
      job.status = 'X';
      unlink(getJobFileName(job.jobId));
    }
    pthread_mutex_unlock(&queueMutex);
    return job;
  } else
  if ((xCount == count) && (this->writeFile != this->readFile)) {
    if ((qf = fopen(this->queueFile[this->writeFile],"r+")) != NULL) {
      flags[0] = 'R';
      flags[1] = 'W';
      fseek(qf,0L,SEEK_SET);
      fwrite(flags,2,1,qf);  
      fclose(qf);
    }

    fclose(wf);
    unlink(this->queueFile[this->readFile]);
    this->readFile = this->writeFile;
    pthread_mutex_unlock(&queueMutex);
    return this->get(status,jobName,jobId,keep);   
  }

  fclose(wf);

  if (this->writeFile != this->readFile) {
    pos = -1;
    wf = fopen(queueFile[writeFile],"r+");
    if (wf == NULL) return(job); 
    fseek(wf,2L,SEEK_SET);
    do {
      if (fread(&job,23,1,wf) > 0) {
        if ((job.status == status) && 
            ((jobName == NULL) || (strcmp(jobName,job.jobName) == 0)) && 
            ((jobId == NULL) || (strncmp(job.jobId,jobId,jid_l) == 0))) {
          pos = ftell(wf)-23;
        }
      }
    } while (!feof(wf) && !ferror(wf) && (pos < 0));

    if (pos >= 0) {
      if (!keep) {
        job.status = 'X';
        fseek(wf,(long)pos,SEEK_SET);
        fwrite(&job,23,1,wf);
      }
      fclose(wf);

      for (i = 0; i < memQueued; i++) {
        if (strcmp(jobs[i].jobId,job.jobId) == 0) {
          if (!keep) {
            jobs[i].status = 'X';
            unlink(getJobFileName(job.jobId));
          }
          pthread_mutex_unlock(&queueMutex);
          return(jobs[i]);
        }
      }

      job = loadJob(job.jobId);
      if (!keep) {
        job.status = 'X';
        unlink(getJobFileName(job.jobId));
      }
      pthread_mutex_unlock(&queueMutex);
      return job;
    }

    fclose(wf);
  }

  pthread_mutex_unlock(&queueMutex);
  return(job);
}


int JobClassQueue::put(JobInfo job) {
 FILE *wf = NULL;
 int i;

 if (saveJob(job) < 0) return -1;

 pthread_mutex_lock(&queueMutex);
cout << "put " << readFile << " " << writeFile << " " << queueFile[writeFile] << endl;

 wf = fopen(queueFile[writeFile],"a");
cout << "put " << wf << " " << this << endl;  
 if (wf == NULL) {
    pthread_mutex_unlock(&queueMutex);
    return -1;
 } 
 //fseek(wf,0L,SEEK_END);
 fwrite(&job,23,1,wf);

 if (fclose(wf) != 0) {
cout << "put fclose " << endl;  
   unlink(getJobFileName(job.jobId));
   pthread_mutex_unlock(&queueMutex);
   return -1;
 }

 for (i = 0; i < memQueued; i++) {
   if (jobs[i].status == 'X') {
     jobs[i] = job;
     break;
   }
 }

cout << "put 2 " << i << " " << memQueued << " " << job.job << endl; 
 if (i >= memQueued) {
   if (job.job != NULL) delete job.job;
   if (job.fileName != NULL) delete job.fileName;
   if (job.params != NULL) delete job.params;
 }
cout << "put 3" << endl;  

 semaphore_up(&entriesThere);
cout << "put 4" << endl;  
 pthread_mutex_unlock(&queueMutex);
cout << "put 5" << endl;  
 return 0;
}
