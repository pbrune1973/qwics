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

#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include "RuntimeContext.h"
#include "../spool/SpoolingSystem.h"

using namespace std;


RuntimeContext::RuntimeContext() {
  stackPointer = 0;
  globalParams = NULL;
  memLimit = 1024;
  cpuLimit = 3600;
  cpuUsed = 0;
  userId = 500;
  priority = -20;
  workingDir = "/tmp";
  jobId = "JOB12345";
  msgIdCounter = 0;
}

  
void RuntimeContext::push(JobCard *card) {
  executionStack[stackPointer] = card;
  if (stackPointer < 15) stackPointer++;
}

  
JobCard *RuntimeContext::pop() {
  if (stackPointer > 0) stackPointer--;
  return executionStack[stackPointer];
}

  
JobCard *RuntimeContext::top() {
  return executionStack[stackPointer-1];
}

  
JobCard *RuntimeContext::bottom() {
  return executionStack[0];
}

  
Parameters *RuntimeContext::getGlobalParams() {
  return globalParams;
}

  
void RuntimeContext::setGlobalParams(Parameters *globalParams) {
  this->globalParams = globalParams;
}


char *RuntimeContext::createSysoutFile(char *msgId) {
  FILE *sysout;
  int  count = 0,n;
  char *workingDir = SpoolingSystem::spoolingSystem->workingDir;
  char *name = new char[strlen(workingDir)+strlen(msgId)+2];

  sprintf(name,"%s/%s",workingDir,msgId);

  sysout = fopen(name,"r+");
  if (sysout != NULL) {
    n = fread(&count,sizeof(int),1,sysout);

    if (n < 1) {
      count = 0;
    } else {
      count++;
    }

    fseek(sysout,0L,SEEK_SET);
  } else {
    sysout = fopen(name,"w");
    count = 0;
  }
  fwrite(&count,sizeof(int),1,sysout);
  fclose(sysout);

  return name;
}


void RuntimeContext::deleteSysoutFile(char *name) {
  FILE *sysout;
  int  count = 0,n;

  sysout = fopen(name,"r+");
  n = fread(&count,sizeof(int),1,sysout);

  if (n < 1) {
    count = 0;
  }

  if (count > 0) {
    count--;
    fseek(sysout,0L,SEEK_SET);
    fwrite(&count,sizeof(int),1,sysout);
    fclose(sysout);
  } else {
    fclose(sysout);
    unlink(name);
  }
}


char *RuntimeContext::openLog(char *msgId) {
  char *workingDir = SpoolingSystem::spoolingSystem->workingDir;
  char *name = new char[strlen(workingDir)+strlen(msgId)+2];

  sprintf(name,"%s/%s",workingDir,msgId);
  //logFile.open(name,ios::out|ios::binary,0644);
  logFile.open(name,ios::out|ios::binary);
  return name;
}


void RuntimeContext::closeLog() {
  logFile.close();
}


void RuntimeContext::writeLog(unsigned line, char *str) {
  char *filler = "          ";
  if ((line > 0) && (line < 10))        filler = "         ";
  if ((line >= 10) && (line < 100))     filler = "        ";
  if ((line >= 100) && (line < 1000))   filler = "       ";
  if ((line >= 1000) && (line < 10000)) filler = "      ";
  
  if (stackPointer > 1) {
    str = &(str[2]);
    if (line < 1)
      logFile << filler << " XX" << str << endl;
    else
      logFile << filler << line << " XX" << str << endl;
  } else {
    if (line < 1)
      logFile << filler << " " << str << endl;
    else
      logFile << filler << line << " " << str << endl;
  }
}


char *RuntimeContext::getNextMsgId(char *idBuf) {
  sprintf(idBuf,"%s.%d",jobId,msgIdCounter);
  if (msgIdCounter < 999) msgIdCounter++;
  return idBuf;  
}
