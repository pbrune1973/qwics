/*******************************************************************************************/
/*   QWICS Server COBOL load module executor                                               */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 07.11.2019                                  */
/*                                                                                         */
/*   Copyright (C) 2018, 2019 by Philipp Brune  Email: Philipp.Brune@qwics.org             */
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
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <dlfcn.h>

#include <libcob.h>
#include <setjmp.h>
#include "config.h"
#include "db/conpool.h"
#include "env/envconf.h"
#include "msg/queueman.h"
#include "shm/shmtpm.h"
#include "enqdeq/enqdeq.h"

#ifdef __APPLE__
#include "macosx/fmemopen.h"
#endif

// Keys for thread specific data
pthread_key_t connKey;
pthread_key_t childfdKey;
pthread_key_t cmdbufKey;
pthread_key_t cmdStateKey;
pthread_key_t cobFieldKey;
pthread_key_t xctlStateKey;
pthread_key_t retrieveStateKey;
pthread_key_t xctlParamsKey;
pthread_key_t eibbufKey;
pthread_key_t linkAreaKey;
pthread_key_t linkAreaPtrKey;
pthread_key_t commAreaKey;
pthread_key_t commAreaPtrKey;
pthread_key_t areaModeKey;
pthread_key_t linkStackKey;
pthread_key_t linkStackPtrKey;
pthread_key_t memParamsStateKey;
pthread_key_t memParamsKey;
pthread_key_t twaKey;
pthread_key_t tuaKey;
pthread_key_t allocMemKey;
pthread_key_t allocMemPtrKey;
pthread_key_t respFieldsKey;
pthread_key_t respFieldsStateKey;
pthread_key_t taskLocksKey;

// Callback function declared in libcob
extern int (*performEXEC)(char*, void*);
extern void* (*resolveCALL)(char*);

// SQLCA
cob_field *sqlcode = NULL;
char currentMap[9];

// Making COBOl thread safe
int runningModuleCnt = 0;
char runningModules[500][9];
pthread_mutex_t moduleMutex;
pthread_cond_t  waitForModuleChange;

pthread_mutex_t sharedMemMutex;

int mem_pool_size = -1;
#define MEM_POOL_SIZE GETENV_NUMBER(mem_pool_size,"QWICS_MEM_POOL_SIZE",100)

char *jsDir = NULL;
char *loadmodDir = NULL;
char *connectStr = NULL;

void **sharedAllocMem;
int *sharedAllocMemLen;
int *sharedAllocMemPtr = NULL;

char paramsBuf[10][256];

unsigned char *cwa;
jmp_buf taskState;

jmp_buf *condHandler[100];


void cm(int res) {
    if (res != 0) {
      fprintf(stderr,"%s%d\n","Mutex operation failed: ",res);
    }
}

// Synchronizing COBOL module execution
void startModule(char *progname) {
  int found = 0;
  cm(pthread_mutex_lock(&moduleMutex));
  do {
    found = 0;
    for (int i = 0; i < runningModuleCnt; i++) {
      if (strcmp(runningModules[i],progname) == 0) {
        found = 1;
        break;
      }
    }
    if (found == 1) {
        pthread_cond_wait(&waitForModuleChange,&moduleMutex);
    }
  } while (found == 1);

  if (runningModuleCnt < 500) {
      sprintf(runningModules[runningModuleCnt],"%s",progname);
      runningModuleCnt++;
  }
  cm(pthread_mutex_unlock(&moduleMutex));
}


void endModule(char *progname) {
  int found = 0;
  cm(pthread_mutex_lock(&moduleMutex));
  for (int i = 0; i < runningModuleCnt; i++) {
    if (found == 1) {
      sprintf(runningModules[i-1],"%s",runningModules[i]);
    }
    if ((found == 0) && (strcmp(runningModules[i],progname) == 0)) {
      found = 1;
    }
  }
  if (runningModuleCnt > 0) {
    runningModuleCnt--;
  }
  cm(pthread_mutex_unlock(&moduleMutex));
  pthread_cond_broadcast(&waitForModuleChange);
}


void writeJson(char *map, char *mapset, int childfd) {
    int n = 0, l = strlen(map), found = 0, brackets = 0;
    write(childfd,"JSON=",5);
    char jsonFile[255];
    sprintf(jsonFile,"%s%s%s%s",GETENV_STRING(jsDir,"QWICS_JSDIR","../copybooks"),"/",mapset,".js");
    FILE *js = fopen(jsonFile,"rb");
    if (js != NULL) {
        while (1) {
            char c = fgetc(js);
            if (feof(js)) {
                break;
            }
            if (found == 0) {
                if (map[n] == c) {
                    n++;
                } else {
                    n = 0;
                }
                if (n == l) {
                    found = 1;
                }
            }
            if (found == 1) {
                if (c == '{') {
                    found = 2;
                }
            }
            if (found == 2) {
                write(childfd,&c,1);
                if (c == '{') {
                    brackets++;
                }
                if (c == '}') {
                    brackets--;
                }
                if (brackets <= 0) {
                    break;
                }
            }
        }
        fclose(js);
    }
    write(childfd,"\n",1);
}

// Adjust pading and scale for COBOL numeric data
char* convertNumeric(char *val, int digits, int scale, char *buf) {
    char *sep = strchr(val,'.');
    char *pos = sep;
    if (sep == NULL) {
      pos = val + strlen(val) - 1;
    }
    pos++;
    int i = 0;
    while (((*pos) != 0x00) && (i < scale)) {
      buf[digits-scale+i] = *pos;
      i++;
      pos++;
    }
    // Pad to the right with 0
    while (i < scale) {
      buf[digits-scale+i] = '0';
      i++;
    }

    pos = sep;
    if (sep == NULL) {
      pos = val + strlen(val);
    }
    pos--;
    i = digits-scale-1;
    while ((pos >= val) && (i >= 0)) {
      buf[i] = *pos;
      i--;
      pos--;
    }
    // Pad to the left with 0
    while (i >= 0) {
      buf[i] = '0';
      i--;
    }
    buf[digits] = 0x00;
    return buf;
}

// Callback handler for EXEC statements
int processCmd(char *cmd, cob_field **outputVars) {
    char *pos;
    if ((pos=strstr(cmd,"EXEC SQL")) != NULL) {
        char *sql = (char*)pos+9;
        PGconn *conn = (PGconn*)pthread_getspecific(connKey);
        if (sqlcode != NULL) {
            cob_set_int(sqlcode,0);
        }
        if (outputVars[0] == NULL) {
            int r = execSQL(conn, sql);
            if (r == 0) {
                if (sqlcode != NULL) {
                    cob_set_int(sqlcode,1);
                }
            }
        } else {
            // Query returns data
            PGresult *res = execSQLQuery(conn, sql);
            if (res != NULL) {
                int i = 0;
                int cols = PQnfields(res);
                int rows = PQntuples(res);
                if (rows > 0) {
                    while (outputVars[i] != NULL) {
                        if (i < cols) {
                            if (outputVars[i]->attr->type == COB_TYPE_NUMERIC) {
                              char buf[256];
                              cob_put_picx(outputVars[i]->data,outputVars[i]->size,
                                  convertNumeric(PQgetvalue(res, 0, i),
                                                 outputVars[i]->attr->digits,
                                                 outputVars[i]->attr->scale,buf));
                            } else {
                              cob_put_picx(outputVars[i]->data,outputVars[i]->size,PQgetvalue(res, 0, i));
                            }
                        }
                        i++;
                    }
                } else {
                    if (sqlcode != NULL) {
                        cob_set_int(sqlcode,1);
                    }
                }
                PQclear(res);
            } else {
                if (sqlcode != NULL) {
                    cob_set_int(sqlcode,1);
                }
            }
        }
        printf("%s\n",sql);
    }
    return 1;
}


void initMain() {
  void **allocMem = (void**)pthread_getspecific(allocMemKey);
  int *allocMemPtr = (int*)pthread_getspecific(allocMemPtrKey);
  (*allocMemPtr) = 0;
}


void *getmain(int length, int shared) {
  void **allocMem;
  int *allocMemPtr;
  if (shared == 0) {
    allocMem = (void**)pthread_getspecific(allocMemKey);
    allocMemPtr = (int*)pthread_getspecific(allocMemPtrKey);
  } else {
    cm(pthread_mutex_lock(&sharedMemMutex));
    allocMem = sharedAllocMem;
    allocMemPtr = sharedAllocMemPtr;
  }
  int i = 0;
  for (i = 0; i < (*allocMemPtr); i++) {
      if (allocMem[i] == NULL) {
        break;
      }
  }
  if (i < MEM_POOL_SIZE) {
    void *p = NULL;
    if (shared) {
      p = sharedMalloc(0,length);
      sharedAllocMemLen[i] = length;
    } else {
      p = malloc(length);
    }
    if (p != NULL) {
      allocMem[i] = p;
      if (i == (*allocMemPtr)) {
        (*allocMemPtr)++;
      }
    }
    printf("%s %d %x %d\n","getmain",length,(unsigned int)p,shared);
    if (shared) {
      cm(pthread_mutex_unlock(&sharedMemMutex));
    }
    return p;
  }
  if (shared) {
    cm(pthread_mutex_unlock(&sharedMemMutex));
  }
  return NULL;
}


void freemain(void *p) {
  void **allocMem = (void**)pthread_getspecific(allocMemKey);
  int *allocMemPtr = (int*)pthread_getspecific(allocMemPtrKey);
  for (int i = 0; i < (*allocMemPtr); i++) {
      if ((p != NULL) && (allocMem[i] == p)) {
          printf("%s %x\n","freemain",(unsigned int)p);
          free(allocMem[i]);
          allocMem[i] = NULL;
          if (i == (*allocMemPtr)-1) {
            (*allocMemPtr)--;
          }
          return;
      }
  }
  // Free shared mem
  cm(pthread_mutex_lock(&sharedMemMutex));
  allocMem = sharedAllocMem;
  allocMemPtr = sharedAllocMemPtr;
  for (int i = 0; i < (*allocMemPtr); i++) {
      if ((p != NULL) && (allocMem[i] == p)) {
          printf("%s %x\n","freemain shared",(unsigned int)p);
          sharedFree(allocMem[i],sharedAllocMemLen[i]);
          allocMem[i] = NULL;
          if (i == (*allocMemPtr)-1) {
            (*allocMemPtr)--;
          }
          break;
      }
  }
  cm(pthread_mutex_unlock(&sharedMemMutex));
}


void clearMain() {
  void **allocMem = (void**)pthread_getspecific(allocMemKey);
  int *allocMemPtr = (int*)pthread_getspecific(allocMemPtrKey);
  // Clean up, avoid memory leaks
  for (int i = 0; i < (*allocMemPtr); i++) {
      if (allocMem[i] != NULL) {
          free(allocMem[i]);
      }
  }
  (*allocMemPtr) = 0;
}


