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

#ifndef _JobClass_h
#define _JobClass_h

#include "Initiator.h"
#include "JobClassQueue.h"


class JobClass {
 protected:
  char name[10];
  JobClassQueue *queue;
  Initiator **initiators;
  unsigned int numOfInitiators;
  unsigned int maxNumOfInitiators;
  unsigned long memLimit;
  unsigned long cpuLimit;
         
 public:
  JobClass();
  JobClass(char *name, 
           unsigned int numOfInitiators,
           unsigned int maxNumOfInitiators,
           unsigned long memLimit,
           unsigned long cpuLimit,
           char *spoolDir, 
           int memQueued, 
           int switchLimit);
  ~JobClass();
  
  char *getName();
  int addInitiator(int stop);
  int removeInitiator();
  int submit(JobInfo job);  
};

#endif
