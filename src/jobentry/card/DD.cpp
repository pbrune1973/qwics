/*******************************************************************************************/
/*   QWICS Batch Job Entry System                                                          */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 30.08.2023                                  */
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
#include "DD.h"

using namespace std;


DD::DD(char *name) : JobCard(name) {
  delete this->name;
  int l = strlen(name); if (l > 17) l = 17;
  this->name = new char[l+1];
  for (int i = 0; i < l; i++) this->name[i] = name[i];
  this->name[l] = 0x00; 

  deleteFile = 0;
  fileName = NULL;
  numOfSubmits = 0;
  dataSetDef = NULL;
}


DD::~DD() {
  int i;

  if (fileName != NULL) delete fileName;
  /*
  for (i = 0; i < numOfSubmits; i++) {
    if (submits[i].params != NULL) delete submits[i].params;
  }
  */
  if (dataSetDef != NULL) delete dataSetDef;
}
  

DataSetDef *DD::getDataSetDef() {
  return dataSetDef;
}


JobCard *DD::getCopy() {
  DD *copy = new DD(this->name);

  if (this->params != NULL) {
    copy->setParameters(this->params->getCopy());
  }

  if (this->parameterOverrides != NULL) {
    copy->setParameterOverrides(this->parameterOverrides->getCopy());
  }

  copy->setSourceLineNumber(this->sourceLineNumber);
  for (int i = 0; i < this->sourceLineCount; i++) {
    copy->addSourceLine(this->sourceLines[i]);
  }

  copy->setRuntimeContext(this->context);
  return copy;
}


char *DD::getFileName() {
  return fileName;
}


void DD::splitString(char *str, char separator, char **subStrings, int *nMax) {
  int i,j,n,l = strlen(str);

  n = 0;
  j = 0;
  for (i = 0; i < l; i++) {
    if (str[i] == '.') {
      subStrings[n][j] = 0x00;
      j = 0;

      if (n < (*nMax)) {
        n++;
      } else {
        (*nMax) = n+1;
        return;
      }
    } else
    if (j < 8) {
      subStrings[n][j] = str[i];
      j++;
    }    
  }

  subStrings[n][j] = 0x00;
  (*nMax) = n+1;
}


int DD::equalsType(char *type) {
  if (strcmp(type,"DD") == 0) 
    return 1;
  else 
    return 0;
}