// Execute SQL pure instruction
void execSql(char *sql, void *fd) {
    char response[1024];
    pthread_setspecific(childfdKey, fd);
    if (strstr(sql,"BEGIN")) {
        PGconn *conn = getDBConnection();
        pthread_setspecific(connKey, (void*)conn);
        return;
    }
    if (strstr(sql,"COMMIT")) {
        PGconn *conn = (PGconn*)pthread_getspecific(connKey);
        if (strstr(sql,"PREPARED")) {
            execSQLCmd(conn, sql);
        }
        returnDBConnection(conn, 1);
        return;
    }
    if (strstr(sql,"ROLLBACK")) {
        PGconn *conn = (PGconn*)pthread_getspecific(connKey);
        if (strstr(sql,"PREPARED")) {
            execSQLCmd(conn, sql);
        }
        returnDBConnection(conn, 0);
        return;
    }
    if (strstr(sql,"SELECT") || strstr(sql,"FETCH") || strstr(sql,"select") || strstr(sql,"fetch")) {
        PGconn *conn = (PGconn*)pthread_getspecific(connKey);
        PGresult *res = execSQLQuery(conn, sql);
        if (res != NULL) {
            int i,j;
            int cols = PQnfields(res);
            int rows = PQntuples(res);
            sprintf(response,"%s\n","OK");
            write(*((int*)fd),&response,strlen(response));
            sprintf(response,"%d\n",cols);
            write(*((int*)fd),&response,strlen(response));
            for (j = 0; j < cols; j++) {
                sprintf(response,"%s\n",PQfname(res,j));
                write(*((int*)fd),&response,strlen(response));
            }
            sprintf(response,"%d\n",rows);
            write(*((int*)fd),&response,strlen(response));
            for (i = 0; i < rows; i++) {
                for (j = 0; j < cols; j++) {
                    sprintf(response,"%s\n",PQgetvalue(res, i, j));
                    write(*((int*)fd),&response,strlen(response));
                }
            }
            PQclear(res);
        } else {
            sprintf(response,"%s\n","ERROR");
            write(*((int*)fd),&response,strlen(response));
        }
        return;
    }
    PGconn *conn = (PGconn*)pthread_getspecific(connKey);
    char *r = execSQLCmd(conn, sql);
    if (r == NULL) {
        sprintf(response,"%s\n","ERROR");
        write(*((int*)fd),&response,strlen(response));
    } else {
        sprintf(response,"%s%s\n","OK:",r);
        write(*((int*)fd),&response,strlen(response));
    }
}


int setJmpAbend(int *errcond, char *bufVar) {
  jmp_buf *h = condHandler[*errcond];
  if (h == NULL) {
      h = malloc(sizeof(jmp_buf));
      condHandler[*errcond] = h;
  }
  memcpy(h,bufVar,sizeof(jmp_buf));
  return 0;
}


void abend(int resp, int resp2) {
  fprintf(stderr,"%s %d %s %d\n","ABEND RESP=",resp," RESP2=",resp2);
  jmp_buf *h = condHandler[resp];
  if (h != NULL) {
    longjmp(*h,1);
  } else {
    longjmp(taskState,1);
  }
}


// Execute COBOL loadmod in transaction
void execLoadModule(char *name, int mode) {
    int (*loadmod)();
    char fname[255];
    char response[1024];
    int childfd = *((int*)pthread_getspecific(childfdKey));
    char *commArea = (char*)pthread_getspecific(commAreaKey);

    #ifdef __APPLE__
    sprintf(fname,"%s%s%s%s",GETENV_STRING(loadmodDir,"QWICS_LOADMODDIR","../loadmod"),"/",name,".dylib");
    #else
    sprintf(fname,"%s%s%s%s",GETENV_STRING(loadmodDir,"QWICS_LOADMODDIR","../loadmod"),"/",name,".so");
    #endif
    void* sdl_library = dlopen(fname, RTLD_LAZY);
    if (sdl_library == NULL) {
        sprintf(response,"%s%s%s\n","ERROR: Load module ",fname," not found!");
        if (mode == 0) {
            write(childfd,&response,strlen(response));
        }
        printf("%s",response);
    } else {
        dlerror();
        *(void**)(&loadmod) = dlsym(sdl_library,name);
        char *error;
        if ((error = dlerror()) != NULL)  {
            sprintf(response,"%s%s\n","ERROR: ",error);
            if (mode == 0) {
                write(childfd,&response,strlen(response));
            }
            printf("%s",response);
            if (mode == 1) {
              abend(27,1);
            }
        } else {
            if (mode == 0) {
                sprintf(response,"%s\n","OK");
                write(childfd,&response,strlen(response));
            }
#ifndef _USE_ONLY_PROCESSES_
            startModule(name);
#endif
            if (mode == 0) {
              if (setjmp(taskState) == 0) {
                (*loadmod)(commArea);
              }
            } else {
              (*loadmod)(commArea);
            }
#ifndef _USE_ONLY_PROCESSES_
            endModule(name);
#endif
            if (mode == 0) {
                sprintf(response,"\n%s\n","STOP");
                write(childfd,&response,strlen(response));
            }
        }
        dlclose(sdl_library);
    }
}


void readLine(char *buf, int childfd) {
  buf[0] = 0x00;
  char c = 0x00;
  int pos = 0;
  while (c != '\n') {
      int n = read(childfd,&c,1);
      if ((n == 1) && (pos < 2047) && (c != '\n') && (c != '\r') && (c != '\'')) {
          buf[pos] = c;
          pos++;
      }
  }
  buf[pos] = 0x00;
}


