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
#include <string.h>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include "JOB.h"

using namespace std;


JOB::JOB(char *name) : JobCard(name) {
}


JOB::~JOB() {
}
  

int JOB::execute() {
  JobCard *card = firstSubCard;
  int condCode = 0;
  char *jobDir = new char[strlen(context->workingDir)+
                          strlen(context->jobId)+2];
  sprintf(jobDir,"%s/%s",context->workingDir,context->jobId);
printf("Creating JOB subdir %s\n",jobDir);
  context->writeLog(sourceLineNumber,sourceLines[0]);
  for (int i = 1; i < sourceLineCount; i++) {
    context->writeLog(0,sourceLines[i]);
  }

  if (mkdir(jobDir,S_IRWXU | S_IXGRP | S_IRGRP | S_IXOTH | S_IROTH) < 0) {
    context->writeLog(0,"ERROR CREATING JOB SUBDIRECTORY");
    return errno;
  }

  context->push(this);
  
  while (card != NULL) {
    printf("run job card %x\n",card);
    card->setRuntimeContext(context);
    int rc = card->execute();
    if (rc > condCode) condCode = rc;
    printf("end job card rc=%d\n",rc);
 
    card = card->nextJobCard;
    printf("next job card %x\n",card);
  }
  
  context->pop();

  if (rmdir(jobDir) < 0) {
    context->writeLog(0,"ERROR DELETING JOB SUBDIRECTORY");
    return errno;
  }

  delete jobDir;
  return condCode;
}


void JOB::print(FILE *file) {
  for (int i = 0; i < sourceLineCount; i++) {
    fputs(sourceLines[i],file);
    fputc('\n',file);
  } 

  JobCard *card = firstSubCard; 
  while (card != NULL) { 
    card->print(file);
    card = card->nextJobCard;
  }

  fputs("//\n",file);
}
