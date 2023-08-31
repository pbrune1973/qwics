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
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <iostream>
#include <sys/types.h>
#include <sys/resource.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#include "EXEC.h"
#include "DD.h"
#include "../dataset/Concatenation.h"
extern "C" {
#include "../../batchrun/cobsql.h"
}

using namespace std;


DataSetDef *EXEC::procLib = NULL;
DataSetDef *EXEC::linkLib = NULL;

extern JobCard *thisEXEC;


EXEC::EXEC(char *name) : JobCard(name) {
  Parameters *params;

  if (procLib == NULL) {
    params = new Parameters();
    params->add("DSN");
    params->setValue("SYS1.PROCLIB");
    params->add("DISP");
    params->setValue("SHR");
    procLib = new DataSetDef(params);
    delete params;
  }

  if (linkLib == NULL) {
    params = new Parameters();
    params->add("DSN");
    params->setValue("SYS1.LINKLIB");
    params->add("DISP");
    params->setValue("SHR");
    linkLib = new DataSetDef(params);
    delete params;
  }
}


EXEC::~EXEC() {
  if (procLib != NULL) delete procLib;
  if (linkLib != NULL) delete linkLib;  
}
  

int EXEC::execPROC(char *proc, Parameters *params) {
  JobCard *procCard = NULL,*ddCard = NULL;
  DataSet *lib,*jobLib,*procMember;
  struct PdsDirEntry dirEntry;
  JCLParser *jclParser;
  int condCode = 0;
  
  if (context->bottom() != NULL) {
    procCard = context->bottom()->getSubCard(proc);
  }
    
  if ((procCard != NULL) && procCard->equalsType("PROC")) {
    procCard->setRuntimeContext(context);
    condCode = procCard->executeSpecial(params,firstSubCard);
  } else {
    lib = procLib->open(ACCESS_READ | ACCESS_LOCK | ACCESS_SHARED);  
    if (lib == NULL) {
      context->writeLog(0,"CANNOT OPEN SYS1.PROCLIB");
      throw ABORT_EXCPT;
    }
 
    if (context->bottom() != NULL) {
      ddCard = context->bottom()->getSubCard("JCLLIB");
    }
    
    if ((ddCard != NULL) && ddCard->equalsType("DD")) {
      jobLib = ((DD*)ddCard)->getDataSetDef()->open(ACCESS_READ | ACCESS_LOCK | ACCESS_SHARED); 

      if (jobLib != NULL) {
        jobLib = new Concatenation(jobLib);
        ((Concatenation*)jobLib)->addDataSet(lib);
      } else {
        delete lib;
        context->writeLog(0,"CANNOT ACCESS SPECIFIED JCLLIB");
        throw ABORT_EXCPT;
      }      
    } else {
      jobLib = lib;
    }

    procMember = ((PartitionedDataSet*)jobLib)->findMember(proc,&dirEntry);  
    if (procMember != NULL) {
      jclParser = new JCLParser(procMember);
      procCard = jclParser->parse();

      if ((procCard != NULL) && procCard->equalsType("PROC")) {
        procCard->setRuntimeContext(context);
        condCode = procCard->executeSpecial(params,firstSubCard);
        delete procCard;
      } else {
        delete jclParser;  
        delete procMember;  
        delete jobLib;
        context->writeLog(0,"ERROR IN JCL PROC");
        throw ABORT_EXCPT;
      }
      
      delete jclParser;  
      delete procMember;  
      delete jobLib;
    } else {
      delete jobLib;
      context->writeLog(0,"UNKNOWN JCL PROC NAME");
      throw ABORT_EXCPT;      
    }
  }

  return condCode;
}