int execCallback(char *cmd, void *var) {
    int childfd = *((int*)pthread_getspecific(childfdKey));
    char *cmdbuf = (char*)pthread_getspecific(cmdbufKey);
    int *cmdState = (int*)pthread_getspecific(cmdStateKey);
    cob_field **outputVars = (cob_field**)pthread_getspecific(cobFieldKey);
    char *end = &cmdbuf[strlen(cmdbuf)];
    int *xctlState = (int*)pthread_getspecific(xctlStateKey);
    int *retrieveState = (int*)pthread_getspecific(retrieveStateKey);
    char **xctlParams = (char**)pthread_getspecific(xctlParamsKey);
    char *eibbuf = (char*)pthread_getspecific(eibbufKey);
    char *linkArea = (char*)pthread_getspecific(linkAreaKey);
    int *linkAreaPtr = (int*)pthread_getspecific(linkAreaPtrKey);
    char *commArea = (char*)pthread_getspecific(commAreaKey);
    int *commAreaPtr = (int*)pthread_getspecific(commAreaPtrKey);
    int *areaMode = (int*)pthread_getspecific(areaModeKey);
    char *linkStack = (char*)pthread_getspecific(linkStackKey);
    int *linkStackPtr = (int*)pthread_getspecific(linkStackPtrKey);
    void **memParams = (void**)pthread_getspecific(memParamsKey);
    int *memParamsState = (int*)pthread_getspecific(memParamsStateKey);
    char *twa = (char*)pthread_getspecific(twaKey);
    char *tua = (char*)pthread_getspecific(tuaKey);
    int *respFieldsState = (int*)pthread_getspecific(respFieldsStateKey);
    void **respFields = (void**)pthread_getspecific(respFieldsKey);
    struct taskLock *taskLocks = (struct taskLock *)pthread_getspecific(taskLocksKey);

    // printf("%s %s %d %d %x\n","execCallback",cmd,*cmdState,*memParamsState,var);

    if (strstr(cmd,"SET SQLCODE") && (var != NULL)) {
        sqlcode = var;
        return 1;
    }
    if (strstr(cmd,"SET EIBCALEN") && ((*linkStackPtr) == 0)) {
        cob_field *cobvar = (cob_field*)var;
//        cobvar->data = (unsigned char*)(eibbuf+24);
        // Read in client response value
        char buf[2048];
        buf[0] = 0x00;
        char c = 0x00;
        int pos = 0;
        while (c != '\n') {
            int n = read(childfd,&c,1);
            if ((n == 1) && (pos < 2047) && (c != '\n') && (c != '\r') && (c != '\'')) {
                buf[pos] = c;
                pos++;
            }
        }
        buf[pos] = 0x00;
        long val = (long)atol(buf);
        cob_put_u64_compx(val,cobvar->data,(size_t)cobvar->size);
        return 1;
    }
    if (strstr(cmd,"SET EIBAID") && ((*linkStackPtr) == 0)) {
        // Reset link area ptr before SETLx
        (*linkAreaPtr) = 0;
        (*commAreaPtr) = 0;
        (*areaMode) = 0;
        // Handle EIBAID
        cob_field *cobvar = (cob_field*)var;
//        cobvar->data = (unsigned char*)(eibbuf+26);
        // Read in client response value
        char buf[2048];
        buf[0] = 0x00;
        char c = 0x00;
        int pos = 0;
        while (c != '\n') {
            int n = read(childfd,&c,1);
            if ((n == 1) && (pos < 2047) && (c != '\n') && (c != '\r') && (c != '\'')) {
                buf[pos] = c;
                pos++;
            }
        }
        buf[pos] = 0x00;
        cob_put_picx(cobvar->data,(size_t)cobvar->size,buf);
        return 1;
    }
    if (strstr(cmd,"SET DFHEIBLK") && ((*linkStackPtr) == 0)) {
        cob_field *cobvar = (cob_field*)var;
        cobvar->data = (unsigned char*)eibbuf;
        return 1;
    }
    if (strstr(cmd,"SETL1 1") || strstr(cmd,"SETL0 1") || strstr(cmd,"SETL0 77")) {
        (*areaMode) = 0;
    }
    if (strstr(cmd,"DFHCOMMAREA")) {
        (*areaMode) = 1;
    }
    if (strstr(cmd,"SETL0")) {
        cob_field *cobvar = (cob_field*)var;
        if ((*areaMode) == 0) {
            cobvar->data = (unsigned char*)&linkArea[*linkAreaPtr];
            (*linkAreaPtr) += (size_t)cobvar->size;
        }
        else {
            cobvar->data = (unsigned char*)&commArea[*commAreaPtr];
            (*commAreaPtr) += (size_t)cobvar->size;
        }
        /*
        printf("%s%s%d%s%d%s%d%s%d\n",cmd," ",(long)cobvar->data," ",(int)cobvar->size," ",(*linkAreaPtr)," ",                                      (*commAreaPtr));
        */
    }

    if (strcmp(cmd,"CICS") == 0) {
        cmdbuf[0] = 0x00;
        (*cmdState) = -1;
        return 1;
    }

    if ((*cmdState) < 0) {
        if (strcmp(cmd,"SEND") == 0) {
            sprintf(cmdbuf,"%s%s",cmd,"\n");
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
            (*respFieldsState) = 0;
            respFields[0] = NULL;
            respFields[1] = NULL;
            return 1;
        }
        if (strcmp(cmd,"RECEIVE") == 0) {
            sprintf(cmdbuf,"%s%s",cmd,"\n");
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
            (*cmdState) = -2;
            (*memParamsState) = 0;
            (*respFieldsState) = 0;
            *((int*)memParams[0]) = -1;
            respFields[0] = NULL;
            respFields[1] = NULL;
            return 1;
        }
        if (strcmp(cmd,"XCTL") == 0) {
            sprintf(cmdbuf,"%s%s",cmd,"\n");
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
            (*cmdState) = -3;
            (*xctlState) = 0;
            (*respFieldsState) = 0;
            respFields[0] = NULL;
            respFields[1] = NULL;
            return 1;
        }
        if (strcmp(cmd,"RETRIEVE") == 0) {
            sprintf(cmdbuf,"%s%s",cmd,"\n");
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
            (*cmdState) = -4;
            (*retrieveState) = 0;
            (*respFieldsState) = 0;
            respFields[0] = NULL;
            respFields[1] = NULL;
            return 1;
        }
        if (strcmp(cmd,"LINK") == 0) {
            sprintf(cmdbuf,"%s%s",cmd,"\n");
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
            (*cmdState) = -5;
            (*xctlState) = 0;
            (*respFieldsState) = 0;
            respFields[0] = NULL;
            respFields[1] = NULL;
            return 1;
        }
        if ((strcmp(cmd,"GETMAIN") == 0) || (strcmp(cmd,"GETMAIN64") == 0)) {
            sprintf(cmdbuf,"%s%s",cmd,"\n");
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
            (*cmdState) = -6;
            (*memParamsState) = 0;
            memParams[3] = NULL;
            (*respFieldsState) = 0;
            respFields[0] = NULL;
            respFields[1] = NULL;
            return 1;
        }
        if ((strcmp(cmd,"FREEMAIN") == 0) || (strcmp(cmd,"FREEMAIN64") == 0)) {
            sprintf(cmdbuf,"%s%s",cmd,"\n");
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
            (*cmdState) = -7;
            (*memParamsState) = 0;
            memParams[2] = (void*)0; // SHARED
            (*respFieldsState) = 0;
            respFields[0] = NULL;
            respFields[1] = NULL;
            return 1;
        }
        if (strcmp(cmd,"ADDRESS") == 0) {
            sprintf(cmdbuf,"%s%s",cmd,"\n");
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
            (*cmdState) = -8;
            (*memParamsState) = 0;
            (*respFieldsState) = 0;
            respFields[0] = NULL;
            respFields[1] = NULL;
            return 1;
        }
        if (strcmp(cmd,"PUT") == 0) {
            sprintf(cmdbuf,"%s%s",cmd,"\n");
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
            (*cmdState) = -9;
            (*memParamsState) = 0;
            *((int*)memParams[0]) = -1;
            (*respFieldsState) = 0;
            respFields[0] = NULL;
            respFields[1] = NULL;
            return 1;
        }
        if (strcmp(cmd,"GET") == 0) {
            sprintf(cmdbuf,"%s%s",cmd,"\n");
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
            (*cmdState) = -10;
            (*memParamsState) = 0;
            *((int*)memParams[0]) = -1;
            (*respFieldsState) = 0;
            respFields[0] = NULL;
            respFields[1] = NULL;
            return 1;
        }
        if (strcmp(cmd,"ENQ") == 0) {
            sprintf(cmdbuf,"%s%s",cmd,"\n");
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
            (*cmdState) = -11;
            (*memParamsState) = 0;
            (*((int*)memParams[0])) = -1;
            memParams[2] = NULL;
            memParams[3] = NULL;
            memParams[4] = NULL;
            (*respFieldsState) = 0;
            respFields[0] = NULL;
            respFields[1] = NULL;
            return 1;
        }
        if (strcmp(cmd,"DEQ") == 0) {
            sprintf(cmdbuf,"%s%s",cmd,"\n");
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
            (*cmdState) = -12;
            (*memParamsState) = 0;
            (*((int*)memParams[0])) = -1;
            memParams[2] = NULL;
            memParams[3] = NULL;
            memParams[4] = NULL;
            (*respFieldsState) = 0;
            respFields[0] = NULL;
            respFields[1] = NULL;
            return 1;
        }
        if (strcmp(cmd,"SYNCPOINT") == 0) {
            sprintf(cmdbuf,"%s%s",cmd,"\n");
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
            (*cmdState) = -13;
            (*memParamsState) = 0;
            (*respFieldsState) = 0;
            respFields[0] = NULL;
            respFields[1] = NULL;
            return 1;
        }
        if (strcmp(cmd,"WRITEQ") == 0) {
            sprintf(cmdbuf,"%s%s",cmd,"\n");
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
            (*cmdState) = -14;
            (*memParamsState) = 0;
            *((int*)memParams[0]) = -1;
            memParams[3] = NULL;
            (*respFieldsState) = 0;
            respFields[0] = NULL;
            respFields[1] = NULL;
            return 1;
        }
        if (strcmp(cmd,"READQ") == 0) {
            sprintf(cmdbuf,"%s%s",cmd,"\n");
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
            (*cmdState) = -15;
            (*memParamsState) = 0;
            *((int*)memParams[0]) = -1;
            memParams[3] = NULL;
            (*respFieldsState) = 0;
            respFields[0] = NULL;
            respFields[1] = NULL;
            return 1;
        }
        if (strcmp(cmd,"DELETEQ") == 0) {
            sprintf(cmdbuf,"%s%s",cmd,"\n");
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
            (*cmdState) = -16;
            (*memParamsState) = 0;
            *((int*)memParams[0]) = -1;
            (*respFieldsState) = 0;
            respFields[0] = NULL;
            respFields[1] = NULL;
            return 1;
        }
        if (strcmp(cmd,"ABEND") == 0) {
            sprintf(cmdbuf,"%s%s",cmd,"\n");
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
            (*cmdState) = -17;
            (*respFieldsState) = 0;
            respFields[0] = NULL;
            respFields[1] = NULL;
            return 1;
        }
        if ((strcmp(cmd,"ASKTIME") == 0) ||
            (strcmp(cmd,"INQUIRE") == 0) ||
            (strcmp(cmd,"ASSIGN") == 0) ||
            (strcmp(cmd,"FORMATTIME") == 0)) {
            sprintf(cmdbuf,"%s%s",cmd,"\n");
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
            (*cmdState) = -18; // General read only data cmd
            (*memParamsState) = 0;
            *((int*)memParams[0]) = -1;
            (*respFieldsState) = 0;
            respFields[0] = NULL;
            respFields[1] = NULL;
            return 1;
        }
        if ((strcmp(cmd,"START") == 0) ||
            (strcmp(cmd,"CANCEL") == 0)) {
            sprintf(cmdbuf,"%s%s",cmd,"\n");
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
            (*cmdState) = -19; // Call other transactions
            (*memParamsState) = 0;
            *((int*)memParams[0]) = -1;
            memParams[1] = NULL;
            memParams[2] = NULL;
            (*respFieldsState) = 0;
            respFields[0] = NULL;
            respFields[1] = NULL;
            return 1;
        }

        if (strstr(cmd,"END-EXEC")) {
            int resp = 0;
            int resp2 = 0;
            cmdbuf[0] = 0x00;
            outputVars[0] = NULL; // NULL terminated list
            write(childfd,"\n",1);
            if (((*cmdState) == -2) && ((*memParamsState) >= 1)) {
                int len = *((int*)memParams[0]);
                cob_field *cobvar = (cob_field*)memParams[1];
                int i,l;
                if ((len >= 0) && (len <= cobvar->size)) {
                  l = len;
                } else {
                  l = cobvar->size;
                }
                char c;
                i = 0;
                while (i < l) {
                    int n = read(childfd,&c,1);
                    if (n == 1) {
                        cobvar->data[i] = c;
                        i++;
                    }
                }
                while (i < cobvar->size) {
                  int n = read(childfd,&c,1);
                  if (n == 1) {
                      i++;
                  }
                }
                char buf[2048];
                readLine((char*)&buf,childfd);
                resp = atoi(buf);
                readLine((char*)&buf,childfd);
                resp2 = atoi(buf);
                if (resp > 0) {
                  abend(resp,resp2);
                }
            }
            if (((*cmdState) == -3) && ((*xctlState) >= 1)) {
                // XCTL
                (*xctlState) = 0;
                (*cmdState) = 0;
                //printf("%s%s\n","XCTL ",xctlParams[0]);
                execLoadModule(xctlParams[0],1);
            }
            if (((*cmdState) == -4) && ((*retrieveState) >= 1)) {
                // RETRIEVE
                (*retrieveState) = 0;
            }
            if (((*cmdState) == -5) && ((*xctlState) >= 1)) {
                // LINK
                (*xctlState) = 0;
                (*cmdState) = 0;
                sprintf(&(linkStack[9*(*linkStackPtr)]),"%s",xctlParams[0]);
                if ((*linkStackPtr) < 99) {
                  (*linkStackPtr)++;
                }
                execLoadModule(xctlParams[0],1);
            }
            if (((*cmdState) == -6) && ((*memParamsState) >= 1)) {
                cob_field *cobvar = (cob_field*)memParams[1];
                if (*((int*)memParams[0]) < 1) {
                  resp = 22;
                }
                (*((unsigned char**)cobvar->data)) = (unsigned char*)getmain(*((int*)memParams[0]),(int)memParams[2]);
                if (cobvar->data == NULL) {
                  resp = 22;
                }
                if (resp > 0) {
                  abend(resp,resp2);
                }
                if (memParams[3] != NULL) {
                  // INITIMG
                  for (int i = 0; i < *((int*)memParams[0]); i++) {
                    (*((unsigned char**)cobvar->data))[i] = ((char*)memParams[3])[0];
                  }
                }
            }
            if (((*cmdState) == -7) && ((*memParamsState) >= 1)) {
                freemain(memParams[1]);
            }
            if (((*cmdState) == -9) && ((*memParamsState) >= 1)) {
                int len = *((int*)memParams[0]);
                cob_field *cobvar = (cob_field*)memParams[1];
                int i,l;
                if ((len >= 0) && (len <= cobvar->size)) {
                  l = len;
                } else {
                  l = cobvar->size;
                }
                write(childfd,cobvar->data,l);
                if (l < len) {
                  char zero[1];
                  zero[0] = 0x00;
                  for (i = l; i < len; i++) {
                    write(childfd,&zero,1);
                  }
                }
                char buf[2048];
                readLine((char*)&buf,childfd);
                resp = atoi(buf);
                readLine((char*)&buf,childfd);
                resp2 = atoi(buf);
                write(childfd,"\n",1);
                write(childfd,"\n",1);
            }
            if (((*cmdState) == -10) && ((*memParamsState) >= 1)) {
                int len = *((int*)memParams[0]);
                cob_field *cobvar = (cob_field*)memParams[1];
                int i,l;
                if ((len >= 0) && (len <= cobvar->size)) {
                  l = len;
                } else {
                  l = cobvar->size;
                }
                char c;
                i = 0;
                while (i < l) {
                    int n = read(childfd,&c,1);
                    if (n == 1) {
                        cobvar->data[i] = c;
                        i++;
                    }
                }
                while (i < len) {
                  int n = read(childfd,&c,1);
                  if (n == 1) {
                      i++;
                  }
                }
                char buf[2048];
                readLine((char*)&buf,childfd);
                resp = atoi(buf);
                readLine((char*)&buf,childfd);
                resp2 = atoi(buf);
            }
            if (((*cmdState) == -11) && ((*memParamsState) >= 1)) {
                int len = *((int*)memParams[0]);
                cob_field *cobvar = (cob_field*)memParams[1];
                int type = ((int)memParams[4] == 1) ? 1 : 0;
                int nosuspend = ((int)memParams[2] == 1) ? 1 : 0;
                if (len <= 0) {
                  int r = enq((char*)cobvar,0,nosuspend,type,taskLocks);
                } else {
                  if (len > 255) {
                    resp = 22;
                    resp2 = 1;
                  } else {
                    int r = enq((char*)cobvar->data,len,nosuspend,type,taskLocks);
                    if (r < 0) {
                      resp = 55;
                    }
                  }
                }
            }
            if (((*cmdState) == -12) && ((*memParamsState) >= 1)) {
                int len = *((int*)memParams[0]);
                cob_field *cobvar = (cob_field*)memParams[1];
                int type = ((int)memParams[4] == 1) ? 1 : 0;
                if (len <= 0) {
                  deq((char*)cobvar,0,type,taskLocks);
                } else {
                  if (len > 255) {
                    resp = 22;
                    resp2 = 1;
                  } else {
                    deq((char*)cobvar->data,len,type,taskLocks);
                  }
                }
            }
            if ((*cmdState) == -13) {
                // SYNCPOINT handling
                char buf[2048];
                buf[0] = 0x00;
                while(strstr(buf,"END-SYNCPOINT") == NULL) {
                  char c = 0x00;
                  int pos = 0;
                  while (c != '\n') {
                    int n = read(childfd,&c,1);
                    if ((n == 1) && (pos < 2047) && (c != '\n') && (c != '\r')) {
                      buf[pos] = c;
                      pos++;
                    }
                  }
                  buf[pos] = 0x00;
                  if (pos > 0) {
                    char *cmd = strstr(buf,"sql");
                    if (cmd) {
                      char *sql = cmd+4;
                      execSql(sql, &childfd);
                    }
                  }
                }
                releaseLocks(UOW, taskLocks);
                if ((strstr(buf,"ROLLBACK") == NULL) || ((*memParamsState) == 1)) {
                  if ((*memParamsState) == 0) {
                    resp = 82;
                  }
                  abend(resp,resp2);
                }
            }
            if (((*cmdState) == -14) && ((*memParamsState) >= 1)) {
                int len = *((int*)memParams[0]);
                cob_field *cobvar = (cob_field*)memParams[1];
                int i,l;
                if ((len >= 0) && (len <= cobvar->size)) {
                  l = len;
                } else {
                  l = cobvar->size;
                }
                write(childfd,cobvar->data,l);
                if (l < len) {
                  char zero[1];
                  zero[0] = 0x00;
                  for (i = l; i < len; i++) {
                    write(childfd,&zero,1);
                  }
                }
                char buf[2048];
                readLine((char*)&buf,childfd);
                int item = atoi(buf);
                if ((memParams[3] != NULL) &&
                    (((cob_field*)memParams[3])->attr->type == COB_TYPE_NUMERIC_BINARY)) {
                  cob_put_u64_compx(item,((cob_field*)memParams[3])->data,2);
                }
                readLine((char*)&buf,childfd);
                resp = atoi(buf);
                readLine((char*)&buf,childfd);
                resp2 = atoi(buf);
                write(childfd,"\n",1);
                write(childfd,"\n",1);
                if (resp > 0) {
                  abend(resp,resp2);
                }
            }
            if (((*cmdState) == -15) && ((*memParamsState) >= 1)) {
                int len = *((int*)memParams[0]);
                cob_field *cobvar = (cob_field*)memParams[1];
                int i,l;
                if ((len >= 0) && (len <= cobvar->size)) {
                  l = len;
                } else {
                  l = cobvar->size;
                }
                char c;
                i = 0;
                while (i < l) {
                    int n = read(childfd,&c,1);
                    if (n == 1) {
                        cobvar->data[i] = c;
                        i++;
                    }
                }
                while (i < len) {
                  int n = read(childfd,&c,1);
                  if (n == 1) {
                      i++;
                  }
                }
                char buf[2048];
                readLine((char*)&buf,childfd);
                int item = atoi(buf);
                if ((memParams[3] != NULL) &&
                    (((cob_field*)memParams[3])->attr->type == COB_TYPE_NUMERIC_BINARY)) {
                  cob_put_u64_compx(item,((cob_field*)memParams[3])->data,2);
                }
                readLine((char*)&buf,childfd);
                resp = atoi(buf);
                readLine((char*)&buf,childfd);
                resp2 = atoi(buf);
                if (resp > 0) {
                  abend(resp,resp2);
                }
            }
            if ((*cmdState) == -16) {
                char buf[2048];
                readLine((char*)&buf,childfd);
                resp = atoi(buf);
                readLine((char*)&buf,childfd);
                resp2 = atoi(buf);
            }
            if ((*cmdState) == -17) {
              abend(resp,resp2);
            }
            if ((*cmdState) == -18) {
                char buf[2048];
                readLine((char*)&buf,childfd);
                resp = atoi(buf);
                readLine((char*)&buf,childfd);
                resp2 = atoi(buf);
            }
            if ((*cmdState) == -19) {
                // Send FROM data
                int len = *((int*)memParams[0]);
                cob_field *cobvar = (cob_field*)memParams[1];
                int i,l;
                if (cobvar != NULL) {
                  if ((len >= 0) && (len <= cobvar->size)) {
                    l = len;
                  } else {
                    l = cobvar->size;
                  }
                  write(childfd,cobvar->data,l);
                }

                char buf[2048];
                readLine((char*)&buf,childfd);
                resp = atoi(buf);
                readLine((char*)&buf,childfd);
                resp2 = atoi(buf);
                if (resp > 0) {
                  abend(resp,resp2);
                }
            }
            if ((*respFieldsState) == 1) {
              cob_put_u64_compx(resp,((cob_field*)respFields[0])->data,4);
            }
            if ((*respFieldsState) == 2) {
              cob_put_u64_compx(resp,((cob_field*)respFields[0])->data,4);
              cob_put_u64_compx(resp2,((cob_field*)respFields[1])->data,4);
            }
            (*cmdState) = 0;
            (*respFieldsState) = 0;
            return 1;
        }
        if ((var == NULL) || strstr(cmd,"'") || strstr(cmd,"MAP") || strstr(cmd,"MAPSET") || strstr(cmd,"DATAONLY") ||
            strstr(cmd,"ERASE") || strstr(cmd,"MAPONLY") || strstr(cmd,"RETURN") || strstr(cmd,"FROM") ||
            strstr(cmd,"INTO") || strstr(cmd,"HANDLE") || strstr(cmd,"CONDITION") || strstr(cmd,"ERROR") ||
            strstr(cmd,"SET") || strstr(cmd,"MAPFAIL") || strstr(cmd,"NOTFND") || strstr(cmd,"ASSIGN") ||
            strstr(cmd,"SYSID") || strstr(cmd,"TRANSID") || strstr(cmd,"COMMAREA") || strstr(cmd,"LENGTH") ||
            strstr(cmd,"CONTROL") || strstr(cmd,"FREEKB") || strstr(cmd,"PROGRAM") || strstr(cmd,"XCTL") ||
            strstr(cmd,"ABEND") || strstr(cmd,"ABCODE") || strstr(cmd,"NODUMP") || strstr(cmd,"LINK") ||
            strstr(cmd,"FLENGTH") || strstr(cmd,"DATA") || strstr(cmd,"DATAPOINTER") || strstr(cmd,"SHARED") ||
            strstr(cmd,"CWA") || strstr(cmd,"TWA") || strstr(cmd,"EIB") || strstr(cmd,"TCTUA") || strstr(cmd,"TCTUALENG") || strstr(cmd,"PUT") || strstr(cmd,"GET") ||
            strstr(cmd,"CONTAINER") || strstr(cmd,"CHANNEL") || strstr(cmd,"BYTEOFFSET") || strstr(cmd,"NODATA-FLENGTH") ||
            strstr(cmd,"INTOCCSID") || strstr(cmd,"INTOCODEPAGE") || strstr(cmd,"CONVERTST") || strstr(cmd,"CCSID") ||
            strstr(cmd,"FROMCCSID") || strstr(cmd,"FROMCODEPAGE") || strstr(cmd,"DATATYPE") ||
            strstr(cmd,"APPEND") || strstr(cmd,"BIT") || strstr(cmd,"CHAR") || strstr(cmd,"CANCEL") ||
            strstr(cmd,"RESP") || strstr(cmd,"RESP2") || strstr(cmd,"RESOURCE") || strstr(cmd,"UOW") ||
            strstr(cmd,"TASK") || strstr(cmd,"NOSUSPEND") || strstr(cmd,"INITIMG") ||
            strstr(cmd,"USERDATAKEY") || strstr(cmd,"CICSDATAKEY") || strstr(cmd,"MAXLIFETIME") ||
            strstr(cmd,"ROLLBACK") || strstr(cmd,"ITEM") || strstr(cmd,"QUEUE") || strstr(cmd,"SYSID") ||
            strstr(cmd,"TS") || strstr(cmd,"TD") || strstr(cmd,"REWRITE") || strstr(cmd,"NEXT") ||
            strstr(cmd,"QNAME") || strstr(cmd,"MAIN") || strstr(cmd,"AUXILIARY") || strstr(cmd,"ABSTIME") ||
            strstr(cmd,"YYMMDD") || strstr(cmd,"YEAR") || strstr(cmd,"TIME") || strstr(cmd,"DDMMYY") ||
            strstr(cmd,"DATESEP") || strstr(cmd,"TIMESEP") || strstr(cmd,"DB2CONN") || strstr(cmd,"CONNECTST") ||
            strstr(cmd,"TRANSID") || strstr(cmd,"REQID") || strstr(cmd,"INTERVAL")) {
            sprintf(end,"%s%s",cmd,"\n");

            if (var != NULL) {
              cob_field *cobvar = (cob_field*)var;
              if (strcmp(cmd,"RESP") == 0) {
                cob_put_u64_compx(0,cobvar->data,4);
                respFields[0] = (void*)cobvar;
                (*respFieldsState) = 1;
              }
              if (strcmp(cmd,"RESP2") == 0) {
                cob_put_u64_compx(0,cobvar->data,4);
                respFields[1] = (void*)cobvar;
                (*respFieldsState) = 2;
              }
            }
            if (((*cmdState) == 2) && ((*memParamsState) == 1)) {
                // RECEIVE INTO LENGTH param value
                (*((int*)memParams[0])) = atoi(cmd);
                (*memParamsState) = 10;
            }
            if ((*cmdState) == -2) {
                (*memParamsState) = 10;

                if (strcmp(cmd,"LENGTH") == 0) {
                    (*memParamsState) = 1;
                }
                if (strcmp(cmd,"INTO") == 0) {
                    (*memParamsState) = 2;
                }
            }
            if (((*cmdState) == -3) && ((*xctlState) == 1)) {
                // XCTL PROGRAM param value
                char *progname = (cmd+1);
                int l = strlen(progname);
                if (l > 9) l = 9;
                int i = l-1;
                while ((i > 0) &&
                       ((progname[i]==' ') || (progname[i]=='\'') ||
                        (progname[i]==10) || (progname[i]==13))) {
                    i--;
                }
                l = i+1;
                if (l > 8) l = 8;
                for (i = 0; i < l; i++) {
                  xctlParams[0][i] = progname[i];
                }
                xctlParams[0][l] = 0x00;
                (*xctlState) = 10;
            }
            if ((*cmdState) == -3) {
                if (strstr(cmd,"PROGRAM")) {
                    (*xctlState) = 1;
                }
            }
            if ((*cmdState) == -4) {
                if (strstr(cmd,"INTO")) {
                    (*retrieveState) = 1;
                }
                if (strstr(cmd,"SET")) {
                    (*retrieveState) = 2;
                }
                if (strstr(cmd,"LENGTH")) {
                    (*retrieveState) = 3;
                }
            }
            if (((*cmdState) == -5) && ((*xctlState) == 1)) {
                // LINK PROGRAM param value
                char *progname = (cmd+1);
                int l = strlen(progname);
                if (l > 9) l = 9;
                int i = l-1;
                while ((i > 0) &&
                       ((progname[i]==' ') || (progname[i]=='\'') ||
                        (progname[i]==10) || (progname[i]==13))) {
                    i--;
                }
                l = i+1;
                if (l > 8) l = 8;
                for (i = 0; i < l; i++) {
                  xctlParams[0][i] = progname[i];
                }
                xctlParams[0][l] = 0x00;
                (*xctlState) = 10;
            }
            if ((*cmdState) == -5) {
              if (strstr(cmd,"PROGRAM")) {
                  (*xctlState) = 1;
              }
              if (strstr(cmd,"COMMAREA")) {
                  (*xctlState) = 2;
              }
            }
            if (((*cmdState) == -6) && ((*memParamsState) == 3)) {
                // GETMAIN INITIMG param value
                char *imgchar = (cmd+1);
                int l = strlen(imgchar);
                if (l > 2) l = 2;
                int i = l-1;
                while ((i > 0) &&
                       ((imgchar[i]==' ') || (imgchar[i]=='\'') ||
                        (imgchar[i]==10) || (imgchar[i]==13))) {
                    i--;
                }
                l = i+1;
                if (l > 1) l = 1;
                memParams[3] = (void*)&paramsBuf[3];
                for (i = 0; i < l; i++) {
                  ((char*)memParams[3])[i] = imgchar[i];
                }
                ((char*)memParams[3])[l] = 0x00;
                (*memParamsState) = 10;
            }
            if (((*cmdState) == -6) && ((*memParamsState) == 2)) {
                // GETMAIN LENGTH/FLENGTH param value
                (*((int*)memParams[0])) = atoi(cmd);
                (*memParamsState) = 10;
            }
            if ((*cmdState) == -6) {
                if (strcmp(cmd,"SET") == 0) {
                    (*memParamsState) = 1;
                }
                if (strstr(cmd,"LENGTH")) {
                    (*memParamsState) = 2;
                }
                if (strstr(cmd,"INITIMG")) {
                    (*memParamsState) = 3;
                }
                if (strstr(cmd,"SHARED")) {
                    memParams[2] = (void*)1;
                }
            }
            if ((*cmdState) == -7) {
                if (strcmp(cmd,"DATA") == 0) {
                    (*memParamsState) = 1;
                }
                if (strcmp(cmd,"DATAPOINTER") == 0) {
                    (*memParamsState) = 2;
                }
            }
            if ((*cmdState) == -8) {
                if (strcmp(cmd,"CWA") == 0) {
                    (*memParamsState) = 1;
                }
                if (strcmp(cmd,"TWA") == 0) {
                    (*memParamsState) = 2;
                }
                if (strcmp(cmd,"TCTUA") == 0) {
                    (*memParamsState) = 3;
                }
                if (strcmp(cmd,"TCTUALENG") == 0) {
                    (*memParamsState) = 4;
                }
                if (strcmp(cmd,"COMMAREA") == 0) {
                    (*memParamsState) = 5;
                }
                if (strcmp(cmd,"EIB") == 0) {
                    (*memParamsState) = 6;
                }
            }
            if (((*cmdState) == -9) && ((*memParamsState) == 1)) {
                // PUT FLENGTH param value
                (*((int*)memParams[0])) = atoi(cmd);
                (*memParamsState) = 10;
            }
            if ((*cmdState) == -9) {
                if (strcmp(cmd,"FLENGTH") == 0) {
                    (*memParamsState) = 1;
                }
                if (strcmp(cmd,"FROM") == 0) {
                    (*memParamsState) = 2;
                }
            }
            if (((*cmdState) == -10) && ((*memParamsState) == 1)) {
                // GET FLENGTH param value
                (*((int*)memParams[0])) = atoi(cmd);
                (*memParamsState) = 10;
            }
            if ((*cmdState) == -10) {
                if (strcmp(cmd,"FLENGTH") == 0) {
                    (*memParamsState) = 1;
                }
                if (strcmp(cmd,"INTO") == 0) {
                    (*memParamsState) = 2;
                }
            }
            if (((*cmdState) == -11) && ((*memParamsState) == 1)) {
                // ENQ RESOURCE
                char *resname = (cmd+1);
                int l = strlen(resname);
                if (l > 256) l = 256;
                int i = l-1;
                while ((i > 0) &&
                       ((resname[i]==' ') || (resname[i]=='\'') ||
                        (resname[i]==10) || (resname[i]==13))) {
                    i--;
                }
                l = i+1;
                if (l > 255) l = 255;
                memParams[1] = (void*)&paramsBuf[1];
                for (i = 0; i < l; i++) {
                  ((char*)memParams[1])[i] = resname[i];
                }
                ((char*)memParams[1])[l] = 0x00;
                (*memParamsState) = 10;
            }
            if (((*cmdState) == -11) && ((*memParamsState) == 2)) {
                // ENQ LENGTH
                (*((int*)memParams[0])) = atoi(cmd);
                (*memParamsState) = 10;
            }
            if ((*cmdState) == -11) {
                // ENQ
                if (strcmp(cmd,"RESOURCE") == 0) {
                    (*memParamsState) = 1;
                }
                if (strcmp(cmd,"LENGTH") == 0) {
                    (*memParamsState) = 2;
                }
                if (strcmp(cmd,"NOSUSPEND") == 0) {
                    memParams[2] = (void*)1;
                }
                if (strcmp(cmd,"UOW") == 0) {
                    memParams[3] = (void*)1;
                }
                if (strcmp(cmd,"TASK") == 0) {
                    memParams[4] = (void*)1;
                }
            }
            if (((*cmdState) == -12) && ((*memParamsState) == 1)) {
                // DEQ RESOURCE
                char *resname = (cmd+1);
                int l = strlen(resname);
                if (l > 256) l = 256;
                int i = l-1;
                while ((i > 0) &&
                       ((resname[i]==' ') || (resname[i]=='\'') ||
                        (resname[i]==10) || (resname[i]==13))) {
                    i--;
                }
                l = i+1;
                if (l > 255) l = 255;
                memParams[1] = (void*)&paramsBuf[1];
                for (i = 0; i < l; i++) {
                  ((char*)memParams[1])[i] = resname[i];
                }
                ((char*)memParams[1])[l] = 0x00;
                (*memParamsState) = 10;
            }
            if (((*cmdState) == -12) && ((*memParamsState) == 2)) {
                // DEQ LENGTH
                (*((int*)memParams[0])) = atoi(cmd);
                (*memParamsState) = 10;
            }
            if ((*cmdState) == -12) {
                // DEQ
                if (strcmp(cmd,"RESOURCE") == 0) {
                    (*memParamsState) = 1;
                }
                if (strcmp(cmd,"LENGTH") == 0) {
                    (*memParamsState) = 2;
                }
                if (strcmp(cmd,"NOSUSPEND") == 0) {
                    memParams[2] = (void*)1;
                }
                if (strcmp(cmd,"UOW") == 0) {
                    memParams[3] = (void*)1;
                }
                if (strcmp(cmd,"TASK") == 0) {
                    memParams[4] = (void*)1;
                }
            }
            if ((*cmdState) == -13) {
                if (strcmp(cmd,"ROLLBACK") == 0) {
                    (*memParamsState) = 1;
                }
            }
            if (((*cmdState) == -14) && ((*memParamsState) == 1)) {
                // WRIEQ LENGTH param value
                (*((int*)memParams[0])) = atoi(cmd);
                (*memParamsState) = 10;
            }
            if ((*cmdState) == -14) {
                if (strcmp(cmd,"LENGTH") == 0) {
                    (*memParamsState) = 1;
                }
                if (strcmp(cmd,"FROM") == 0) {
                    (*memParamsState) = 2;
                }
                if (strcmp(cmd,"QUEUE") == 0) {
                    (*memParamsState) = 3;
                }
                if (strcmp(cmd,"ITEM") == 0) {
                    (*memParamsState) = 4;
                }
                if (strcmp(cmd,"TD") == 0) {
                    memParams[5] = (void*)((long)memParams[5] + 1);
                }
                if (strcmp(cmd,"REWRITE") == 0) {
                    memParams[5] = (void*)((long)memParams[5] + 2);
                }
            }
            if (((*cmdState) == -15) && ((*memParamsState) == 1)) {
                // READQ LENGTH param value
                (*((int*)memParams[0])) = atoi(cmd);
                (*memParamsState) = 10;
            }
            if ((*cmdState) == -15) {
                if (strcmp(cmd,"LENGTH") == 0) {
                    (*memParamsState) = 1;
                }
                if (strcmp(cmd,"INTO") == 0) {
                    (*memParamsState) = 2;
                }
                if (strcmp(cmd,"QUEUE") == 0) {
                    (*memParamsState) = 3;
                }
                if (strcmp(cmd,"ITEM") == 0) {
                    (*memParamsState) = 4;
                }
                if (strcmp(cmd,"TD") == 0) {
                    memParams[5] = (void*)((long)memParams[5] + 1);
                }
                if (strcmp(cmd,"NEXT") == 0) {
                    memParams[5] = (void*)((long)memParams[5] + 2);
                }
            }
            if ((*cmdState) == -16) {
                if (strcmp(cmd,"QUEUE") == 0) {
                    (*memParamsState) = 3;
                }
                if (strcmp(cmd,"TD") == 0) {
                    memParams[5] = (void*)((long)memParams[5] + 1);
                }
            }
            if (((*cmdState) == -18) && ((*memParamsState) == 1)) {
                (*memParamsState) = 0;
            }
            if ((*cmdState) == -18) {
                if (strcmp(cmd,"DATESEP") == 0) {
                    (*memParamsState) = 1;
                }
                if (strcmp(cmd,"TIMESEP") == 0) {
                    (*memParamsState) = 1;
                }
            }
            if (((*cmdState) == -19) && ((*memParamsState) == 1)) {
                // START TRANSID LENGTH param value
                (*((int*)memParams[0])) = atoi(cmd);
                (*memParamsState) = 10;
            }
            if ((*cmdState) == -19) {
                (*memParamsState) = 10;

                if (strcmp(cmd,"LENGTH") == 0) {
                  (*memParamsState) = 1;
                }
                if (strcmp(cmd,"FROM") == 0) {
                  (*memParamsState) = 2;
                }
                if (strcmp(cmd,"REQID") == 0) {
                  (*memParamsState) = 3;
                }
            }
            if (cmdbuf[0] == '\'') {
              // String constant
              write(childfd,"=",1);
            }
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
            if ((*cmdState) == -1) {
                if (strstr(cmd,"MAP=")) {
                    sprintf(currentMap,"%s",(cmd+4));
                }
                if (strstr(cmd,"MAPSET=")) {
                    writeJson(currentMap,(cmd+7),childfd);
                }
            }
        } else {
            if (var != NULL) {
                cob_field *cobvar = (cob_field*)var;
                if ((*cmdState) == -1) {
                    sprintf(end,"%s%s",cmd,"=");
                    end = &cmdbuf[strlen(cmdbuf)];
                    FILE *f = fmemopen(end, 2048-strlen(cmdbuf), "w");
                    if (COB_FIELD_TYPE(cobvar) == COB_TYPE_ALPHANUMERIC) putc('\'',f);
                    if ((cobvar->data[0] != 0) || (COB_FIELD_TYPE(cobvar) == 17)) {
                        display_cobfield(cobvar,f);
                    }
                    if (COB_FIELD_TYPE(cobvar) == COB_TYPE_ALPHANUMERIC) putc('\'',f);
                    putc(0x00,f);
                    fclose(f);
                    write(childfd,cmdbuf,strlen(cmdbuf));
                    write(childfd,"\n",1);
                }
                if (((*cmdState) == -2) && ((*memParamsState) == 0)) {
                    sprintf(end,"%s%s",cmd,"\n");
                    write(childfd,cmdbuf,strlen(cmdbuf));
                    // Read in client response value
                    char buf[2048];
                    buf[0] = 0x00;
                    char c = 0x00;
                    int pos = 0;
                    while (c != '\n') {
                        int n = read(childfd,&c,1);
                        if ((n == 1) && (pos < 2047) && (c != '\n') && (c != '\r') && (c != '\'')) {
                            buf[pos] = c;
                            pos++;
                        }
                    }
                    buf[pos] = 0x00;
                    cob_put_picx(cobvar->data,cobvar->size,buf);
                }
                if (((*cmdState) == -2) && ((*memParamsState) == 2)) {
                    memParams[1] = (void*)cobvar;
                    (*memParamsState) = 10;
                    char str[20];
                    sprintf((char*)&str,"%s\n","SIZE");
                    write(childfd,str,strlen(str));
                    sprintf((char*)&str,"%s%d\n","=",(int)cobvar->size);
                    write(childfd,str,strlen(str));
                }
                if (((*cmdState) == -2) && ((*memParamsState) == 1)) {
                    // WRITEQ LENGTH
                    sprintf(end,"%s%s",cmd,"=");
                    end = &cmdbuf[strlen(cmdbuf)];
                    FILE *f = fmemopen(end, 2048-strlen(cmdbuf), "w");
                    if ((cobvar->data[0] != 0) || (COB_FIELD_TYPE(cobvar) == 17)) {
                        display_cobfield(cobvar,f);
                    }
                    putc(0x00,f);
                    fclose(f);
                    write(childfd,cmdbuf,strlen(cmdbuf));
                    write(childfd,"\n",1);
                    (*((int*)memParams[0])) = atoi(end);
                    (*memParamsState) = 10;
                }
                if ((*cmdState) == -3) {
                    sprintf(end,"%s%s",cmd,"=");
                    end = &cmdbuf[strlen(cmdbuf)];
                    FILE *f = fmemopen(end, 2048-strlen(cmdbuf), "w");
                    if (COB_FIELD_TYPE(cobvar) == COB_TYPE_ALPHANUMERIC) putc('\'',f);
                    if ((cobvar->data[0] != 0) || (COB_FIELD_TYPE(cobvar) == 17)) {
                        display_cobfield(cobvar,f);
                    }
                    if (COB_FIELD_TYPE(cobvar) == COB_TYPE_ALPHANUMERIC) putc('\'',f);
                    putc(0x00,f);
                    fclose(f);
                    write(childfd,cmdbuf,strlen(cmdbuf));
                    write(childfd,"\n",1);
                    if ((*xctlState) == 1) {
                        // XCTL PROGRAM param value
                        char *progname = (cmdbuf+2);
                        int l = strlen(progname);
                        if (l > 9) l = 9;
                        int i = l-1;
                        while ((i > 0) &&
                               ((progname[i]==' ') || (progname[i]=='\'') ||
                                (progname[i]==10) || (progname[i]==13)))
                            i--;
                        progname[i+1] = 0x00;
                        sprintf(xctlParams[0],"%s",progname);
                        (*xctlState) = 10;
                    }
                }
                if ((*cmdState) == -4) {
                    if ((*retrieveState) == 1) {
                      // INTO
                      sprintf(end,"%d",(int)cobvar->size);
                      write(childfd,cmdbuf,strlen(cmdbuf));
                      write(childfd,"\n",1);
                      int i = 0;
                      char c;
                      for (i = 0; i < (size_t)cobvar->size; ) {
                          int n = read(childfd,&c,1);
                          if (n == 1) {
                            cobvar->data[i] = c;
                            i++;
                          }
                      }
                    }
                }
                if ((*cmdState) == -5) {
                    sprintf(end,"%s%s",cmd,"=");
                    end = &cmdbuf[strlen(cmdbuf)];
                    FILE *f = fmemopen(end, 2048-strlen(cmdbuf), "w");
                    if (COB_FIELD_TYPE(cobvar) == COB_TYPE_ALPHANUMERIC) putc('\'',f);
                    if ((cobvar->data[0] != 0) || (COB_FIELD_TYPE(cobvar) == 17)) {
                        display_cobfield(cobvar,f);
                    }
                    if (COB_FIELD_TYPE(cobvar) == COB_TYPE_ALPHANUMERIC) putc('\'',f);
                    putc(0x00,f);
                    fclose(f);
                    write(childfd,cmdbuf,strlen(cmdbuf));
                    write(childfd,"\n",1);
                    if ((*xctlState) == 1) {
                        // LINK PROGRAM param value
                        char *progname = (cmdbuf+2);
                        int l = strlen(progname);
                        if (l > 9) l = 9;
                        int i = l-1;
                        while ((i > 0) &&
                               ((progname[i]==' ') || (progname[i]=='\'') ||
                                (progname[i]==10) || (progname[i]==13)))
                            i--;
                        progname[i+1] = 0x00;
                        sprintf(xctlParams[0],"%s",progname);
                        (*xctlState) = 10;
                    }
                }
                if (((*cmdState) == -5) && ((*xctlState) == 2)) {
                    for (int i = 0; i < cobvar->size; i++) {
                      commArea[i] = cobvar->data[i];
                    }
                    (*xctlState) = 10;
                }
                if (((*cmdState) < -5) &&
                    !(((*cmdState) == -9) && ((*memParamsState) == 1)) &&
                    !(((*cmdState) == -10) && ((*memParamsState) == 1)) &&
                    !(((*cmdState) == -6) && ((*memParamsState) == 2))  &&
                    !(((*cmdState) == -11) && ((*memParamsState) == 2)) &&
                    !(((*cmdState) == -12) && ((*memParamsState) == 2)) &&
                    !(((*cmdState) == -14) && ((*memParamsState) == 1)) &&
                    !(((*cmdState) == -15) && ((*memParamsState) == 1)) &&
                    !(((*cmdState) == -19) && ((*memParamsState) == 1)) &&
                    !(((*cmdState) == -19) && ((*memParamsState) == 3))) {
                    sprintf(end,"%s%s",cmd,"=");
                    end = &cmdbuf[strlen(cmdbuf)];
                    FILE *f = fmemopen(end, 2048-strlen(cmdbuf), "w");
                    if (COB_FIELD_TYPE(cobvar) == COB_TYPE_ALPHANUMERIC) putc('\'',f);
                    if ((cobvar->data[0] != 0) || (COB_FIELD_TYPE(cobvar) == 17)) {
                        display_cobfield(cobvar,f);
                    }
                    if (COB_FIELD_TYPE(cobvar) == COB_TYPE_ALPHANUMERIC) putc('\'',f);
                    putc(0x00,f);
                    fclose(f);
                    write(childfd,cmdbuf,strlen(cmdbuf));
                    write(childfd,"\n",1);
                }
                if (((*cmdState) == -6) && ((*memParamsState) == 1)) {
                  memParams[1] = (void*)cobvar;
                  (*memParamsState) = 10;
                }
                if (((*cmdState) == -6) && ((*memParamsState) == 2)) {
                    // GETMAIN LENGTH/FLENGTH param value
                    sprintf(end,"%s%s",cmd,"=");
                    end = &cmdbuf[strlen(cmdbuf)];
                    FILE *f = fmemopen(end, 2048-strlen(cmdbuf), "w");
                    if ((cobvar->data[0] != 0) || (COB_FIELD_TYPE(cobvar) == 17)) {
                        display_cobfield(cobvar,f);
                    }
                    putc(0x00,f);
                    fclose(f);
                    write(childfd,cmdbuf,strlen(cmdbuf));
                    write(childfd,"\n",1);
                    (*((int*)memParams[0])) = atoi(end);
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -6) && ((*memParamsState) == 3)) {
                  // GETMAIN INITIMG param value
                  if (COB_FIELD_TYPE(cobvar) == COB_TYPE_ALPHANUMERIC) {
                    memParams[3] = (void*)&paramsBuf[3];
                    ((char*)memParams[3])[0] = cobvar->data[0];
                    ((char*)memParams[3])[1] = 0x00;
                    (*memParamsState) = 10;
                  }
                }
                if (((*cmdState) == -7) && ((*memParamsState) == 1)) {
                  memParams[1] = (void*)cobvar->data;
                  (*memParamsState) = 10;
                }
                if (((*cmdState) == -7) && ((*memParamsState) == 2)) {
                  memParams[1] = (void*)(*((unsigned char**)cobvar->data));
                  (*memParamsState) = 10;
                }
                if (((*cmdState) == -8) && ((*memParamsState) == 1)) {
                  (*((unsigned char**)cobvar->data)) = cwa;
                  (*memParamsState) = 10;
                }
                if (((*cmdState) == -8) && ((*memParamsState) == 2)) {
                  (*((unsigned char**)cobvar->data)) = (unsigned char*)twa;
                  (*memParamsState) = 10;
                }
                if (((*cmdState) == -8) && ((*memParamsState) == 3)) {
                  (*((unsigned char**)cobvar->data)) = (unsigned char*)tua;
                  (*memParamsState) = 10;
                }
                if (((*cmdState) == -8) && ((*memParamsState) == 4)) {
                  (*((unsigned char**)cobvar->data)) = (unsigned char*)tua;
                  (*memParamsState) = 10;
                }
                if (((*cmdState) == -8) && ((*memParamsState) == 5)) {
                  (*((unsigned char**)cobvar->data)) = (unsigned char*)commArea;
                  (*memParamsState) = 10;
                }
                if (((*cmdState) == -8) && ((*memParamsState) == 6)) {
                  (*((unsigned char**)cobvar->data)) = (unsigned char*)eibbuf;
                  (*memParamsState) = 10;
                }
                if (((*cmdState) == -9) && ((*memParamsState) == 1)) {
                    // PUT FLENGTH param value
                    sprintf(end,"%s%s",cmd,"=");
                    end = &cmdbuf[strlen(cmdbuf)];
                    FILE *f = fmemopen(end, 2048-strlen(cmdbuf), "w");
                    if ((cobvar->data[0] != 0) || (COB_FIELD_TYPE(cobvar) == 17)) {
                        display_cobfield(cobvar,f);
                    }
                    putc(0x00,f);
                    fclose(f);
                    write(childfd,cmdbuf,strlen(cmdbuf));
                    write(childfd,"\n",1);
                    (*((int*)memParams[0])) = atoi(end);
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -9) && ((*memParamsState) == 2)) {
                  memParams[1] = (void*)cobvar;
                  (*memParamsState) = 10;
                  char str[20];
                  sprintf((char*)&str,"%s\n","SIZE");
                  write(childfd,str,strlen(str));
                  sprintf((char*)&str,"%s%d\n","=",(int)cobvar->size);
                  write(childfd,str,strlen(str));
                }
                if (((*cmdState) == -10) && ((*memParamsState) == 1)) {
                    // GET FLENGTH param value
                    sprintf(end,"%s%s",cmd,"=");
                    end = &cmdbuf[strlen(cmdbuf)];
                    FILE *f = fmemopen(end, 2048-strlen(cmdbuf), "w");
                    if ((cobvar->data[0] != 0) || (COB_FIELD_TYPE(cobvar) == 17)) {
                        display_cobfield(cobvar,f);
                    }
                    putc(0x00,f);
                    fclose(f);
                    write(childfd,cmdbuf,strlen(cmdbuf));
                    write(childfd,"\n",1);
                    (*((int*)memParams[0])) = atoi(end);
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -10) && ((*memParamsState) == 2)) {
                  memParams[1] = (void*)cobvar;
                  (*memParamsState) = 10;
                  char str[20];
                  sprintf((char*)&str,"%s\n","SIZE");
                  write(childfd,str,strlen(str));
                  sprintf((char*)&str,"%s%d\n","=",(int)cobvar->size);
                  write(childfd,str,strlen(str));
                }
                if (((*cmdState) == -11) && ((*memParamsState) == 1)) {
                    // ENQ RESOURCE
                    memParams[1] = (void*)cobvar;
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -11) && ((*memParamsState) == 2)) {
                    // ENQ LENGTH
                    sprintf(end,"%s%s",cmd,"=");
                    end = &cmdbuf[strlen(cmdbuf)];
                    FILE *f = fmemopen(end, 2048-strlen(cmdbuf), "w");
                    if ((cobvar->data[0] != 0) || (COB_FIELD_TYPE(cobvar) == 17)) {
                        display_cobfield(cobvar,f);
                    }
                    putc(0x00,f);
                    fclose(f);
                    write(childfd,cmdbuf,strlen(cmdbuf));
                    write(childfd,"\n",1);
                    (*((int*)memParams[0])) = atoi(end);
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -12) && ((*memParamsState) == 1)) {
                    // DEQ RESOURCE
                    memParams[1] = (void*)cobvar;
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -12) && ((*memParamsState) == 2)) {
                    // DEQ LENGTH
                    sprintf(end,"%s%s",cmd,"=");
                    end = &cmdbuf[strlen(cmdbuf)];
                    FILE *f = fmemopen(end, 2048-strlen(cmdbuf), "w");
                    if ((cobvar->data[0] != 0) || (COB_FIELD_TYPE(cobvar) == 17)) {
                        display_cobfield(cobvar,f);
                    }
                    putc(0x00,f);
                    fclose(f);
                    write(childfd,cmdbuf,strlen(cmdbuf));
                    write(childfd,"\n",1);
                    (*((int*)memParams[0])) = atoi(end);
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -14) && ((*memParamsState) == 4)) {
                    memParams[3] = (void*)cobvar;
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -14) && ((*memParamsState) == 2)) {
                    memParams[1] = (void*)cobvar;
                    (*memParamsState) = 10;
                    char str[20];
                    sprintf((char*)&str,"%s\n","SIZE");
                    write(childfd,str,strlen(str));
                    sprintf((char*)&str,"%s%d\n","=",(int)cobvar->size);
                    write(childfd,str,strlen(str));
                }
                if (((*cmdState) == -14) && ((*memParamsState) == 1)) {
                    // WRITEQ LENGTH
                    sprintf(end,"%s%s",cmd,"=");
                    end = &cmdbuf[strlen(cmdbuf)];
                    FILE *f = fmemopen(end, 2048-strlen(cmdbuf), "w");
                    if ((cobvar->data[0] != 0) || (COB_FIELD_TYPE(cobvar) == 17)) {
                        display_cobfield(cobvar,f);
                    }
                    putc(0x00,f);
                    fclose(f);
                    write(childfd,cmdbuf,strlen(cmdbuf));
                    write(childfd,"\n",1);
                    (*((int*)memParams[0])) = atoi(end);
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -15) && ((*memParamsState) == 4)) {
                    memParams[3] = (void*)cobvar;
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -15) && ((*memParamsState) == 2)) {
                    memParams[1] = (void*)cobvar;
                    (*memParamsState) = 10;
                    char str[20];
                    sprintf((char*)&str,"%s\n","SIZE");
                    write(childfd,str,strlen(str));
                    sprintf((char*)&str,"%s%d\n","=",(int)cobvar->size);
                    write(childfd,str,strlen(str));
                }
                if (((*cmdState) == -15) && ((*memParamsState) == 1)) {
                    // WRITEQ LENGTH
                    sprintf(end,"%s%s",cmd,"=");
                    end = &cmdbuf[strlen(cmdbuf)];
                    FILE *f = fmemopen(end, 2048-strlen(cmdbuf), "w");
                    if ((cobvar->data[0] != 0) || (COB_FIELD_TYPE(cobvar) == 17)) {
                        display_cobfield(cobvar,f);
                    }
                    putc(0x00,f);
                    fclose(f);
                    write(childfd,cmdbuf,strlen(cmdbuf));
                    write(childfd,"\n",1);
                    (*((int*)memParams[0])) = atoi(end);
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -18) && ((*memParamsState) == 0)) {
                    // General Read-Only data handling
                    char buf[2048];
                    readLine((char*)&buf,childfd);

                    if (COB_FIELD_TYPE(cobvar) == COB_TYPE_ALPHANUMERIC) {
                        for (int i = 0; i < cobvar->size; i++) {
                          cobvar->data[i] = buf[i];
                        }
                    }
                    if (COB_FIELD_TYPE(cobvar) == COB_TYPE_NUMERIC) {
                        char hbuf[256];
                        cob_put_picx(cobvar->data,cobvar->size,
                            convertNumeric(buf,cobvar->attr->digits,
                                           cobvar->attr->scale,hbuf));
                    }
                    if (COB_FIELD_TYPE(cobvar) == COB_TYPE_NUMERIC_BINARY) {
                      long v = atol(buf);
                      cob_put_u64_compx(v,cobvar->data,cobvar->size);
                    }
                    if (COB_FIELD_TYPE(cobvar) == COB_TYPE_NUMERIC_PACKED) {
                      long v = atol(buf);
                      cob_put_s64_comp3(v,cobvar->data,cobvar->size);
                    }
                }
                if (((*cmdState) == -18) && ((*memParamsState) == 1)) {
                    (*memParamsState) = 0;
                }
                if (((*cmdState) == -19) && ((*memParamsState) == 3)) {
                    // START TRANSID REQID
                    write(childfd,"'",1);
                    write(childfd,cobvar->data,8);
                    write(childfd,"'\n",2);
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -19) && ((*memParamsState) == 2)) {
                    memParams[1] = (void*)cobvar;
                    (*memParamsState) = 10;
                    char str[20];
                    sprintf((char*)&str,"%s\n","SIZE");
                    write(childfd,str,strlen(str));
                    sprintf((char*)&str,"%s%d\n","=",(int)cobvar->size);
                    write(childfd,str,strlen(str));
                }
                if (((*cmdState) == -19) && ((*memParamsState) == 1)) {
                    // START TRANSID LENGTH
                    sprintf(end,"%s%s",cmd,"=");
                    end = &cmdbuf[strlen(cmdbuf)];
                    FILE *f = fmemopen(end, 2048-strlen(cmdbuf), "w");
                    if ((cobvar->data[0] != 0) || (COB_FIELD_TYPE(cobvar) == 17)) {
                        display_cobfield(cobvar,f);
                    }
                    putc(0x00,f);
                    fclose(f);
                    write(childfd,cmdbuf,strlen(cmdbuf));
                    write(childfd,"\n",1);
                    (*((int*)memParams[0])) = atoi(end);
                    (*memParamsState) = 10;
                }
            }
            cmdbuf[0] = 0x00;
        }
        return 1;
    }
    if (strstr(cmd,"END-EXEC")) {
        cmdbuf[strlen(cmdbuf)-1] = '\n';
        cmdbuf[strlen(cmdbuf)] = 0x00;
//      write(childfd,cmdbuf,strlen(cmdbuf));
        cmdbuf[strlen(cmdbuf)-1] = 0x00;
        processCmd(cmdbuf,outputVars);
        cmdbuf[0] = 0x00;
        (*cmdState) = 0;
        outputVars[0] = NULL; // NULL terminated list
    } else {
        if ((strlen(cmd) == 0) && (var != NULL)) {
            cob_field *cobvar = (cob_field*)var;
            if ((*cmdState) < 2) {
                FILE *f = fmemopen(end, 2048-strlen(cmdbuf), "w");
                if (COB_FIELD_TYPE(cobvar) == COB_TYPE_ALPHANUMERIC) putc('\'',f);
                if ((cobvar->data[0] != 0) || (COB_FIELD_TYPE(cobvar) == 17)) {
                    display_cobfield(cobvar,f);
                }
                if (COB_FIELD_TYPE(cobvar) == COB_TYPE_ALPHANUMERIC) putc('\'',f);
                putc(' ',f);
                putc(0x00,f);
                fclose(f);
            } else {
                int index = (*cmdState)-2;
                if (index <= 98) {
                    outputVars[index] = cobvar;
                    outputVars[index+1] = NULL;
                }
                (*cmdState)++;
            }
        } else {
            if (strstr(cmd,"SELECT") || strstr(cmd,"FETCH")) {
                (*cmdState) = 1;
            } else {
                if (strstr(cmd,"INTO") && ((*cmdState) == 1)) {
                    (*cmdState) = 2;
                } else {
                    if ((strstr(cmd,",") == NULL) && (*cmdState) >= 2) {
                        (*cmdState) = 0;
                    }
                }
            }
            if ((*cmdState) < 2) {
                sprintf(end,"%s%s",cmd," ");
            }
        }
    }
    return 1;
}