int DD::execute() {
  char *path,*out,*msgClass;
  JobCard *myJOB,*myPROC,*myEXEC,*myOUTPUT;
  Parameters *outList,*sysoutList;
  int i;
  FILE *f;

  deleteFile = 0;
  fileName = NULL;
  numOfSubmits = 0;

  context->writeLog(sourceLineNumber,sourceLines[0]);
  for (int i = 1; i < sourceLineCount; i++) {
    context->writeLog(0,sourceLines[i]);
  }

  Parameters *params = this->params->getRuntimeParams(parameterOverrides,context->getGlobalParams(),NULL);
  this->runtimeParams = params;
  params->toString();
  cout << endl;
  sysoutList = NULL;

  if (((path = params->getValue("SYSOUT",0)) != NULL) || 
      ((sysoutList = params->getPValue("SYSOUT",0)) != NULL)) {
    // SYSOUT DD
    myJOB = context->bottom();
    myEXEC = context->top();

    if (path == NULL) {
      path = sysoutList->getValue(0);
    }
    
    msgClass = NULL;
    if (path != NULL) {
      if (strcmp(path,"*") == 0) {
        msgClass = myJOB->getParameters()->getValue("MSGCLASS",0);
      } else {
        msgClass = path;
      }
    } 
 
    char *names[4],name1[9],name2[9],name3[9],name4[9];  
    names[0] = name1;
    names[1] = name2;
    names[2] = name3;
    names[3] = name4;
    int n = 3;
    myOUTPUT = NULL;

    if ((out = params->getValue("OUTPUT",0)) != NULL) {
      splitString(out,'.',names,&n);

      if ((strcmp(names[0],"*") != 0) || (n < 2)) {
        context->writeLog(0,"INVALID OUTPUT STATEMENT NAME");
        throw ABORT_EXCPT;
      } else 
      if (n == 2) {
        myOUTPUT = myEXEC->getSubCard(names[1],"OUTPUT");

        if (myOUTPUT == NULL) {
          myOUTPUT = myJOB->getSubCard(names[1],"OUTPUT");
        }

        if (myOUTPUT == NULL) {
          context->writeLog(0,"REFERENCE TO UNKNOWN OUTPUT STATEMENT");
          throw ABORT_EXCPT;
        }
      } else 
      if (n == 3) {
        myEXEC = myJOB->getSubCard(names[1],"EXEC");

        if (myEXEC == NULL) {
          context->writeLog(0,"REFERENCE TO UNKNOWN EXEC STATEMENT");
          throw ABORT_EXCPT;
        }

        myOUTPUT = myEXEC->getSubCard(names[2],"OUTPUT");

        if (myOUTPUT == NULL) {
          context->writeLog(0,"REFERENCE TO UNKNOWN OUTPUT STATEMENT");
          throw ABORT_EXCPT;
        }
      } else 
      if (n == 4) {
        myPROC = myJOB->getSubCard(names[1],"PROC");

        if (myPROC == NULL) {
          context->writeLog(0,"REFERENCE TO UNKNOWN PROCEDURE");
          throw ABORT_EXCPT;
        }

        myEXEC = myPROC->getSubCard(names[2],"EXEC");

        if (myEXEC == NULL) {
          context->writeLog(0,"REFERENCE TO UNKNOWN EXEC STATEMENT");
          throw ABORT_EXCPT;
        }

        myOUTPUT = myEXEC->getSubCard(names[3],"OUTPUT");

        if (myOUTPUT == NULL) {
          context->writeLog(0,"REFERENCE TO UNKNOWN OUTPUT STATEMENT");
          throw ABORT_EXCPT;
        }
      }

      if (msgClass == NULL) {
        msgClass = myOUTPUT->getParameters()->getValue("CLASS",0);
      }

      if (msgClass == NULL) {
        context->writeLog(0,"INVALID SYSOUT CLASS VALUE");
        throw ABORT_EXCPT;
      }

      if (numOfSubmits > 39) {
        context->writeLog(0,"TOO MANY SYSOUT DATASETS CREATED (>40)");
        throw ABORT_EXCPT;
      }
      sprintf(submits[numOfSubmits].msgClass,"%s",msgClass);
      submits[numOfSubmits].params = myOUTPUT->getParameters()->getRuntimeParams(this->getParameters(),NULL,NULL);
      numOfSubmits++;
    } else 
    if ((outList = params->getPValue("OUTPUT",0)) != NULL) {
      i = 0;
      while ((out = outList->getValue(i)) != NULL) {
        n = 3;
        splitString(out,'.',names,&n);

        if ((strcmp(names[0],"*") != 0) || (n < 2)) {
          context->writeLog(0,"INVALID OUTPUT STATEMENT NAME");
          throw ABORT_EXCPT;
        } else 
        if (n == 2) {
          myOUTPUT = myEXEC->getSubCard(names[1],"OUTPUT");

          if (myOUTPUT == NULL) {
            myOUTPUT = myJOB->getSubCard(names[1],"OUTPUT");
          }

          if (myOUTPUT == NULL) {
            context->writeLog(0,"REFERENCE TO UNKNOWN OUTPUT STATEMENT");
            throw ABORT_EXCPT;
          }
        } else 
        if (n == 3) {
          myEXEC = myJOB->getSubCard(names[1],"EXEC");

          if (myEXEC == NULL) {
            context->writeLog(0,"REFERENCE TO UNKNOWN EXEC STATEMENT");
            throw ABORT_EXCPT;
          }

          myOUTPUT = myEXEC->getSubCard(names[2],"OUTPUT");

          if (myOUTPUT == NULL) {
            context->writeLog(0,"REFERENCE TO UNKNOWN OUTPUT STATEMENT");
            throw ABORT_EXCPT;
          }
        } else 
        if (n == 4) {
          myPROC = myJOB->getSubCard(names[1],"PROC");

          if (myPROC == NULL) {
            context->writeLog(0,"REFERENCE TO UNKNOWN PROCEDURE");
            throw ABORT_EXCPT;
          }

          myEXEC = myPROC->getSubCard(names[2],"EXEC");

          if (myEXEC == NULL) {
            context->writeLog(0,"REFERENCE TO UNKNOWN EXEC STATEMENT");
            throw ABORT_EXCPT;
          }

          myOUTPUT = myEXEC->getSubCard(names[3],"OUTPUT");

          if (myOUTPUT == NULL) {
            context->writeLog(0,"REFERENCE TO UNKNOWN OUTPUT STATEMENT");
            throw ABORT_EXCPT;
          }
        }

        if (msgClass == NULL) {
          msgClass = myOUTPUT->getParameters()->getValue("CLASS",0);
        }

        if (msgClass == NULL) {
          context->writeLog(0,"INVALID SYSOUT CLASS VALUE");
          throw ABORT_EXCPT;
        }

        if (numOfSubmits > 39) {
          context->writeLog(0,"TOO MANY SYSOUT DATASETS CREATED (>40)");
          throw ABORT_EXCPT;
        }
        sprintf(submits[numOfSubmits].msgClass,"%s",msgClass);
        submits[numOfSubmits].params = myOUTPUT->getParameters()->getRuntimeParams(this->getParameters(),NULL,NULL);
        numOfSubmits++;
        i++;
      }
    } else {
      myOUTPUT = myEXEC->getSubCard("OUTPUT","DEFAULT","YES");

      if (myOUTPUT == NULL) {
        myOUTPUT = myJOB->getSubCard("OUTPUT","DEFAULT","YES");
      }

      if ((msgClass == NULL) && (myOUTPUT != NULL)) {
        msgClass = myOUTPUT->getParameters()->getValue("CLASS",0);
      }

      if (msgClass == NULL) {
        context->writeLog(0,"INVALID SYSOUT CLASS VALUE");
        throw ABORT_EXCPT;
      }

      if (numOfSubmits > 39) {
        context->writeLog(0,"TOO MANY SYSOUT DATASETS CREATED (>40)");
        throw ABORT_EXCPT;
      }
      sprintf(submits[numOfSubmits].msgClass,"%s",msgClass);
      if (myOUTPUT != NULL) {
        submits[numOfSubmits].params = myOUTPUT->getParameters()->getCopy();
      } else {
        submits[numOfSubmits].params = new Parameters();
      }
      numOfSubmits++;
    }

    context->getNextMsgId(sysoutFileInfo.jobId);
    sprintf(sysoutFileName,"%s",sysoutFileInfo.jobId);
    fileName = RuntimeContext::createSysoutFile(sysoutFileName);
    sprintf(sysoutFileInfo.jobName,"%s",context->bottom()->name);
    sysoutFileInfo.status = 'W';
    sysoutFileInfo.job = NULL;
    deleteFile = 0;
    dataSetDef = new DataSetDef(fileName,params);
  } else    
  if ((path = params->getValue("PATH",0)) != NULL) {
    // Unix-style DD
    if (strcmp(path,"*") == 0) {
      int i,l = strlen(this->name);
      fileName = new char[l+1];
      for (i = 0; i < l; i++) fileName[i] = this->name[i];
      fileName[i] = 0x00;
      deleteFile = 1;
    } else {
      int i,l = strlen(path);
      fileName = new char[l+1];
      for (i = 0; i < l; i++) fileName[i] = path[i];
      fileName[i] = 0x00;
      deleteFile = 0;
    }
  } else {
    // Normal DD card
    dataSetDef = new DataSetDef(params);
  } 

  this->runtimeParams = NULL;
  delete params;

  return 0;
}