int EXEC::execPGM(char *pgm, Parameters *params, char *_stdin, char *_stdout, char *_stderr) {
  pid_t childPID;
  char *region,*time;
  unsigned long memLimit;
  unsigned long cpuLimit;

  childPID = fork();

  if (childPID < 0) {
    // Error
    return(errno);
  } else
  if (childPID == 0) {
    // Child
    thisEXEC = this;

    memLimit = context->memLimit;
    if ((region = params->getValue("REGION",0)) != NULL) {
      int l = strlen(region);
      char dim = region[l-1];
      region[l-1] = 0x00;
      if (toupper(dim) == 'M') {
        memLimit = atoi(region)*1024;
      } else
      if (toupper(dim) == 'K') {
        memLimit = atoi(region);
      }   
      if (context->memLimit < memLimit) {
        memLimit = context->memLimit;
      }
      region[l-1] = dim;
    } else
    if ((region = params->getValue("REG",0)) != NULL) {
      int l = strlen(region);
      char dim = region[l-1];
      region[l-1] = 0x00;
      if (toupper(dim) == 'M') {
        memLimit = atoi(region)*1024;
      } else
      if (toupper(dim) == 'K') {
        memLimit = atoi(region);
      }   
      if (context->memLimit < memLimit) {
        memLimit = context->memLimit;
      }
      region[l-1] = dim;
    }

    cpuLimit = context->cpuLimit;
    if ((time = params->getValue("TIME",0)) != NULL) {
      int l = strlen(time);
      char dim = time[l-1];
      time[l-1] = 0x00;
      if (toupper(dim) == 'S') {
        cpuLimit = atoi(time);
      } else
      if (toupper(dim) == 'M') {
        cpuLimit = atoi(time)*60;
      } else
      if (toupper(dim) == 'H') {
        cpuLimit = atoi(time)*3600;
      }   
      if ((context->cpuLimit - context->cpuUsed) < cpuLimit) {
        cpuLimit = context->cpuLimit - context->cpuUsed;
      }
      time[l-1] = dim;
    }
    
cout << "Limits: " << cpuLimit << " " << memLimit << endl;
    struct rlimit limits;

    limits.rlim_cur = memLimit;
    limits.rlim_max = memLimit;
    setrlimit(RLIMIT_DATA,&limits);
    limits.rlim_cur = cpuLimit;
    limits.rlim_max = cpuLimit;
    setrlimit(RLIMIT_CPU,&limits);

    if (setuid(context->userId) < 0) return(errno);

    if (_stdin != NULL) {
      if ((stdin = freopen(_stdin,"r",stdin)) == NULL) return(errno);
    } 
    if (_stdout != NULL) {
      if ((stdout = freopen(_stdout,"a",stdout)) == NULL) return(errno);
    } 
    if (_stderr != NULL) {
      if ((stderr = freopen(_stderr,"a",stderr)) == NULL) return(errno);
    } 

    if (chdir(context->workingDir) < 0) {
      context->writeLog(0,"ERROR SETTING WORKING DIRECTORY");
      return errno;
    }

    if (chdir(context->jobId) < 0) {
      context->writeLog(0,"ERROR SETTING JOB SUBDIRECTORY");
      return errno;
    }

    char *argv[5];
    argv[0] = "batchrun";
    argv[1] = pgm;
    argv[2] = context->jobId;
    argv[3] = this->name; // STEP
    argv[4] = pgm;

    int rc = batchrun(5,argv);
    exit(rc);
  } else {
    // Parent
    int status;
    struct rusage usage;

    setpriority(PRIO_PROCESS,childPID,context->priority);
    int rc = wait4(childPID,&status,WUNTRACED,&usage);

    long cpuUsed = usage.ru_stime.tv_sec; // seconds
    long memUsed = usage.ru_idrss; // kBytes 

cout << "Child ended " << rc << " " << (int)WEXITSTATUS(status) << " " << cpuUsed << "s " 
     << memUsed << "kByte " << endl;
‚
    context->cpuUsed = context->cpuUsed + cpuUsed;
    return((int)WEXITSTATUS(status));
  }   
}


int EXEC::equalsType(char *type) {
  if (strcmp(type,"EXEC") == 0) 
    return 1;
  else 
    return 0;
}


int EXEC::execute() {
  JobCard *card,*prevJobCard,*procCard = NULL,*ddCard = NULL;
  char *pgm,*proc,*_stdin,*_stdout,*_stderr;
  int condCode = 0;
  DataSetDef *def;
  
  context->writeLog(sourceLineNumber,sourceLines[0]);
  for (int i = 1; i < sourceLineCount; i++) {
    context->writeLog(0,sourceLines[i]);
  }

  Parameters *params = this->params->getRuntimeParams(parameterOverrides,context->getGlobalParams(),this->name);
  this->runtimeParams = params;
  params->toString();
  cout << endl;

  context->push(this);

  if ((pgm = params->getValue("PGM",0)) != NULL) {
    card = firstSubCard;
    prevJobCard = NULL;
    while (card != NULL) {
      card->setRuntimeContext(context);
      card->execute();
      if ((card->name[0] == 0x00) && (prevJobCard !=NULL)) {
        def = ((DD*)prevJobCard)->getDataSetDef();
        if (def != NULL) def->setNext(((DD*)card)->getDataSetDef());
      }
      prevJobCard = card;
      card = card->nextJobCard;
    }

    // Execute program...
    _stdin = NULL;
    if ((ddCard = this->getSubCard("STDIN")) != NULL) {
      _stdin = ddCard->getFileName();
    }
    _stdout = NULL;
    if ((ddCard = this->getSubCard("STDOUT")) != NULL) {
      _stdout = ddCard->getFileName();
    }
    _stderr = NULL;
    if ((ddCard = this->getSubCard("STDERR")) != NULL) {
      _stderr = ddCard->getFileName();
    }

    condCode = execPGM(pgm,params,_stdin,_stdout,_stderr);

    card = firstSubCard;
    while (card != NULL) {
      card->executeSpecial(params,NULL);
      card->setParameterOverrides(NULL);
      card = card->nextJobCard;
    }
  } else 
  if ((proc = params->getValue("PROC",0)) != NULL) {
    condCode = execPROC(proc,params);
  } else
  if ((proc = params->getValue(0)) != NULL) {
    condCode = execPROC(proc,params);
  }

  context->pop();
  this->runtimeParams = NULL;  
  delete params;

  return condCode;
}