// Manage load module executor
void initExec(int initCons) {
    performEXEC = &execCallback;
    resolveCALL = &callCallback;
    cobinit();
    pthread_key_create(&childfdKey, NULL);
    pthread_key_create(&connKey, NULL);
    pthread_key_create(&cmdbufKey, NULL);
    pthread_key_create(&cmdStateKey, NULL);
    pthread_key_create(&cobFieldKey, NULL);
    pthread_key_create(&xctlStateKey, NULL);
    pthread_key_create(&xctlParamsKey, NULL);
    pthread_key_create(&eibbufKey, NULL);
    pthread_key_create(&linkAreaKey, NULL);
    pthread_key_create(&linkAreaPtrKey, NULL);
    pthread_key_create(&commAreaKey, NULL);
    pthread_key_create(&commAreaPtrKey, NULL);
    pthread_key_create(&areaModeKey, NULL);
    pthread_key_create(&linkStackKey, NULL);
    pthread_key_create(&linkStackPtrKey, NULL);
    pthread_key_create(&memParamsKey, NULL);
    pthread_key_create(&memParamsStateKey, NULL);
    pthread_key_create(&twaKey, NULL);
    pthread_key_create(&tuaKey, NULL);
    pthread_key_create(&respFieldsStateKey, NULL);
    pthread_key_create(&respFieldsKey, NULL);
    pthread_key_create(&taskLocksKey, NULL);
#ifndef _USE_ONLY_PROCESSES_
    pthread_mutex_init(&moduleMutex,NULL);
    pthread_cond_init(&waitForModuleChange,NULL);
#endif

    for (int i = 0; i < 100; i++) {
        condHandler[i] = NULL;
    }
    initSharedMalloc(initCons);
    sharedAllocMem = (void**)sharedMalloc(11,MEM_POOL_SIZE*sizeof(void*));
    sharedAllocMemLen = (int*)sharedMalloc(14,MEM_POOL_SIZE*sizeof(int));
    sharedAllocMemPtr = (int*)sharedMalloc(12,sizeof(int));
    cwa = (unsigned char*)sharedMalloc(13,4096);
    initEnqResources(initCons);

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&sharedMemMutex,&attr);

    setUpPool(10, GETENV_STRING(connectStr,"QWICS_DB_CONNECTSTR","dbname=qwics"), initCons);
    currentMap[0] = 0x00;
    cob_get_global_ptr ()->cob_call_params = 1;
}