int DD::executeSpecial(Parameters *params, JobCard *SubCards) {
  JobCard *myEXEC;
  int i = 0, r = 0;

  if (dataSetDef != NULL) {
    myEXEC = context->top();
    dataSetDef->cleanup(myEXEC->getConditionCode());
    delete dataSetDef;
  }

  while (i < numOfSubmits) {
    if (i == 0) {
      sysoutFileInfo.fileName = new char[strlen(fileName)+1];  
      sprintf(sysoutFileInfo.fileName,"%s",fileName); 
    } else {
      context->getNextMsgId(sysoutFileInfo.jobId);
      sysoutFileInfo.fileName = RuntimeContext::createSysoutFile(sysoutFileName);
    }
    sysoutFileInfo.params = submits[i].params;
    r = SpoolingSystem::spoolingSystem->submitToClass(sysoutFileInfo,submits[i].msgClass);

    if (r < 0) {
      context->writeLog(0,"INVALID OUTPUT QUEUE SPECIFIED");
      throw ABORT_EXCPT;
    }

    i++;
  }

  if (deleteFile) {
    if (unlink(fileName) < 0) {
      context->writeLog(0,"ERROR DELETING DD TEMP FILE");
      return errno;
    }
  }

  return 0;
}


void DD::print(FILE *file) { 
  fputs("//",file);
  fputs(name,file);
  fputs(" DD ",file);
  params->print(file,0);
  fputc('\n',file);
}
