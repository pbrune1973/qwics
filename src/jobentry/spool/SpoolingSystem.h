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

#ifndef _SpoolingSystem_h
#define _SpoolingSystem_h

#include <pthread.h>
#include "JobClass.h"
#include "OutputClass.h"


class SpoolingSystem {
 private:
  JobClass        *jobClasses[80];
  unsigned int    numOfJobClasses;
  unsigned long   jobIdCounter;  
  char            counterFile[255];
  JobInfo         newJob;
  pthread_mutex_t classMutex;

  SpoolingSystem(char *configFile, char *spoolDir, char *workingDir);
  ~SpoolingSystem();
          
 public:
  char *spoolDir;
  char *workingDir;
  static SpoolingSystem *spoolingSystem;
  
  static void create(char *configFile, char *spoolDir, char *workingDir);
  static void destroy();  

  int addJobClass(char *name, 
                  unsigned long memLimit,
                  unsigned long cpuLimit,
                  unsigned int numOfInitiators,
                  unsigned int maxNumOfInitiators,
                  int memQueued, 
                  int switchLimit);
  int addOutputClass(char *name, 
                     unsigned int numOfWriters,
                     unsigned int maxNumOfWriters,
                     int memQueued, 
                     int switchLimit);
  int removeJobClass(char *name);
  JobClass *getJobClass(char *name);
  JobClass *getJobClassEx(char *name);
  unsigned long getNewId();
  int submit(JobCard *job, char *jobId);
  int submit(JobInfo job, char *className);
};

#endif