void clearExec(int initCons) {
#ifndef _USE_ONLY_PROCESSES_
    pthread_cond_destroy(&waitForModuleChange);
#endif
    tearDownPool(initCons);
    sharedFree(sharedAllocMem,MEM_POOL_SIZE*sizeof(void*));
    sharedFree(sharedAllocMemLen,MEM_POOL_SIZE*sizeof(int));
    sharedFree(sharedAllocMemPtr,sizeof(int));
    sharedFree(cwa,4096);

    for (int i = 0; i < 100; i++) {
        if (condHandler[i] != NULL) {
            free(condHandler[i]);
        }
    }
}


void execTransaction(char *name, void *fd, int setCommArea) {
    char cmdbuf[2048];
    int cmdState = 0;
    int xctlState = 0;
    char progname[9];
    char *xctlParams[10];
    char eibbuf[150];
    char linkArea[4096];
    char commArea[32768];
    int linkAreaPtr = 0;
    int commAreaPtr = 0;
    int areaMode = 0;
    char linkStack[900];
    int linkStackPtr = 0;
    int memParamsState = 0;
    void *memParams[10];
    int memParam = 0;
    char twa[32768];
    char tua[256];
    void** allocMem = (void**)malloc(MEM_POOL_SIZE*sizeof(void*));
    int allocMemPtr = 0;
    int respFieldsState = 0;
    void *respFields[2];
    struct taskLock *taskLocks = createTaskLocks();
    int i = 0;
    for (i= 0; i < 150; i++) eibbuf[i] = 0;
    xctlParams[0] = progname;
    memParams[0] = &memParam;
    cob_field* outputVars[100];
    outputVars[0] = NULL; // NULL terminated list
    cmdbuf[0] = 0x00;
    pthread_setspecific(childfdKey, fd);
    pthread_setspecific(cmdbufKey, &cmdbuf);
    pthread_setspecific(cmdStateKey, &cmdState);
    pthread_setspecific(cobFieldKey, &outputVars);
    pthread_setspecific(xctlStateKey, &xctlState);
    pthread_setspecific(xctlParamsKey, &xctlParams);
    pthread_setspecific(eibbufKey, &eibbuf);
    pthread_setspecific(linkAreaKey, &linkArea);
    pthread_setspecific(linkAreaPtrKey, &linkAreaPtr);
    pthread_setspecific(commAreaKey, &commArea);
    pthread_setspecific(commAreaPtrKey, &commAreaPtr);
    pthread_setspecific(areaModeKey, &areaMode);
    pthread_setspecific(linkStackKey, &linkStack);
    pthread_setspecific(linkStackPtrKey, &linkStackPtr);
    pthread_setspecific(memParamsKey, &memParams);
    pthread_setspecific(memParamsStateKey, &memParamsState);
    pthread_setspecific(twaKey, &twa);
    pthread_setspecific(tuaKey, &tua);
    pthread_setspecific(allocMemKey, allocMem);
    pthread_setspecific(allocMemPtrKey, &allocMemPtr);
    pthread_setspecific(respFieldsStateKey, &respFieldsState);
    pthread_setspecific(respFieldsKey, &respFields);
    pthread_setspecific(taskLocksKey, taskLocks);
    // Oprionally read in content of commarea
    if (setCommArea == 1) {
      write(*(int*)fd,"COMMAREA\n",9);
      char c = 0x00;
      for (i = 0; i < 4096; ) {
        int n = read(*(int*)fd,&c,1);
        if (n == 1) {
          commArea[i] = c;
          i++;
        }
      }
    }
    PGconn *conn = getDBConnection();
    pthread_setspecific(connKey, (void*)conn);
    initMain();
    execLoadModule(name,0);
    releaseLocks(TASK,taskLocks);
    clearMain();
    free(allocMem);
    returnDBConnection(conn,1);
}


// Exec COBOL module within an existing DB transaction
void execInTransaction(char *name, void *fd, int setCommArea) {
    char cmdbuf[2048];
    int cmdState = 0;
    int xctlState = 0;
    char *xctlParams[10];
    char eibbuf[150];
    char linkArea[4096];
    char commArea[4096];
    int linkAreaPtr = 0;
    int commAreaPtr = 0;
    int areaMode = 0;
    char progname[9];
    char linkStack[900];
    int linkStackPtr = 0;
    int memParamsState = 0;
    void *memParams[10];
    int memParam = 0;
    char twa[32768];
    char tua[256];
    void** allocMem = (void**)malloc(MEM_POOL_SIZE*sizeof(void*));
    int allocMemPtr = 0;
    int respFieldsState = 0;
    void *respFields[2];
    struct taskLock *taskLocks = createTaskLocks();
    int i = 0;
    for (i= 0; i < 150; i++) eibbuf[i] = 0;
    xctlParams[0] = progname;
    memParams[0] = &memParam;
    cob_field* outputVars[100];
    outputVars[0] = NULL; // NULL terminated list
    cmdbuf[0] = 0x00;
    pthread_setspecific(childfdKey, fd);
    pthread_setspecific(cmdbufKey, &cmdbuf);
    pthread_setspecific(cmdStateKey, &cmdState);
    pthread_setspecific(cobFieldKey, &outputVars);
    pthread_setspecific(xctlStateKey, &xctlState);
    pthread_setspecific(xctlParamsKey, &xctlParams);
    pthread_setspecific(eibbufKey, &eibbuf);
    pthread_setspecific(linkAreaKey, &linkArea);
    pthread_setspecific(linkAreaPtrKey, &linkAreaPtr);
    pthread_setspecific(commAreaKey, &commArea);
    pthread_setspecific(commAreaPtrKey, &commAreaPtr);
    pthread_setspecific(areaModeKey, &areaMode);
    pthread_setspecific(linkStackKey, &linkStack);
    pthread_setspecific(linkStackPtrKey, &linkStackPtr);
    pthread_setspecific(memParamsKey, &memParams);
    pthread_setspecific(memParamsStateKey, &memParamsState);
    pthread_setspecific(twaKey, &twa);
    pthread_setspecific(tuaKey, &tua);
    pthread_setspecific(allocMemKey, allocMem);
    pthread_setspecific(allocMemPtrKey, &allocMemPtr);
    pthread_setspecific(respFieldsStateKey, &respFieldsState);
    pthread_setspecific(respFieldsKey, &respFields);
    pthread_setspecific(taskLocksKey, taskLocks);
    // Oprionally read in content of commarea
    if (setCommArea == 1) {
      write(*(int*)fd,"COMMAREA\n",9);
      char c = 0x00;
      for (i = 0; i < 4096; ) {
        int n = read(*(int*)fd,&c,1);
        if (n == 1) {
          commArea[i] = c;
          i++;
        }
      }
    }
    initMain();
    execLoadModule(name,0);
    releaseLocks(TASK,taskLocks);
    clearMain();
    free(allocMem);
}
