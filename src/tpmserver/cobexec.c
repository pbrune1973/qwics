/*******************************************************************************************/
/*   QWICS Server COBOL load module executor                                               */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 06.12.2023                                  */
/*                                                                                         */
/*   Copyright (C) 2018 - 2023 by Philipp Brune  Email: Philipp.Brune@qwics.org            */
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
#include <signal.h>
#include <errno.h>
#include <time.h>

#include <libcob.h>
#include <setjmp.h>
#include "config.h"
#include "db/conpool.h"
#include "env/envconf.h"
#include "msg/queueman.h"
#include "shm/shmtpm.h"
#include "enqdeq/enqdeq.h"
#include "dataset/isamdb.h"

#ifdef __APPLE__
#include "macosx/fmemopen.h"
#endif

#define CMDBUF_SIZE 65536

#define execSql(sql, fd) _execSql(sql, fd, 1, 0)

// Keys for thread specific data
pthread_key_t connKey;
pthread_key_t childfdKey;
pthread_key_t cmdbufKey;
pthread_key_t cmdStateKey;
pthread_key_t runStateKey;
pthread_key_t cobFieldKey;
pthread_key_t xctlStateKey;
pthread_key_t retrieveStateKey;
pthread_key_t xctlParamsKey;
pthread_key_t eibbufKey;
pthread_key_t eibaidKey;
pthread_key_t linkAreaKey;
pthread_key_t linkAreaPtrKey;
pthread_key_t linkAreaAdrKey;
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
pthread_key_t callStackKey;
pthread_key_t callStackPtrKey;
pthread_key_t chnBufListKey;
pthread_key_t chnBufListPtrKey;
pthread_key_t isamTxKey;
pthread_key_t currentNamesKey;
pthread_key_t callbackFuncKey;

// Callback function declared in libcob
extern int (*performEXEC)(char*, void*);
extern void* (*resolveCALL)(char*);

// SQLCA
cob_field *sqlcode = NULL;

#define MAX_OPENDATASETS 10
#define MAX_OPENCURSORS 10
#define MAX_ABENDHANDLERS 50

struct openCursorType {
    int id;
    void* curHandle;
    int keylen;
    unsigned char rid[256];
};

struct openDatasetType {
    char dsName[46];
    void* dsHandle;
    struct openCursorType openCursors[MAX_OPENCURSORS];
    struct openCursorType rewriteCur;
};

struct abendHandlerType {
    int (*abendHandler)();
    char abendProgname[9];
    int abendIsActive;
};

struct currentNamesType {
    char currentMap[9];
    char currentMapSet[9];
    char fileName[46];
    struct openDatasetType openDatasets[MAX_OPENDATASETS];
    int memParamInts[10];
    int abendHandlerCnt;
    struct abendHandlerType abendHandlers[MAX_ABENDHANDLERS];
    char abcode[5];
};

#ifdef _LIBTPMSERVER_
#include <jni.h>

struct callbackFuncType {
    int (*callback)(char *loadmod, void *data);
    JNIEnv *env;
    jobject self;
    int mode;
};

#endif

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
char *datasetDir = NULL;

void **sharedAllocMem;
int *sharedAllocMemLen;
int *sharedAllocMemPtr = NULL;

char paramsBuf[10][256];
void *paramList[10];

unsigned char *cwa;
jmp_buf taskState;

jmp_buf *condHandler[100];

cob_module thisModule;

struct chnBuf {
    unsigned char *buf;
};

char *cobDateFormat = "YYYY-MM-dd-hh.mm.ss.uuuuu";
//char *dbDateFormat = "dd-MM-YYYY hh:mm:ss.uuu";
char *dbDateFormat = "YYYY-MM-dd hh:mm:ss.uuu";
char result[30];


int putOpenDataset(struct openDatasetType *ds, char *dsName, void *dsHandle) {
    int i = 0;
    for (i = 0; i < MAX_OPENDATASETS; i++) {
        if (ds[i].dsHandle == NULL) {
            sprintf(ds[i].dsName,"%s",dsName);
            ds[i].dsHandle = dsHandle;
            ds[i].rewriteCur.id = -1;
            ds[i].rewriteCur.curHandle = NULL;
            ds[i].rewriteCur.keylen = 0;
            return 1;
        }
    }
    return 0;
}


void *getOpenDataset(struct openDatasetType *ds, char *dsName, struct openDatasetType **ods) {
    int i = 0;
    for (i = 0; i < MAX_OPENDATASETS; i++) {
        if ((ds[i].dsHandle != NULL) && (strcmp(ds[i].dsName,dsName) == 0)) {
            *ods = &ds[i];
            return ds[i].dsHandle;
        }
    }
    *ods = NULL;
    return NULL;
}


void initOpenDatasets(struct openDatasetType *ds) {
    int i = 0, j = 0;
    for (i = 0; i < MAX_OPENDATASETS; i++) {
        ds[i].dsHandle = NULL;
        ds[i].dsName[0] = 0x00;
        for (j = 0; j < MAX_OPENCURSORS; j++) {
            ds[i].openCursors[j].id = 0;
            ds[i].openCursors[j].curHandle = NULL;
            ds[i].openCursors[j].keylen = 0;
        }
        ds[i].rewriteCur.id = -1;
        ds[i].rewriteCur.curHandle = NULL;
        ds[i].rewriteCur.keylen = 0;
    }
}


void closeOpenDatasets(struct openDatasetType *ds) {
    int i = 0, j = 0;
    for (i = 0; i < MAX_OPENDATASETS; i++) {
        if (ds[i].dsHandle != NULL) {
            for (j = 0; j < MAX_OPENCURSORS; j++) {
                if (ds[i].openCursors[j].curHandle != NULL) {
                    closeCursor(ds[i].openCursors[j].curHandle);
                }
            }
            closeDataset(ds[i].dsHandle);
        }
    }
}


int putOpenCursor(struct openDatasetType *ds, char *dsName, int id, void *curHandle) {
    int i = 0, j = 0;
    for (i = 0; i < MAX_OPENDATASETS; i++) {
        if ((ds[i].dsHandle != NULL) && (strcmp(ds[i].dsName,dsName) == 0)) {
            for (j = 0; j < MAX_OPENCURSORS; j++) {
                if (ds[i].openCursors[j].curHandle == NULL) {
                    ds[i].openCursors[j].id = id;
                    ds[i].openCursors[j].curHandle = curHandle;
                    ds[i].openCursors[j].keylen = 0;
                    return 1;
                }
            }
            return 0;
        }
    }
    return 0;
}


struct openCursorType *getOpenCursor(struct openDatasetType *ds, char *dsName, int id) {
    int i = 0, j = 0;
    for (i = 0; i < MAX_OPENDATASETS; i++) {
        if ((ds[i].dsHandle != NULL) && (strcmp(ds[i].dsName,dsName) == 0)) {
            for (j = 0; j < MAX_OPENCURSORS; j++) {
                if ((ds[i].openCursors[j].curHandle != NULL) && (ds[i].openCursors[j].id == id)) {
                    return &ds[i].openCursors[j];
                }
            }
            return NULL;
        }
    }
    return NULL;
}


void setCursorRid(struct openCursorType *cur, unsigned char *rid, int keylen) {
    if (keylen > 255) {
        keylen = 255;
    }
    cur->keylen = keylen;
    memcpy(cur->rid,rid,keylen);
}


int equalsCursorRid(struct openCursorType *cur, unsigned char *rid, int keylen) {
    if (keylen > 255) {
        keylen = 255;
    }
    if (cur->keylen != keylen) {
        return 0;
    }
    int i = 0;
    for (i = 0; i < keylen; i++) {
        if (cur->rid[i] != rid[i]) {
            return 0;
        }
    }
    return 1;
}


unsigned char *getNextChnBuf(int size) {
    int *chnBufListPtr = (int*)pthread_getspecific(chnBufListPtrKey);
    struct chnBuf *chnBufList = (struct chnBuf *)pthread_getspecific(chnBufListKey);

    if (*chnBufListPtr < 256) {
        chnBufList[*chnBufListPtr].buf = malloc(size);
        (*chnBufListPtr)++;
        return chnBufList[(*chnBufListPtr)-1].buf;
    }
    return NULL;
}


void clearChnBufList() {
    int *chnBufListPtr = (int*)pthread_getspecific(chnBufListPtrKey);
    struct chnBuf *chnBufList = (struct chnBuf *)pthread_getspecific(chnBufListKey);
    int i;

    for (i = 0; i < (*chnBufListPtr); i++) {
        free(chnBufList[i].buf);
    }
}


void cm(int res) {
    if (res != 0) {
      fprintf(stderr,"%s%d\n","Mutex operation failed: ",res);
    }
}


void displayNumeric(cob_field *f, FILE *fp) {
    cob_pic_symbol *p;
    unsigned char q[255];
    unsigned short digits = COB_FIELD_DIGITS (f);
    signed short scale = COB_FIELD_SCALE (f);
    int i, size = digits + !!COB_FIELD_HAVE_SIGN (f) + !!scale;
    cob_field_attr attr;
    cob_field temp;
    cob_pic_symbol pic[6] = {{ '\0' }};

    temp.size = size;
    temp.data = q;
    temp.attr = &attr;

    attr.type = COB_TYPE_NUMERIC_EDITED; 
    attr.digits = digits; 
    attr.scale = scale; 
    attr.flags = 0; 
    attr.pic = (const cob_pic_symbol *)pic; 

    p = pic;

    if (COB_FIELD_HAVE_SIGN(f)) {
        if (!COB_FIELD_SIGN_SEPARATE(f) || COB_FIELD_SIGN_LEADING(f)) {
            p->symbol = '+';
            p->times_repeated = 1;
            p++;
        }
    }
    if (scale > 0) {
        if (digits - scale > 0) {
            p->symbol = '9';
            p->times_repeated = digits - scale;
            p++;
        }

        p->symbol = '.';
        p->times_repeated = 1;
        p++;

        p->symbol = '9';
        p->times_repeated = scale;
        p++;
    } else {
        p->symbol = '9';
        p->times_repeated = digits;
        p++;
    }
    if (COB_FIELD_HAVE_SIGN (f)) {
        if (COB_FIELD_SIGN_SEPARATE (f) && !COB_FIELD_SIGN_LEADING(f)) {
            p->symbol = '+';
            p->times_repeated = 1;
            p++;
        }
    }
    p->symbol = '\0';

    cob_move (f, &temp);
    for (i = 0; i < size; i++) {
        putc (q[i], fp);
    }
}


int getCobType(cob_field *f) {
    if (f->attr->type == COB_TYPE_NUMERIC_BINARY) {
#ifndef WORDS_BIGENDIAN
        if (COB_FIELD_BINARY_SWAP(f))
            return COB_TYPE_NUMERIC_BINARY;
        return COB_TYPE_NUMERIC_COMP5;
#else
        return COB_TYPE_NUMERIC_BINARY;
#endif
        }
        return (int)f->attr->type;
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


void setNumericValue(long v, cob_field *cobvar) {
    if (COB_FIELD_TYPE(cobvar) == COB_TYPE_NUMERIC) {
        char hbuf[256],buf[256];
        sprintf(buf,"%ld",v);
        cob_put_picx(cobvar->data,cobvar->size,
                    convertNumeric(buf,cobvar->attr->digits,
                                       cobvar->attr->scale,hbuf));
    }
    if (COB_FIELD_TYPE(cobvar) == COB_TYPE_NUMERIC_PACKED) {
        cob_put_s64_comp3(v,cobvar->data,cobvar->size);
    }
    if (getCobType(cobvar) == COB_TYPE_NUMERIC_BINARY) {
        cob_put_u64_compx(v,cobvar->data,cobvar->size);
    }
    if (getCobType(cobvar) == COB_TYPE_NUMERIC_COMP5) {
        cob_put_s64_comp5(v,cobvar->data,cobvar->size);
    }
}


char* adjustDateFormatToDb(char *str, int len) {
    int i = 0, l = strlen(cobDateFormat), pos = 0;
    char lastc = ' ';
    if (len < l) {
        return str;
    }
    // Check if str is date
    for (i = 0; i < l; i++) {
        if ((cobDateFormat[i] == '-') || (cobDateFormat[i] == ' ') || 
            (cobDateFormat[i] == ':') || (cobDateFormat[i] == '.')) {
            if (cobDateFormat[i] != str[i]) {
                if ((i == 10) &&
                    (cobDateFormat[i] == '-') && (str[i] == ' ')) {
                   continue;
                }
                if ((i == 13) &&
                    (cobDateFormat[i] == '.') && (str[i] == ':')) {
                   continue;
                }
                if ((i == 16) &&
                    (cobDateFormat[i] == '.') && (str[i] == ':')) {
                   continue;
                }
                return str;
            }       
        }
    }

    memset(result,' ',len);
    result[len] = 0x00;

    for (i = 0; i < strlen(dbDateFormat); i++) {
        if ((dbDateFormat[i] == '-') || (dbDateFormat[i] == ' ') || 
            (dbDateFormat[i] == ':') || (dbDateFormat[i] == '.')) {
            result[i] = dbDateFormat[i];
            continue;
        } else {
            if (lastc != dbDateFormat[i]) {
                int j = 0;
                while (j < l) {
                    if (dbDateFormat[i] == cobDateFormat[j]) {
                        break;
                    }
                    j++;
                }                
                if (j < l) {
                    pos = j;
                } else {
                    return result;                    
                }
                lastc = dbDateFormat[i];
            }

            result[i] = str[pos];
            pos++;
        }
    }

    return result;
}


char* adjustTimeFormatToDb(char *str, int len) {
    int i = 0, l = strlen(cobDateFormat), pos = 0;
    if (len == 10) {
        for (i = 0; i < len; i++) {
            if (str[i] != ' ') {
                return str;
            }
        }
        sprintf(result,"%s","0001-01-01");
        return result;
    }
    if (len == 26) {
        for (i = 0; i < len; i++) {
            if (str[i] != ' ') {
                return str;
            }
        }
        sprintf(result,"%s","0001-01-01 00:00:00.000   ");
        return result;
    }
    if (len != 5) {
        return str;
    }
    for (i = 0; i < len; i++) {
        if (str[i] != ' ') {
            break;
        }
    }
    if (i == len) {
        sprintf(result,"%s","00:00");
        return result;
    }
    if (str[2] != '.') {
        return str;
    }
    if (!(str[0] >= '0' && str[0] <= '9')) {
        return str;
    }
    if (!(str[1] >= '0' && str[1] <= '9')) {
        return str;
    }
    if (!(str[3] >= '0' && str[3] <= '9')) {
        return str;
    }
    if (!(str[4] >= '0' && str[4] <= '9')) {
        return str;
    }
    sprintf(result,"%s",str);
    result[2] = ':';
    return result;
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
    char m[9], ms[9];
    sprintf(m,"%s",map);
    sprintf(ms,"%s",mapset);
    for (int i = 0; i < 8; i++) {
        if (m[i] == ' ') m[i] = 0x00;
        if (ms[i] == ' ') ms[i] = 0x00;
    }
    l = strlen(m);
    write(childfd,"JSON=",5);
    char jsonFile[255];
    sprintf(jsonFile,"%s%s%s%s",GETENV_STRING(jsDir,"QWICS_JSDIR","../copybooks"),"/",ms,".js");
    FILE *js = fopen(jsonFile,"rb");
    if (js != NULL) {
        while (1) {
            char c = fgetc(js);
            if (feof(js)) {
                break;
            }
            if (found == 0) {
                if (m[n] == c) {
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


void setSQLCA(int code, char *state) {
    if (sqlcode != NULL) {
        cob_field sqlstate = { 5, sqlcode->data+119, NULL };
        cob_set_int(sqlcode,code);
        cob_put_picx(sqlstate.data,sqlstate.size,state);
    }
}


// Callback handler for EXEC statements
int processCmd(char *cmd, cob_field **outputVars) {
    char *pos;
#ifndef _LIBTPMSERVER_  
    if ((pos=strstr(cmd,"EXEC SQL")) != NULL) {
        char *sql = (char*)pos+9;
        PGconn *conn = (PGconn*)pthread_getspecific(connKey);
        if (conn == NULL) {
            setSQLCA(-1,"00000");
            return 1;
        }
        setSQLCA(0,"00000");
        if (outputVars[0] == NULL) {
            int r = execSQL(conn, sql);
            if (r == 0) {
                setSQLCA(-1,"00000");
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
                            if (outputVars[i]->attr->type == COB_TYPE_GROUP) {
                                // Map VARCHAR to group struct
                                char *v = (char*)PQgetvalue(res, 0, i);
                                unsigned int l = (unsigned int)strlen(v);
		                        if (l > (outputVars[i]->size-2)) {
                                   l = outputVars[i]->size-2;
                                }
                                int n = 0, j = 2;
                                for (n = 0; n < l; n++, j++) {
                                    char c = v[n];
                                    if (n < l-1) {
                                        if (v[n] == '\\' && v[n+1] == '0') {
                                            n++;
                                            c = 0x00;
                                        } else 
                                        if ((c & 0xF0) == 0xC0) {
                                            // Convert UTF-8 to ASCII                                            
                                            char ca = (c & 0x03) << 6;
                                            ca = ca | (v[n+1] & 0x3F);
                                            c = ca;
                                            n++;
                                        }
                                    }
                                    outputVars[i]->data[j] = c;
                                }
                                l = j-2;
	                            outputVars[i]->data[0] = (unsigned char)((l >> 8) & 0xFF);
	                            outputVars[i]->data[1] = (unsigned char)(l & 0xFF);
                            } else 
                            if (outputVars[i]->attr->type == COB_TYPE_ALPHANUMERIC) {
                                char *v = (char*)PQgetvalue(res, 0, i);
                                unsigned int l = (unsigned int)strlen(v);
		                        if (l > outputVars[i]->size) {
                                   l = outputVars[i]->size;
                                }
                                int n = 0, j = 0;
                                for (n = 0; n < l; n++, j++) {
                                    char c = v[n];
                                    if (n < l-1) {
                                        if (v[n] == '\\' && v[n+1] == '0') {
                                            n++;
                                            c = 0x00;
                                        } else 
                                        if ((c & 0xF0) == 0xC0) {
                                            // Convert UTF-8 to ASCII                                            
                                            char ca = (c & 0x03) << 6;
                                            ca = ca | (v[n+1] & 0x3F);
                                            c = ca;
                                            n++;
                                        }
                                    }
                                    outputVars[i]->data[j] = c;
                                }
                                for (; j < outputVars[i]->size; j++) {
                                    outputVars[i]->data[j] = ' ';
                                }
                            } else 
                            if (outputVars[i]->attr->type == COB_TYPE_NUMERIC) {
                                char buf[256];
                                cob_put_picx(outputVars[i]->data,outputVars[i]->size,
                                convertNumeric(PQgetvalue(res, 0, i),
                                                 outputVars[i]->attr->digits,
                                                 outputVars[i]->attr->scale,buf));
                            } else
                            if (outputVars[i]->attr->type == COB_TYPE_NUMERIC_PACKED) {
                                long v = atol(PQgetvalue(res, 0, i));
                                cob_put_s64_comp3(v,outputVars[i]->data,outputVars[i]->size);
                            } else 
                            if (getCobType(outputVars[i]) == COB_TYPE_NUMERIC_BINARY) {
                                long v = atol(PQgetvalue(res, 0, i));
                                cob_put_u64_compx(v,outputVars[i]->data,outputVars[i]->size);  
                            } else
                            if (getCobType(outputVars[i]) == COB_TYPE_NUMERIC_COMP5) {
                                long v = atol(PQgetvalue(res, 0, i));
                                cob_put_s64_comp5(v,outputVars[i]->data,outputVars[i]->size);  
                            } else {
                                cob_put_picx(outputVars[i]->data,outputVars[i]->size,PQgetvalue(res, 0, i));
                            }
                        }
                        i++;
                    }
                } else {
                    setSQLCA(100,"02000");
                }
                PQclear(res);
            } else {
                setSQLCA(-1,"00000");
            }
        }
        printf("%s\n",sql);
    }
#endif
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
printf("getmain %d %d %d %d %x\n",length,shared,*allocMemPtr,MEM_POOL_SIZE,allocMem);
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
    printf("%s %d %lx %d\n","getmain",length,(unsigned long)p,shared);
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


int freemain(void *p) {
  void **allocMem = (void**)pthread_getspecific(allocMemKey);
  int *allocMemPtr = (int*)pthread_getspecific(allocMemPtrKey);
  for (int i = 0; i < (*allocMemPtr); i++) {
      if ((p != NULL) && (allocMem[i] == p)) {
          printf("%s %lx\n","freemain",(unsigned long)p);
          free(allocMem[i]);
          allocMem[i] = NULL;
          if (i == (*allocMemPtr)-1) {
            (*allocMemPtr)--;
          }
          return 0;
      }
  }
  // Free shared mem
  int r = -1;
  cm(pthread_mutex_lock(&sharedMemMutex));
  allocMem = sharedAllocMem;
  allocMemPtr = sharedAllocMemPtr;
  for (int i = 0; i < (*allocMemPtr); i++) {
      if ((p != NULL) && (allocMem[i] == p)) {
          printf("%s %lx\n","freemain shared",(unsigned long)p);
          sharedFree(allocMem[i],sharedAllocMemLen[i]);
          allocMem[i] = NULL;
          if (i == (*allocMemPtr)-1) {
            (*allocMemPtr)--;
          }
          r = 0;
          break;
      }
  }
  cm(pthread_mutex_unlock(&sharedMemMutex));
  return r;  
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
void _execSql(char *sql, void *fd, int sendRes, int sync) {
    char response[1024];
    pthread_setspecific(childfdKey, fd);
    if (strstr(sql,"BEGIN")) {
        if (!sync) {
           PGconn *conn = getDBConnection();
           pthread_setspecific(connKey, (void*)conn);
        } else {
           PGconn *conn = (PGconn*)pthread_getspecific(connKey);
           if (conn == NULL) {
               conn = getDBConnection();
               pthread_setspecific(connKey, (void*)conn);
           } else {
               beginDBConnection(conn);            
           }
        }
        void* tx = pthread_getspecific(isamTxKey);
        if (tx != NULL) {
            endTransaction(tx,1);            
        }
        tx = beginTransaction();
        pthread_setspecific(isamTxKey,tx);
        return;
    }
    if (strstr(sql,"COMMIT")) {
        PGconn *conn = (PGconn*)pthread_getspecific(connKey);
        int r = 0;
        if (conn != NULL) {
            if (!sync) {
                r = returnDBConnection(conn, 1);
                pthread_setspecific(connKey, NULL);
            } else {
                r = syncDBConnection(conn, 1);
            }
        } else {
            r = 1;            
        }
        void* tx = pthread_getspecific(isamTxKey);
        if (tx != NULL) {
            if (endTransaction(tx,1) != 0) {
                r = 0;
            }          
        }
        pthread_setspecific(isamTxKey,NULL);

        if (sendRes == 1) {
            if (r == 0) {
                sprintf(response,"%s\n","ERROR");
                write(*((int*)fd),&response,strlen(response));
            } else {
                sprintf(response,"%s\n","OK");
                write(*((int*)fd),&response,strlen(response));
            }
        }

        return;
    }
    if (strstr(sql,"ROLLBACK")) {
        PGconn *conn = (PGconn*)pthread_getspecific(connKey);
        int r = 0;
        if (conn != NULL) {
            if (!sync) {
                r = returnDBConnection(conn, 0);
                pthread_setspecific(connKey, NULL);
            } else {
                r = syncDBConnection(conn, 0);
            }
        } else {
            r = 1;            
        }
        void* tx = pthread_getspecific(isamTxKey);
        if (tx != NULL) {
            if (endTransaction(tx,0) != 0) {
                r = 0;
            }          
        }
        pthread_setspecific(isamTxKey,NULL);
        
        if (sendRes == 1) {
            if (r == 0) {
                sprintf(response,"%s\n","ERROR");
                write(*((int*)fd),&response,strlen(response));
            } else {
                sprintf(response,"%s\n","OK");
                write(*((int*)fd),&response,strlen(response));
            }
        }
        return;
    }
    if ((strstr(sql,"SELECT") || strstr(sql,"FETCH") || strstr(sql,"select") || strstr(sql,"fetch")) &&
        (strstr(sql,"DECLARE") == NULL) && (strstr(sql,"declare") == NULL)) {
        PGconn *conn = (PGconn*)pthread_getspecific(connKey);
        if (conn == NULL) {
            sprintf(response,"%s\n","ERROR");
            write(*((int*)fd),&response,strlen(response));
            return;
        }
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
    char *r = NULL;
    if (conn != NULL) {
        r = execSQLCmd(conn, sql);
    }
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
  char response[1024];
  char *abcode = "ASRA";
  switch (resp) {
      case 16: abcode = "A47B"; 
               break;
      case 22: abcode = "AEIV"; 
               break;
      case 23: abcode = "AEIW"; 
               break;
      case 26: abcode = "AEIZ"; 
               break;
      case 27: abcode = "AEI0"; 
               break;
      case 28: abcode = "AEI1"; 
               break;
      case 44: abcode = "AEYH"; 
               break;
      case 55: abcode = "ASRA"; 
               break;
      case 82: abcode = "ASRA"; 
               break;
      case 110: abcode = "ASRA"; 
               break;
      case 122: abcode = "ASRA"; 
               break;
  }
  int *respFieldsState = (int*)pthread_getspecific(respFieldsStateKey);
  int *cmdState = (int*)pthread_getspecific(cmdStateKey);
  int *xctlState = (int*)pthread_getspecific(xctlStateKey);
  struct currentNamesType *currentNames = (struct currentNamesType*)pthread_getspecific(currentNamesKey);

  if (strlen(currentNames->abcode) > 0) {
    abcode = currentNames->abcode;
  }

  if ((*cmdState) != -17) {
    // ABEND not triggered by explicit ABEND command
    if ((*respFieldsState) > 0) {
      // RESP param set, continue
      return;      
    }

    int *runState = (int*)pthread_getspecific(runStateKey);
    if ((*runState) != 3) {
        // Check for active ABEND handlers
        int i = 0;
        for (i = currentNames->abendHandlerCnt-1; i >= 0; i--) {
            if (currentNames->abendHandlers[i].abendIsActive) {
                currentNames->abendHandlers[i].abendIsActive = 0;
                if (strlen(currentNames->abendHandlers[i].abendProgname) > 0) {
                    (*cmdState) = 0;
                    (*xctlState) = 0;
                    execLoadModule(currentNames->abendHandlers[i].abendProgname,1,0);
                } else
                if (currentNames->abendHandlers[i].abendHandler != NULL) {
                    (*cmdState) = 0;
                    (*xctlState) = 0;
                    (*currentNames->abendHandlers[i].abendHandler)();
                }
                currentNames->abendHandlers[i].abendIsActive = 1;
                return;
            }
        }
    }  

    char buf[56];
    int childfd = *((int*)pthread_getspecific(childfdKey));
    sprintf(buf,"%s","ABEND\n");
    write(childfd,buf,strlen(buf));
    sprintf(buf,"%s","ABCODE\n");
    write(childfd,buf,strlen(buf));
    sprintf(buf,"%s%s%s","='",abcode,"'\n\n");
    write(childfd,buf,strlen(buf));

    if ((*runState) == 3) {   // SEGV ABEND
        sprintf(response,"\n%s\n","STOP");
        write(childfd,&response,strlen(response));
    }
  }
  fprintf(stderr,"%s%s%s%d%s%d\n","ABEND ABCODE=",abcode," RESP=",resp," RESP2=",resp2);
  jmp_buf *h = condHandler[resp];
  if (h != NULL) {
    longjmp(*h,1);
  } else {
    longjmp(taskState,1);
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


// Handling plain COBOL call invocation for preprocessed QWICS modules
struct callLoadlib {
    char name[9];
    void* sdl_library;
    int (*loadmod)();    
};


// Db2 DSNTIAR assembler routine mockup
int dsntiar(unsigned char *commArea, unsigned char *sqlca, unsigned char *errMsg, int32_t *errLen) {
    return 0;
}


// EXEC XML GENERATE replacement
int xmlGenerate(unsigned char *xmlOutput, unsigned char *sourceRec, int32_t *xmlCharCount) {
    int childfd = *((int*)pthread_getspecific(childfdKey));
    char *commArea = (char*)pthread_getspecific(commAreaKey);

    write(childfd,"XML\n",4);
    write(childfd,"GENERATE\n",9);
    write(childfd,"SOURCE-REC\n",11);
    write(childfd,"XML-CHAR-COUNT\n",15);
    char lbuf[32];
    sprintf(lbuf,"%s%d\n","=",(int)*xmlCharCount);
    write(childfd,lbuf,strlen(lbuf));
    write(childfd,"\n",1);

    char c = 0x00;
    int pos = 0;
    while (pos < (int)(*xmlCharCount)) {
      int n = read(childfd,&c,1);
      if (n == 1) {
          xmlOutput[pos] = c;
          pos++;
      }    
    }

    char buf[2048];
    readLine((char*)&buf,childfd);
    int res = atoi(buf);
    readLine((char*)&buf,childfd);
    return res;
}


void* globalCallCallback(char *name) {
    void *res = NULL;
    char fname[255];
    char response[1024];
    int *callStackPtr = (int*)pthread_getspecific(callStackPtrKey);
    struct callLoadlib *callStack = (struct callLoadlib *)pthread_getspecific(callStackKey);
    int i = 0;
    #ifdef __APPLE__
    sprintf(fname,"%s%s%s%s",GETENV_STRING(loadmodDir,"QWICS_LOADMODDIR","../loadmod"),"/",name,".dylib");
    #else
    sprintf(fname,"%s%s%s%s",GETENV_STRING(loadmodDir,"QWICS_LOADMODDIR","../loadmod"),"/",name,".so");
    #endif

    if (strcmp("DSNTIAR",name) == 0) {
        return (void*)&dsntiar;
    }

    if (strcmp("xmlGenerate",name) == 0) {
        return (void*)&xmlGenerate;
    }

    for (i = 0; i < (*callStackPtr); i++) {
        if (strcmp(name,callStack[i].name) == 0) {
            return (void*)callStack[i].loadmod;
        }
    } 

    callStack[*callStackPtr].sdl_library = dlopen(fname, RTLD_LAZY);
    if (callStack[*callStackPtr].sdl_library == NULL) {
        sprintf(response,"%s%s%s\n","ERROR: Load module ",fname," not found!");
        printf("%s",response);
    } else {
        dlerror();
        *(void**)(&callStack[*callStackPtr].loadmod) = dlsym(callStack[*callStackPtr].sdl_library,name);
        char *error;
        if ((error = dlerror()) != NULL)  {
            dlclose(callStack[*callStackPtr].sdl_library);
            sprintf(response,"%s%s\n","ERROR: ",error);
            printf("%s",response);
            abend(27,1);
        } else {
            sprintf(callStack[*callStackPtr].name,"%s",name);
            res = (void*)callStack[*callStackPtr].loadmod;

            if (*callStackPtr < 1023) {
                (*callStackPtr)++;
            }
        }
    }

    return res; 
}


void globalCallCleanup() {
    int *callStackPtr = (int*)pthread_getspecific(callStackPtrKey);
    struct callLoadlib *callStack = (struct callLoadlib *)pthread_getspecific(callStackKey);

    int i = 0;
    for (i = (*callStackPtr)-1; i >= 0; i--) {
        dlclose(callStack[i].sdl_library);
    } 
    *callStackPtr = 0;
}


// Execute COBOL loadmod in transaction
int execLoadModule(char *name, int mode, int parCount) {
    int (*loadmod)();
    int (*abndhndl)();
    char fname[255];
    char response[1024];
    int childfd = *((int*)pthread_getspecific(childfdKey));
    char *commArea = (char*)pthread_getspecific(commAreaKey);
    struct currentNamesType *currentNames = ( struct currentNamesType*)pthread_getspecific(currentNamesKey);
    int res = 0;

    #ifdef _LIBTPMSERVER_
    struct callbackFuncType *cbInfo = (struct callbackFuncType*)pthread_getspecific(callbackFuncKey);
    if (cbInfo != NULL) {
        int *runState = (int*)pthread_getspecific(runStateKey);
        if (mode == 0) {
            sprintf(response,"%s\n","OK");
            write(childfd,&response,strlen(response));

            if ((*runState) < 3) {
                cbInfo->mode = 1;
            } else {
                cbInfo->mode = 0;
            }
        }
 
        int r = (*(cbInfo->callback))(name,(void*)cbInfo);

        if ((mode == 0) && ((*runState) < 3)) {
            sprintf(response,"\n%s\n","STOP");
            write(childfd,&response,strlen(response));
        }   
        return r;
    }
    #endif
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
        res = -1;
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
            res = -2;
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
            if (currentNames->abendHandlerCnt < MAX_ABENDHANDLERS) {
                *(void**)(&abndhndl) = dlsym(sdl_library,"ABNDHNDL");
                if ((error = dlerror()) != NULL) {
                    abndhndl = NULL;
                }
                currentNames->abendHandlers[currentNames->abendHandlerCnt].abendHandler = abndhndl;
                currentNames->abendHandlers[currentNames->abendHandlerCnt].abendProgname[0] = 0x00;
                currentNames->abendHandlers[currentNames->abendHandlerCnt].abendIsActive = 0;
                currentNames->abendHandlerCnt++;
            }

            if (mode == 0) {
              if (setjmp(taskState) == 0) {
                if (parCount > 0) {
                    if (parCount == 1) (*loadmod)(commArea,paramList[0]);
                    if (parCount == 2) (*loadmod)(commArea,paramList[0],paramList[1]);
                    if (parCount == 3) (*loadmod)(commArea,paramList[0],paramList[1],paramList[2]);
                    if (parCount == 4) (*loadmod)(commArea,paramList[0],paramList[1],paramList[2],paramList[3]);
                    if (parCount == 5) (*loadmod)(commArea,paramList[0],paramList[1],paramList[2],paramList[3],paramList[4]);
                    if (parCount == 6) (*loadmod)(commArea,paramList[0],paramList[1],paramList[2],paramList[3],paramList[4],
                                                            paramList[5]);
                    if (parCount == 7) (*loadmod)(commArea,paramList[0],paramList[1],paramList[2],paramList[3],paramList[4],
                                                            paramList[5],paramList[6]);
                    if (parCount == 8) (*loadmod)(commArea,paramList[0],paramList[1],paramList[2],paramList[3],paramList[4],
                                                            paramList[5],paramList[6],paramList[7]);
                    if (parCount == 9) (*loadmod)(commArea,paramList[0],paramList[1],paramList[2],paramList[3],paramList[4],
                                                            paramList[5],paramList[6],paramList[7],paramList[8]);
                    if (parCount == 10) (*loadmod)(commArea,paramList[0],paramList[1],paramList[2],paramList[3],paramList[4],
                                                            paramList[5],paramList[6],paramList[7],paramList[8],paramList[9]);
                } else {
                    (*loadmod)(commArea);
                }
              }
            } else {
              cob_get_global_ptr()->cob_current_module = &thisModule;
              cob_get_global_ptr()->cob_call_params = 1;
              (*loadmod)(commArea);
            }

            if (currentNames->abendHandlerCnt > 0) {
                currentNames->abendHandlers[currentNames->abendHandlerCnt].abendHandler = NULL;
                currentNames->abendHandlers[currentNames->abendHandlerCnt].abendProgname[0] = 0x00;
                currentNames->abendHandlers[currentNames->abendHandlerCnt].abendIsActive = 0;
                currentNames->abendHandlerCnt--;
            }
#ifndef _USE_ONLY_PROCESSES_
            endModule(name);
#endif
            int *runState = (int*)pthread_getspecific(runStateKey);
            if ((mode == 0) && ((*runState) < 3)) {
                sprintf(response,"\n%s\n","STOP");
                write(childfd,&response,strlen(response));
            }
        }
        dlclose(sdl_library);
    }
    return res;
}


int execCallback(char *cmd, void *var) {
    int childfd = *((int*)pthread_getspecific(childfdKey));
    char *cmdbuf = (char*)pthread_getspecific(cmdbufKey);
    int *cmdState = (int*)pthread_getspecific(cmdStateKey);
    int *runState = (int*)pthread_getspecific(runStateKey);
    cob_field **outputVars = (cob_field**)pthread_getspecific(cobFieldKey);
    char *end = &cmdbuf[strlen(cmdbuf)];
    int *xctlState = (int*)pthread_getspecific(xctlStateKey);
    int *retrieveState = (int*)pthread_getspecific(retrieveStateKey);
    char **xctlParams = (char**)pthread_getspecific(xctlParamsKey);
    char *eibbuf = (char*)pthread_getspecific(eibbufKey);
    char *eibaid = (char*)pthread_getspecific(eibaidKey);
    char *linkArea = (char*)pthread_getspecific(linkAreaKey);
    int *linkAreaPtr = (int*)pthread_getspecific(linkAreaPtrKey);
    char **linkAreaAdr = (char**)pthread_getspecific(linkAreaAdrKey);
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
    int *callStackPtr = (int*)pthread_getspecific(callStackPtrKey);
    void *isamTx = (void*)pthread_getspecific(isamTxKey);
    struct currentNamesType *currentNames = ( struct currentNamesType*)pthread_getspecific(currentNamesKey);
    int respFieldsStateLocal = 0;
    void *respFieldsLocal[2];

    struct taskLock *taskLocks = (struct taskLock *)pthread_getspecific(taskLocksKey);

    printf("%s %s %d %d %x %d %x %x %x\n","execCallback",cmd,*cmdState,*memParamsState,var,(*callStackPtr),isamTx,var,memParams[5]);

    if (strstr(cmd,"SET SQLCODE") && (var != NULL)) {
        sqlcode = var;
        return 1;
    }
    if (strstr(cmd,"SET EIBCALEN") && (((*linkStackPtr) == 0) && ((*callStackPtr) == 0))) {
        cob_field *cobvar = (cob_field*)var;
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
    if (strstr(cmd,"SET EIBAID") && (((*linkStackPtr) == 0) && ((*callStackPtr) == 0))) {
        (*commAreaPtr) = 0;
        (*areaMode) = 0;
        // Handle EIBAID
        cob_field *cobvar = (cob_field*)var;
        if (cobvar->data != NULL) {
            eibaid = (char*)cobvar->data;
            pthread_setspecific(eibaidKey, eibaid);
        }
        // Read in client response value
        char buf[2048];
        readLine((char*)&buf,childfd);
        cob_put_picx(cobvar->data,(size_t)cobvar->size,buf);
        return 1;
    }
    if (strstr(cmd,"SET DFHEIBLK") && (((*linkStackPtr) == 0) && ((*callStackPtr) == 0))) {
        cob_field *cobvar = (cob_field*)var;
        if (cobvar->data != NULL) {
            eibbuf = (char*)cobvar->data;
            pthread_setspecific(eibbufKey, eibbuf);
        }
        // Read in TRNID from client
        char c = 0x00;
        int pos = 8;
        while (c != '\n') {
            int n = read(childfd,&c,1);
            if ((n == 1) && (pos < 12) && (c != '\n') && (c != '\r') && (c != '\'')) {
                eibbuf[pos] = c;
                pos++;
            }
        }
        while (pos < 12) {
            eibbuf[pos] = ' ';
            pos++;
        }
        // Read in REQID from client
        c = 0x00;
        pos = 43;
        while (c != '\n') {
            int n = read(childfd,&c,1);
            if ((n == 1) && (pos < 51) && (c != '\n') && (c != '\r') && (c != '\'')) {
                eibbuf[pos] = c;
                pos++;
            }
        }
        while (pos < 51) {
            eibbuf[pos] = ' ';
            pos++;
        }
        // Read in TERMID from client
        c = 0x00;
        pos = 16;
        while (c != '\n') {
            int n = read(childfd,&c,1);
            if ((n == 1) && (pos < 20) && (c != '\n') && (c != '\r') && (c != '\'')) {
                eibbuf[pos] = c;
                pos++;
            }
        }
        while (pos < 20) {
            eibbuf[pos] = '0';
            pos++;
        }
        // Read in TASKID from client
        char idbuf[9];
        c = 0x00;
        pos = 0;
        while (c != '\n') {
            int n = read(childfd,&c,1);
            if ((n == 1) && (pos < 8) && (c != '\n') && (c != '\r') && (c != '\'')) {
                idbuf[pos] = c;
                pos++;
            }
        }
        idbuf[pos] = 0x00;
        int id = atoi(idbuf);
        cob_put_s64_comp3(id,(void*)&eibbuf[12],4);
        // SET EIBDATE and EIBTIME
        time_t t = time(NULL);
        struct tm now = *localtime(&t);
        int ti = now.tm_hour*10000 + now.tm_min*100 + now.tm_sec;
        cob_put_s64_comp3(ti,(void*)&eibbuf[0],4);
        int da = now.tm_year*1000 + now.tm_yday;
        cob_put_s64_comp3(da,(void*)&eibbuf[4],4);
        return 1;
    } else 
    if (strstr(cmd,"SET DFHEIBLK") && (((*linkStackPtr) >= 0) || ((*callStackPtr) >= 0))) {
        // Called by LINk inside transaction, pass through EIB
        cob_field *cobvar = (cob_field*)var;
        if (cobvar->data != NULL) {
            int n = 0;
            for (n = 0; n < cobvar->size; n++) {
                ((char*)cobvar->data)[n] = eibbuf[n];
            }
        }
        return 1;
    }

    if (strstr(cmd,"SETL1 1 ") || strstr(cmd,"SETL0 1 ") || strstr(cmd,"SETL0 77")) {
        (*areaMode) = 0;
    }
    if (strstr(cmd,"DFHCOMMAREA")) {
        (*areaMode) = 1;
    }
    if (strstr(cmd,"SETL0") || strstr(cmd,"SETL1")) {
        cob_field *cobvar = (cob_field*)var;
        if ((*areaMode) == 0) {
            if (strstr(cmd,"SETL1 1 ") || strstr(cmd,"SETL0 1 ") || strstr(cmd,"SETL0 77")) {
                // Top level var
                // printf("data %x\n",cobvar->data);
                if (cobvar->data == NULL) {
                    cobvar->data = (unsigned char*)&linkArea[*linkAreaPtr];
                    (*linkAreaAdr) = &linkArea[*linkAreaPtr];
                    (*linkAreaPtr) += (size_t)cobvar->size;
                    // printf("set top level linkAreaPtr %x\n",cobvar->data);
                }
            } else {
                // printf("linkAreaPtr = %d\n",*linkAreaPtr);
                if ((unsigned long)(*linkAreaAdr) + (unsigned long)cobvar->data < (unsigned long)&linkArea[*linkAreaPtr]) {
                    cobvar->data = (unsigned char*)(*linkAreaAdr) + (unsigned long)cobvar->data;
                    // printf("set sub level linkAreaPtr %x\n",cobvar->data);
                }
            }
        } else {
            if (cobvar->data == NULL) {
                cobvar->data = (unsigned char*)&commArea[*commAreaPtr];
                (*commAreaPtr) += (size_t)cobvar->size;
            }
        }

        if ((strstr(cmd,"SETL0 77") || strstr(cmd,"SETL1 1 ")) && 
            (((*linkStackPtr) == 0) && ((*callStackPtr) == 0)) && ((*areaMode) == 0)) {
/*
            cob_field *cobvar = (cob_field*)var;
            char obuf[255];
            sprintf(obuf,"%s %ld\n",cmd,cobvar->size);
            write(childfd,obuf,strlen(obuf));
*/
            // Read in value from client
/*        
            char lvar[65536];
            char c = 0x00;
            int pos = 0;
            while (c != '\n') {
                int n = read(childfd,&c,1);
                if ((n == 1) && (c != '\n') && (c != '\r') && (c != '\'') && (pos < 65536)) {
                    lvar[pos] = c;
                    pos++;
                }
            }
            lvar[pos] = 0x00; 

            char buf[256];
            long v = 0;
            switch (COB_FIELD_TYPE(cobvar)) {
                case COB_TYPE_NUMERIC:          cob_put_picx(cobvar->data,cobvar->size,
                                                            convertNumeric(lvar,cobvar->attr->digits,cobvar->attr->scale,buf));
                                                break;
                case COB_TYPE_NUMERIC_BINARY:   v = atol(lvar);
                                                cob_put_u64_compx(v,cobvar->data,cobvar->size);
                                                break;
                case COB_TYPE_NUMERIC_PACKED:   v = atol(lvar);
                                                cob_put_s64_comp3(v,cobvar->data,cobvar->size);                     
                                                break;
                default:                        cob_put_picx(cobvar->data,(size_t)cobvar->size,lvar);
            }
 */       
        }
        return 1;
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
            (*cmdState) = -1;
            (*memParamsState) = 0;
            (*respFieldsState) = 0;
            *((int*)memParams[0]) = -1;
            currentNames->currentMap[0] = 0x00;
            currentNames->currentMapSet[0] = 0x00;
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
            xctlParams[1] = NULL;
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
            memParams[2] = (void*)0;
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
            memParams[1] = NULL;
            memParams[2] = NULL;
            memParams[3] = NULL;
            memParams[4] = NULL;
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
        if ((strcmp(cmd,"ABEND") == 0) && ((*cmdState) != -27)) {
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
            ((strcmp(cmd,"CANCEL") == 0) && ((*cmdState) != -27))) {
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
        if (strcmp(cmd,"RETURN") == 0) {
            sprintf(cmdbuf,"%s%s",cmd,"\n");
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
            (*cmdState) = -20; // RETURN
            (*memParamsState) = 0;
            *((int*)memParams[0]) = -1;
            memParams[1] = NULL;
            memParams[2] = NULL;
            (*respFieldsState) = 0;
            respFields[0] = NULL;
            respFields[1] = NULL;
            (*runState) = 2; // TASK ENDED
            return 1;
        }
        if (strcmp(cmd,"SOAPFAULT") == 0) {
            sprintf(cmdbuf,"%s%s",cmd,"\n");
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
            (*cmdState) = -21;
            (*memParamsState) = 0;
            *((int*)memParams[0]) = -1;
            (*respFieldsState) = 0;
            respFields[0] = NULL;
            respFields[1] = NULL;
            return 1;
        }
        if (strcmp(cmd,"INVOKE") == 0) {
            sprintf(cmdbuf,"%s%s",cmd,"\n");
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
            (*cmdState) = -22;
            (*memParamsState) = 0;
            *((int*)memParams[0]) = -1;
            (*respFieldsState) = 0;
            respFields[0] = NULL;
            respFields[1] = NULL;
            return 1;
        }
        if (strcmp(cmd,"QUERY") == 0) {
            sprintf(cmdbuf,"%s%s",cmd,"\n");
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
            (*cmdState) = -23;
            (*memParamsState) = 0;
            *((int*)memParams[0]) = -1;
            memParams[1] = NULL;
            memParams[2] = NULL;
            memParams[3] = NULL;
            memParams[4] = NULL;
            (*respFieldsState) = 0;
            respFields[0] = NULL;
            respFields[1] = NULL;
            return 1;
        }
        if (strcmp(cmd,"STARTBR") == 0) {
            sprintf(cmdbuf,"%s%s",cmd,"\n");
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
            (*cmdState) = -24;
            (*memParamsState) = 0;
            memParams[2] = &currentNames->memParamInts[2];
            memParams[3] = &currentNames->memParamInts[3];
            *((int*)memParams[0]) = -1;
            memParams[1] = NULL;
            *((int*)memParams[2]) = 0;
            *((int*)memParams[3]) = 0;
            (*respFieldsState) = 0;
            respFields[0] = NULL;
            respFields[1] = NULL;
            return 1;
        }
        if (strcmp(cmd,"ENDBR") == 0) {
            sprintf(cmdbuf,"%s%s",cmd,"\n");
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
            (*cmdState) = -25;
            (*memParamsState) = 0;
            memParams[1] = &currentNames->memParamInts[1];
            *((int*)memParams[0]) = -1;
            *((int*)memParams[1]) = 0;
            (*respFieldsState) = 0;
            respFields[0] = NULL;
            respFields[1] = NULL;
            return 1;
        }
        if ((strcmp(cmd,"READ") == 0) ||
            (strcmp(cmd,"READNEXT") == 0) ||   
            (strcmp(cmd,"READPREV") == 0) || 
            (strcmp(cmd,"WRITE") == 0) || 
            (strcmp(cmd,"REWRITE") == 0) || 
            (strcmp(cmd,"DELETE") == 0)) {
            sprintf(cmdbuf,"%s%s",cmd,"\n");
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
            (*cmdState) = -26;
            (*memParamsState) = 0;
            memParams[2] = &currentNames->memParamInts[2];
            memParams[3] = &currentNames->memParamInts[3];
            memParams[4] = &currentNames->memParamInts[4];
            memParams[7] = &currentNames->memParamInts[7];
            *((int*)memParams[0]) = -1;
            memParams[1] = NULL;
            *((int*)memParams[2]) = 0;
            *((int*)memParams[3]) = 0;
            if (strcmp(cmd,"READ") == 0) {
                *((int*)memParams[4]) = 1;
            }
            if (strcmp(cmd,"READNEXT") == 0) {
                *((int*)memParams[4]) = 2;
            }
            if (strcmp(cmd,"READPREV") == 0) {
                *((int*)memParams[4]) = 3;
            }
            if (strcmp(cmd,"WRITE") == 0) {
                *((int*)memParams[4]) = 4;
            }
            if (strcmp(cmd,"REWRITE") == 0) {
                *((int*)memParams[4]) = 5;
            }
            if (strcmp(cmd,"DELETE") == 0) {
                *((int*)memParams[4]) = 6;
            }
            memParams[5] = NULL;
            memParams[6] = NULL;
            *((int*)memParams[7]) = -1;
            memParams[8] = NULL;
            (*respFieldsState) = 0;
            respFields[0] = NULL;
            respFields[1] = NULL;
            return 1;
        }
        if (strcmp(cmd,"HANDLE") == 0) {
            sprintf(cmdbuf,"%s%s",cmd,"\n");
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
            (*cmdState) = -27;
            (*memParamsState) = 0;
            *((int*)memParams[0]) = -1;
            memParams[1] = NULL;
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

            if (((*cmdState) == -1) && ((*memParamsState) >= 1)) {
                writeJson(currentNames->currentMap,currentNames->currentMapSet,childfd);

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
                if (resp > 0) {
                  abend(resp,resp2);
                }
            }
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
                // Read EIBAID
                readLine((char*)&buf,childfd);
                cob_put_picx(eibaid,1,buf);

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
                execLoadModule(xctlParams[0],1,0);
            }
            if (((*cmdState) == -4) && ((*retrieveState) >= 1)) {
                // RETRIEVE
                (*retrieveState) = 0;

                char buf[2048];
                readLine((char*)&buf,childfd);
                resp = atoi(buf);
                readLine((char*)&buf,childfd);
                resp2 = atoi(buf);
                if (resp > 0) {
                  abend(resp,resp2);
                }
            }
            if (((*cmdState) == -5) && ((*xctlState) >= 1)) {
                // LINK
                (*xctlState) = 0;
                (*cmdState) = 0;
                sprintf(&(linkStack[9*(*linkStackPtr)]),"%s",xctlParams[0]);
                if ((*linkStackPtr) < 99) {
                  (*linkStackPtr)++;
                }
                if (xctlParams[1] != NULL) {
                    cob_field *cobvar = (cob_field*)xctlParams[1];
                    if (((int)cobvar->size < 0) || (cobvar->size > 32768)) {
                        resp = 22;
                        resp2 = 11;
                    }
                }
                if (resp == 0) {
                    respFieldsStateLocal = *respFieldsState;
                    respFieldsLocal[0] = respFields[0];
                    respFieldsLocal[1] = respFields[1];
                    cob_field *cobvar = (cob_field*)xctlParams[1];

                    int r = execLoadModule(xctlParams[0],1,0);

                    *respFieldsState = respFieldsStateLocal;
                    respFields[0] = respFieldsLocal[0];
                    respFields[1] = respFieldsLocal[1];
                    *cmdState = -5;

                    if (r < 0) {
                        resp = 27;
                        resp2 = 3;
                    }
                    if ((resp == 0) && (cobvar != NULL)) {
                        for (int i = 0; i < cobvar->size; i++) {
                            cobvar->data[i] = commArea[i];
                        }
                    }
                }
                if ((*linkStackPtr) > 0) {
                  (*linkStackPtr)--;
                }
                if (resp > 0) {
                  abend(resp,resp2);
                }
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
                if (freemain(memParams[1]) < 0) {
                    resp = 16;
                    resp2 = 1;
                    abend(resp,resp2);
                }
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
                char buf[2048];
                int len = *((int*)memParams[0]);
                cob_field *cobvar = NULL, dummy = { len, NULL, NULL };
                if (memParams[1] != NULL) {
                    cobvar = (cob_field*)memParams[1];
                } 
                if (memParams[2] != NULL) {
                    // SET mode
                    readLine((char*)&buf,childfd);
                    len = atoi(buf);

                    (*((unsigned char**)((cob_field*)memParams[2])->data)) = getNextChnBuf(len);
                    dummy.size = len;
                    dummy.data = (*((unsigned char**)((cob_field*)memParams[2])->data));
                    cobvar = &dummy;
                }     
                if (memParams[4] != NULL) {
                    // NODATA mode
                    readLine((char*)&buf,childfd);
                    len = atoi(buf);
                    dummy.size = len;
                    cobvar = &dummy;                    
                }
                int i,l = 0;
                if (cobvar != NULL) {
                    if ((len >= 0) && (len <= cobvar->size)) {
                        l = len;
                    } else {
                        l = cobvar->size;
                    }
                }
                if (memParams[3] != NULL) {
                    if (((cob_field*)memParams[3])->data != NULL) {
                        setNumericValue(l,(cob_field*)memParams[3]);                        
                    }
                }
                if (memParams[4] != NULL) {
                    // NODATA mode
                    l = 0;
                    len = 0;
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
                      _execSql(sql, pthread_getspecific(childfdKey),1,1);
                    }
                  }
                }
                releaseLocks(UOW, taskLocks);
                if ((strstr(buf,"ROLLBACK") != NULL) && ((*memParamsState) == 0)) {
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
                if (memParams[3] != NULL) {
                    if (getCobType((cob_field*)memParams[3]) == COB_TYPE_NUMERIC_BINARY) {
                        cob_put_u64_compx(item,((cob_field*)memParams[3])->data,2);
                    }
                    if (getCobType((cob_field*)memParams[3]) == COB_TYPE_NUMERIC_COMP5) {
                        cob_put_s64_comp5(item,((cob_field*)memParams[3])->data,2);
                    }
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
                if (memParams[3] != NULL) {
                    if (getCobType((cob_field*)memParams[3]) == COB_TYPE_NUMERIC_BINARY) {
                        cob_put_u64_compx(item,((cob_field*)memParams[3])->data,2);
                    }
                    if (getCobType((cob_field*)memParams[3]) == COB_TYPE_NUMERIC_COMP5) {
                        cob_put_s64_comp5(item,((cob_field*)memParams[3])->data,2);
                    }
                }
                readLine((char*)&buf,childfd);
                resp = atoi(buf);
                readLine((char*)&buf,childfd);
                resp2 = atoi(buf);
                if (resp > 0) {
                  abend(resp,resp2);
                  if (COB_FIELD_TYPE(cobvar) == COB_TYPE_ALPHANUMERIC) {
                      memset(cobvar->data,' ',cobvar->size);
                  } else {
                      memset(cobvar->data,0x00,cobvar->size);
                  }
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
            if ((*cmdState) == -21) {
                char buf[2048];
                readLine((char*)&buf,childfd);
                resp = atoi(buf);
                readLine((char*)&buf,childfd);
                resp2 = atoi(buf);
                if (resp > 0) {
                  abend(resp,resp2);
                }
            }
            if ((*cmdState) == -22) {
                char buf[2048];
                readLine((char*)&buf,childfd);
                resp = atoi(buf);
                readLine((char*)&buf,childfd);
                resp2 = atoi(buf);
                if (resp > 0) {
                  abend(resp,resp2);
                }
            }
            if ((*cmdState) == -23) {
                char buf[2048];
                // READ
                readLine((char*)&buf,childfd);
                int v = atoi(buf);
                if (memParams[1] != NULL) {
                    if (((cob_field*)memParams[1])->data != NULL) {
                        setNumericValue(v,(cob_field*)memParams[1]);                        
                    }
                }
                // UPDATE
                readLine((char*)&buf,childfd);
                v = atoi(buf);
                if (memParams[2] != NULL) {
                    if (((cob_field*)memParams[2])->data != NULL) {
                        setNumericValue(v,(cob_field*)memParams[2]);                        
                    }
                }
                // CONTROL
                readLine((char*)&buf,childfd);
                v = atoi(buf);
                if (memParams[3] != NULL) {
                    if (((cob_field*)memParams[3])->data != NULL) {
                        setNumericValue(v,(cob_field*)memParams[3]);                        
                    }
                }
                // ALTER
                readLine((char*)&buf,childfd);
                v = atoi(buf);
                if (memParams[4] != NULL) {
                    if (((cob_field*)memParams[4])->data != NULL) {
                        setNumericValue(v,(cob_field*)memParams[4]);                        
                    }
                }
                readLine((char*)&buf,childfd);
                resp = atoi(buf);
                readLine((char*)&buf,childfd);
                resp2 = atoi(buf);
                if (resp > 0) {
                  abend(resp,resp2);
                }
            }            
            if (((*cmdState) == -24) && ((*memParamsState) >= 1)) {
                struct openDatasetType *ods;
                void* ds = getOpenDataset(currentNames->openDatasets,currentNames->fileName,&ods);
                if (ds == NULL) {
                    ds = openDataset(currentNames->fileName);
                    if (ds != NULL) {
                        putOpenDataset(currentNames->openDatasets,currentNames->fileName,ds);
                    } else {
                        resp = 12;
                        resp2 = 1;
                    }
                }
                if (ds != NULL) {
                    int reqId = *((int*)memParams[2]);
                    struct openCursorType *cur = getOpenCursor(currentNames->openDatasets,currentNames->fileName,reqId);
                    if (cur != NULL) {
                        resp = 16;
                        resp2 = 33;
                    } else {
                        void* curptr = NULL;
                        if (openCursor(ds,isamTx,&curptr,0,0) != 0) {
                            resp = 17;
                            resp2 = 120;
                        } else {    
                            unsigned char *rid = NULL; 
                            int keylen = 0;
                            if (memParams[1] != NULL) {
                                // RIDFLD
                                rid = (unsigned char*)((cob_field*)memParams[1])->data;
                                keylen = ((cob_field*)memParams[1])->size;
                            }
                            if ((rid != NULL) && (*((int*)memParams[0]) >= 0)) {
                                if ((keylen == 0) || ((*((int*)memParams[0]) < keylen))) {
                                    keylen = *((int*)memParams[0]);
                                }
                            }

                            if (!putOpenCursor(currentNames->openDatasets,
                                                currentNames->fileName,
                                                reqId,curptr)) {
                                resp = 17;
                                resp2 = 120;
                            } else {
                                if (rid != NULL) {
                                    if (get(ds,isamTx,curptr,rid,keylen,NULL,0,MODE_SET) != 0) {
                                        resp = 13;
                                        resp2 = 80;
                                    } else {
                                        cur = getOpenCursor(currentNames->openDatasets,currentNames->fileName,reqId);
                                        if (cur != NULL) {
                                            setCursorRid(cur,rid,keylen);
                                        } else {
                                            resp = 17;
                                            resp2 = 120; 
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                if (resp > 0) {
                  abend(resp,resp2);
                }
            }
            if (((*cmdState) == -25) && ((*memParamsState) >= 1)) {
                struct openDatasetType *ods;
                void* ds = getOpenDataset(currentNames->openDatasets,currentNames->fileName,&ods);
                if (ds == NULL) {
                    resp = 12;
                    resp2 = 1;
                } else {
                    int reqId = *((int*)memParams[1]);
                    struct openCursorType *cur = getOpenCursor(currentNames->openDatasets,currentNames->fileName,reqId);
                    if (cur != NULL) {
                        if (closeCursor(cur->curHandle) != 0) {
                            resp = 17;
                            resp2 = 120; 
                        }
                        cur->curHandle = NULL;
                    } else {
                        resp = 16;
                        resp2 = 35;
                    }
                }
                if (resp > 0) {
                  abend(resp,resp2);
                }
            }
            if (((*cmdState) == -26) && ((*memParamsState) >= 1)) {
                struct openDatasetType *ods;
                void* ds = getOpenDataset(currentNames->openDatasets,currentNames->fileName,&ods);
                if (ds == NULL) {
                    ds = openDataset(currentNames->fileName);
                    if (ds != NULL) {
                        putOpenDataset(currentNames->openDatasets,currentNames->fileName,ds);
                    } else {
                        resp = 12;
                        resp2 = 1;
                    }
                }

                if (ds != NULL) {
                    unsigned char *rid = NULL; 
                    int keylen = 0;
                    if (memParams[1] != NULL) {
                        // RIDFLD
                        rid = (unsigned char*)((cob_field*)memParams[1])->data;
                        keylen = ((cob_field*)memParams[1])->size;
                    } 
                    if ((rid != NULL) && (*((int*)memParams[0]) >= 0)) {
                        if ((keylen == 0) || ((*((int*)memParams[0]) < keylen))) {
                            keylen = *((int*)memParams[0]);
                        }
                    }

                    unsigned char *rec = NULL; 
                    int len = 0;
                    if (memParams[6] != NULL) {
                        // SET mode
                        rec = *((unsigned char**)((cob_field*)memParams[6])->data);
                    } else {
                        if (memParams[5] != NULL) {
                            // FROM/INTO mode
                            rec = (unsigned char*)((cob_field*)memParams[5])->data;
                            len = ((cob_field*)memParams[5])->size;
                        } else {
                            resp = 17;
                            resp2 = 120;                                
                        }
                    }
                    if ((rec != NULL) && (*((int*)memParams[7]) >= 0)) {
                        if ((len == 0) || ((*((int*)memParams[7]) < len))) {
                            len = *((int*)memParams[7]);
                            resp = 22;
                            resp2 = 11;
                        }
                    }

                    int reqId = *((int*)memParams[2]);
                    struct openCursorType *cur = NULL;
                    if (rec != NULL) {
                        switch (*((int*)memParams[4])) {
                            case 1: // READ
                                    if (rid != NULL) {
                                        if (get(ds,isamTx,NULL,rid,keylen,rec,len,MODE_SET) != 0) {
                                            resp = 13;
                                            resp2 = 80; 
                                        } else {
                                            if (*((int*)memParams[3]) & 2048) {
                                                // UPDATE set
                                                setCursorRid(&ods->rewriteCur,rid,keylen);
                                                ods->rewriteCur.id = 0;
                                                ods->rewriteCur.curHandle = NULL;
                                            }
                                        }
                                    } else {
                                        resp = 13;
                                        resp2 = 80;    
                                    }
                                    break;

                            case 2: // READNEXT
                                    cur = getOpenCursor(currentNames->openDatasets,currentNames->fileName,reqId);
                                    if (cur != NULL) {
                                        int r = 0;
                                        if (equalsCursorRid(cur,rid,keylen)) {
                                            r = get(ds,isamTx,cur->curHandle,rid,keylen,rec,len,MODE_NEXT);
                                        } else {
                                            r = get(ds,isamTx,cur->curHandle,rid,keylen,rec,len,MODE_SET);
                                            if (r == 0) {
                                                setCursorRid(cur,rid,keylen);
                                                if (*((int*)memParams[3]) & 2048) {
                                                    // UPDATE set
                                                    setCursorRid(&ods->rewriteCur,rid,keylen);
                                                    ods->rewriteCur.id = reqId;
                                                    ods->rewriteCur.curHandle = cur->curHandle;
                                                }
                                            }
                                        }
                                        if (r != 0) {
                                            resp = 13;
                                            resp2 = 80;  
                                        }
                                    } else {
                                        resp = 16;
                                        resp2 = 34;
                                    }
                                    break;

                            case 3: // READPREV
                                    cur = getOpenCursor(currentNames->openDatasets,currentNames->fileName,reqId);
                                    if (cur != NULL) {
                                        int r = 0;
                                        if (equalsCursorRid(cur,rid,keylen)) {
                                            r = get(ds,isamTx,cur->curHandle,rid,keylen,rec,len,MODE_PREV);
                                        } else {
                                            r = get(ds,isamTx,cur->curHandle,rid,keylen,rec,len,MODE_SET);
                                            if (r == 0) {
                                                setCursorRid(cur,rid,keylen);
                                                if (*((int*)memParams[3]) & 2048) {
                                                    // UPDATE set
                                                    setCursorRid(&ods->rewriteCur,rid,keylen);
                                                    ods->rewriteCur.id = reqId;
                                                    ods->rewriteCur.curHandle = cur->curHandle;
                                                }
                                            }
                                        }
                                        if (r != 0) {
                                            resp = 13;
                                            resp2 = 80;  
                                        }
                                    } else {
                                        resp = 16;
                                        resp2 = 34;
                                    }
                                    break;

                            case 4: // WRITE
                                    if (rid != NULL) {
                                        if (put(ds,isamTx,NULL,rid,keylen,rec,len) != 0) {
                                            resp = 17;
                                            resp2 = 120; 
                                        }
                                    } else {
                                        resp = 17;
                                        resp2 = 120;    
                                    }
                                    break;

                            case 5: // REWRITE
                                    if (ods->rewriteCur.id >= 0) {
                                        if (put(ds,isamTx,ods->rewriteCur.curHandle,ods->rewriteCur.rid,ods->rewriteCur.keylen,rec,len) != 0) {
                                            resp = 17;
                                            resp2 = 120; 
                                        } 
                                        ods->rewriteCur.id = -1;
                                        ods->rewriteCur.curHandle = NULL;
                                        ods->rewriteCur.keylen = 0;              
                                    } else {
                                        resp = 16;
                                        resp2 = 30;
                                    }
                                    break;

                            case 6: // DELETE
                                    if (rid != NULL) {
                                        if (del(ds,isamTx,rid,keylen) != 0) {
                                            resp = 13;
                                            resp2 = 80; 
                                        }
                                    } else {
                                        resp = 13;
                                        resp2 = 80;    
                                    }
                                    break;
                        }
                    } else {
                        resp = 17;
                        resp2 = 120;
                    }
                } else {
                    resp = 12;
                    resp2 = 1;
                }
                if (resp > 0) {
                  abend(resp,resp2);
                }
            }
            if (((*cmdState) == -27) && ((*memParamsState) >= 1)) {
                if (currentNames->abendHandlerCnt > 0) {
                    switch (*((int*)memParams[0])) {
                        case 1: // CANCEL
                                currentNames->abendHandlers[currentNames->abendHandlerCnt-1].abendIsActive = 0;
                                break;
                        case 2: // PROGRAM 
                                currentNames->abendHandlers[currentNames->abendHandlerCnt-1].abendIsActive = 1;
                                if (memParams[1] != NULL) {
                                    cob_field *cobvar = (cob_field*)memParams[1]; 
                                    if (cobvar->data != NULL) {
                                        int l = cobvar->size;
                                        int i = 0, j = 0;
                                        for (i = 0; i < l && j < 8; i++) {
                                            if (cobvar->data[i] != ' ') {
                                                currentNames->abendHandlers[currentNames->abendHandlerCnt-1].abendProgname[j] = cmd[i];
                                                j++;
                                            }         
                                        }
                                        currentNames->abendHandlers[currentNames->abendHandlerCnt-1].abendProgname[j] = 0x00;
                                    }                        
                                }
                                break;
                        case 3: // LABEL
                                currentNames->abendHandlers[currentNames->abendHandlerCnt-1].abendIsActive = 1;
                                break;
                        case 4: // RESET
                                currentNames->abendHandlers[currentNames->abendHandlerCnt-1].abendIsActive = 1;
                                break;
                    }
                }

                if (resp > 0) {
                  abend(resp,resp2);
                }
            }

            // SET EIBRESP and EIBRESP2
            cob_put_u64_compx(resp,&eibbuf[76],4);
            cob_put_u64_compx(resp2,&eibbuf[80],4);

            if ((*respFieldsState) == 1) {
              setNumericValue(resp,(cob_field*)respFields[0]);
            }
            if ((*respFieldsState) == 2) {
              setNumericValue(resp,(cob_field*)respFields[0]);
              setNumericValue(resp2,(cob_field*)respFields[1]);
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
            strstr(cmd,"CWA") || strstr(cmd,"TWA") || (strstr(cmd,"EIB") && !strstr(cmd,"EIBAID")) || strstr(cmd,"TCTUA") || strstr(cmd,"TCTUALENG") || strstr(cmd,"PUT") || strstr(cmd,"GET") ||
            strstr(cmd,"CONTAINER") || strstr(cmd,"CHANNEL") || strstr(cmd,"BYTEOFFSET") || strstr(cmd,"NODATA-FLENGTH") ||
            strstr(cmd,"INTOCCSID") || strstr(cmd,"INTOCODEPAGE") || strstr(cmd,"CONVERTST") || strstr(cmd,"CCSID") ||
            strstr(cmd,"FROMCCSID") || strstr(cmd,"FROMCODEPAGE") || strstr(cmd,"DATATYPE") ||
            strstr(cmd,"APPEND") || strstr(cmd,"BIT") || strstr(cmd,"CHAR") || strstr(cmd,"CANCEL") ||
            strstr(cmd,"RESP") || strstr(cmd,"RESP2") || strstr(cmd,"RESOURCE") || strstr(cmd,"UOW") ||
            strstr(cmd,"TASK") || strstr(cmd,"NOSUSPEND") || strstr(cmd,"INITIMG") ||
            strstr(cmd,"USERDATAKEY") || strstr(cmd,"CICSDATAKEY") || strstr(cmd,"MAXLIFETIME") ||
            strstr(cmd,"ROLLBACK") || strstr(cmd,"ITEM") || strstr(cmd,"QUEUE") || 
            strstr(cmd,"TS") || strstr(cmd,"TD") || strstr(cmd,"REWRITE") || strstr(cmd,"NEXT") ||
            strstr(cmd,"QNAME") || strstr(cmd,"MAIN") || strstr(cmd,"AUXILIARY") || strstr(cmd,"ABSTIME") ||
            strstr(cmd,"YYMMDD") || strstr(cmd,"YEAR") || strstr(cmd,"TIME") || strstr(cmd,"DDMMYY") ||
            strstr(cmd,"DATESEP") || strstr(cmd,"TIMESEP") || strstr(cmd,"DB2CONN") || strstr(cmd,"CONNECTST") ||
            strstr(cmd,"TRANSID") || strstr(cmd,"REQID") || strstr(cmd,"INTERVAL") || strstr(cmd,"USERID") || 
            strstr(cmd,"NOHANDLE") || strstr(cmd,"CREATE") || strstr(cmd,"CLIENT") || strstr(cmd,"SERVER") || 
            strstr(cmd,"SENDER") || strstr(cmd,"RECEIVER") || strstr(cmd,"FAULTCODE") || strstr(cmd,"FAULTCODESTR") || 
            strstr(cmd,"FAULTCODELEN") || strstr(cmd,"FAULTSTRING") || strstr(cmd,"FAULTSTRLEN") || strstr(cmd,"NATLANG") || 
            strstr(cmd,"FAULTCODE") || strstr(cmd,"ROLE") || strstr(cmd,"ROLELENGTH") || strstr(cmd,"FAULTACTOR") || 
            strstr(cmd,"FAULTACTLEN") || strstr(cmd,"DETAIL") || strstr(cmd,"DETAILLENGTH")|| strstr(cmd,"FROMCCSID") ||
            strstr(cmd,"SERVICE") || strstr(cmd,"WEBSERVICE") || strstr(cmd,"OPERATION") || strstr(cmd,"URI") ||  
            strstr(cmd,"URIMAP") || strstr(cmd,"SCOPE") || strstr(cmd,"SCOPELEN") || strstr(cmd,"NODATA") ||
            strstr(cmd,"SECURITY") || strstr(cmd,"RESTYPE") || strstr(cmd,"RESCLASS") || strstr(cmd,"RESIDLENGTH") || 
            strstr(cmd,"RESID") || strstr(cmd,"LOGMESSAGE") || strstr(cmd,"READ") || strstr(cmd,"UPDATE") ||
            strstr(cmd,"UPDATE") || strstr(cmd,"ALTER") || strstr(cmd,"KEYLENGTH") || strstr(cmd,"RIDFLD") || 
            strstr(cmd,"DATASET") || strstr(cmd,"GTEQ") || strstr(cmd,"STARTBR")  || strstr(cmd,"ENDBR") || 
            strstr(cmd,"FILE") || strstr(cmd,"READNEXT") || strstr(cmd,"READPREV") || strstr(cmd,"CURSOR") || 
            strstr(cmd,"DEBKEY") || strstr(cmd,"DEBREC") || strstr(cmd,"FILE") || strstr(cmd,"EQUAL") || 
            strstr(cmd,"GENERIC") || strstr(cmd,"RBA") || strstr(cmd,"RRN") || strstr(cmd,"XRBA") || 
            strstr(cmd,"REQID") || strstr(cmd,"TOKEN") || strstr(cmd,"UNCOMMITTED") || strstr(cmd,"REPEATABLE") || 
            strstr(cmd,"CONSISTENT") || strstr(cmd,"MASSINSERT")|| strstr(cmd,"LABEL") || strstr(cmd,"RESET") || 
            strstr(cmd,"TEXT") || strstr(cmd,"APPLID")) {
            sprintf(end,"%s%s",cmd,"\n");

            if ((strcmp(cmd,"NOHANDLE") == 0) && ((*respFieldsState) == 0)) {
                (*respFieldsState) = 3;
            }
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
            if (((*cmdState) == -1) && ((*memParamsState) == 1)) {
                // SEND FROM LENGTH param value
                (*((int*)memParams[0])) = atoi(cmd);
                (*memParamsState) = 10;
            }
            if (((*cmdState) == -1) && ((*memParamsState) == 3)) {
                int l = strlen(cmd);
                int i = 0, j = 0;
                for (i = 0; i < l && j < 8; i++) {
                    if ((cmd[i] != '\'') && (cmd[i] != ' ')) {
                        currentNames->currentMap[j] = cmd[i];
                        j++;
                    }         
                }
                currentNames->currentMap[j] = 0x00;
                (*memParamsState) = 10;
            }
            if (((*cmdState) == -1) && ((*memParamsState) == 4)) {
                int l = strlen(cmd);
                int i = 0, j = 0;
                for (i = 0; i < l && j < 8; i++) {
                    if ((cmd[i] != '\'') && (cmd[i] != ' ')) {
                        currentNames->currentMapSet[j] = cmd[i];
                        j++;
                    }         
                }
                currentNames->currentMapSet[j] = 0x00;
                (*memParamsState) = 10;
            }
            if ((*cmdState) == -1) {
                (*memParamsState) = 10;

                if (strcmp(cmd,"LENGTH") == 0) {
                    (*memParamsState) = 1;
                }
                if (strcmp(cmd,"FROM") == 0) {
                    (*memParamsState) = 2;
                }
                if ((strcmp(cmd,"MAP") == 0) && 
                    (strcmp(cmd,"MAPSET") != 0)) {
                    (*memParamsState) = 3;
                }
                if (strcmp(cmd,"MAPSET") == 0) {
                    (*memParamsState) = 4;
                }
                if (strcmp(cmd,"TEXT") == 0) {
                    (*memParamsState) = 5;
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
                if (strcmp(cmd,"SET") == 0) {
                    (*memParamsState) = 3;
                }
                if (strcmp(cmd,"NODATA") == 0) {
                    memParams[4] = (void*)1;
                    (*memParamsState) = 10;
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
                if ((strcmp(cmd,"QUEUE") == 0) || (strcmp(cmd,"QNAME") == 0)) {
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
                if ((strcmp(cmd,"QUEUE") == 0) || (strcmp(cmd,"QNAME") == 0)) {
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
                if ((strcmp(cmd,"QUEUE") == 0) || (strcmp(cmd,"QNAME") == 0)) {
                    (*memParamsState) = 3;
                }
                if (strcmp(cmd,"TD") == 0) {
                    memParams[5] = (void*)((long)memParams[5] + 1);
                }
            }
            if (((*cmdState) == -17) && ((*memParamsState) == 1)) {
                int l = strlen(cmd);
                int i = 0, j = 0;
                for (i = 0; i < l && j < 4; i++) {
                    if ((cmd[i] != '\'') && (cmd[i] != ' ')) {
                        currentNames->abcode[j] = cmd[i];
                        j++;
                    }         
                }
                currentNames->abcode[j] = 0x00;
                (*memParamsState) = 10;
            }
            if ((*cmdState) == -17) {
                (*memParamsState) = 10;

                if (strcmp(cmd,"ABCODE") == 0) {
                    (*memParamsState) = 1;
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
                if (strcmp(cmd,"APPLID") == 0) {
                    (*memParamsState) = 1;
                }
                if (strcmp(cmd,"SYSID") == 0) {
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
            if ((*cmdState) == -21) {
                (*memParamsState) = 10;

                if (strcmp(cmd,"CREATE") == 0) {
                    (*memParamsState) = 1;
                }
                if (strcmp(cmd,"CLIENT") == 0) {
                    (*memParamsState) = 2;
                }
                if (strcmp(cmd,"SERVER") == 0) {
                    (*memParamsState) = 2;
                }
                if (strcmp(cmd,"SENDER") == 0) {
                    (*memParamsState) = 2;
                }
                if (strcmp(cmd,"RECEIVER") == 0) {
                    (*memParamsState) = 2;
                }
            }
            if ((*cmdState) == -23) {
                (*memParamsState) = 10;

                if (strcmp(cmd,"READ") == 0) {
                    (*memParamsState) = 1;
                }
                if (strcmp(cmd,"UPDATE") == 0) {
                    (*memParamsState) = 2;
                }
                if (strcmp(cmd,"CONTROL") == 0) {
                    (*memParamsState) = 3;
                }
                if (strcmp(cmd,"ALTER") == 0) {
                    (*memParamsState) = 4;
                }
            }
            if (((*cmdState) == -24) && ((*memParamsState) == 1)) {
                // KEYLENGTH param value
                (*((int*)memParams[0])) = atoi(cmd);
                (*memParamsState) = 10;
            }
            if (((*cmdState) == -24) && ((*memParamsState) == 2)) {
                int l = strlen(cmd);
                int i = 0, j = 0;
                for (i = 0; i < l && j < 45; i++) {
                    if ((cmd[i] != '\'') && (cmd[i] != ' ')) {
                        currentNames->fileName[j] = cmd[i];
                        j++;
                    }         
                }
                currentNames->fileName[j] = 0x00;
                (*memParamsState) = 10;
            }
            if (((*cmdState) == -24) && ((*memParamsState) == 4)) {
                // REQID param value
                (*((int*)memParams[2])) = atoi(cmd);
                (*memParamsState) = 10;
            }
            if ((*cmdState) == -24) {
                (*memParamsState) = 10;

                if (strcmp(cmd,"KEYLENGTH") == 0) {
                    (*memParamsState) = 1;
                }
                if ((strcmp(cmd,"FILE") == 0) || (strcmp(cmd,"DATASET") == 0)) {
                    (*memParamsState) = 2;
                }
                if (strcmp(cmd,"RIDFLD") == 0) {
                    (*memParamsState) = 3;
                }
                if (strcmp(cmd,"REQID") == 0) {
                    (*memParamsState) = 4;
                }
                if (strcmp(cmd,"GTEQ") == 0) {
                    *((int*)memParams[3]) = *((int*)memParams[3]) | 1;
                }
                if (strcmp(cmd,"EQUAL") == 0) {
                    *((int*)memParams[3]) = *((int*)memParams[3]) | 2;
                }
                if (strcmp(cmd,"GENERIC") == 0) {
                    *((int*)memParams[3]) = *((int*)memParams[3]) | 4;
                }
                if (strcmp(cmd,"RBA") == 0) {
                    *((int*)memParams[3]) = *((int*)memParams[3]) | 8;
                }
                if (strcmp(cmd,"RRN") == 0) {
                    *((int*)memParams[3]) = *((int*)memParams[3]) | 16;
                }
                if (strcmp(cmd,"XRBA") == 0) {
                    *((int*)memParams[3]) = *((int*)memParams[3]) | 32;
                }
            }
            if (((*cmdState) == -25) && ((*memParamsState) == 1)) {
                int l = strlen(cmd);
                int i = 0, j = 0;
                for (i = 0; i < l && j < 45; i++) {
                    if ((cmd[i] != '\'') && (cmd[i] != ' ')) {
                        currentNames->fileName[j] = cmd[i];
                        j++;
                    }         
                }
                currentNames->fileName[j] = 0x00;
                (*memParamsState) = 10;
            }
            if (((*cmdState) == -25) && ((*memParamsState) == 2)) {
                // REQID param value
                (*((int*)memParams[1])) = atoi(cmd);
                (*memParamsState) = 10;
            }
            if ((*cmdState) == -25) {
                (*memParamsState) = 10;

                if ((strcmp(cmd,"FILE") == 0) || (strcmp(cmd,"DATASET") == 0)) {
                    (*memParamsState) = 1;
                }
                if (strcmp(cmd,"REQID") == 0) {
                    (*memParamsState) = 2;
                }
            }
            if (((*cmdState) == -26) && ((*memParamsState) == 1)) {
                // KEYLENGTH param value
                (*((int*)memParams[0])) = atoi(cmd);
                (*memParamsState) = 10;
            }
            if (((*cmdState) == -26) && ((*memParamsState) == 2)) {
                int l = strlen(cmd);
                int i = 0, j = 0;
                for (i = 0; i < l && j < 45; i++) {
                    if ((cmd[i] != '\'') && (cmd[i] != ' ')) {
                        currentNames->fileName[j] = cmd[i];
                        j++;
                    }         
                }
                currentNames->fileName[j] = 0x00;
                (*memParamsState) = 10;
            }
            if (((*cmdState) == -26) && ((*memParamsState) == 4)) {
                // REQID param value
                (*((int*)memParams[2])) = atoi(cmd);
                (*memParamsState) = 10;
            }
            if (((*cmdState) == -26) && ((*memParamsState) == 7)) {
                // LENGTH param value
                (*((int*)memParams[7])) = atoi(cmd);
                (*memParamsState) = 10;
            }
            if ((*cmdState) == -26) {
                (*memParamsState) = 10;

                if (strcmp(cmd,"KEYLENGTH") == 0) {
                    (*memParamsState) = 1;
                }
                if ((strcmp(cmd,"FILE") == 0) || (strcmp(cmd,"DATASET") == 0)) {
                    (*memParamsState) = 2;
                }
                if (strcmp(cmd,"RIDFLD") == 0) {
                    (*memParamsState) = 3;
                }
                if (strcmp(cmd,"REQID") == 0) {
                    (*memParamsState) = 4;
                }
                if (strcmp(cmd,"INTO") == 0 || 
                    strcmp(cmd,"FROM") == 0) {
                    (*memParamsState) = 5;
                }
                if (strcmp(cmd,"SET") == 0) {
                    (*memParamsState) = 6;
                }
                if (strcmp(cmd,"LENGTH") == 0) {
                    (*memParamsState) = 7;
                }
                if (strcmp(cmd,"TOKEN") == 0) {
                    (*memParamsState) = 8;
                }
                if (strcmp(cmd,"GTEQ") == 0) {
                    *((int*)memParams[3]) = *((int*)memParams[3]) | 1;
                }
                if (strcmp(cmd,"EQUAL") == 0) {
                    *((int*)memParams[3]) = *((int*)memParams[3]) | 2;
                }
                if (strcmp(cmd,"GENERIC") == 0) {
                    *((int*)memParams[3]) = *((int*)memParams[3]) | 4;
                }
                if (strcmp(cmd,"RBA") == 0) {
                    *((int*)memParams[3]) = *((int*)memParams[3]) | 8;
                }
                if (strcmp(cmd,"RRN") == 0) {
                    *((int*)memParams[3]) = *((int*)memParams[3]) | 16;
                }
                if (strcmp(cmd,"XRBA") == 0) {
                    *((int*)memParams[3]) = *((int*)memParams[3]) | 32;
                }
                if (strcmp(cmd,"UNCOMMITTED") == 0) {
                    *((int*)memParams[3]) = *((int*)memParams[3]) | 64;
                }
                if (strcmp(cmd,"CONSISTENT") == 0) {
                    *((int*)memParams[3]) = *((int*)memParams[3]) | 128;
                }
                if (strcmp(cmd,"REPEATABLE") == 0) {
                    *((int*)memParams[3]) = *((int*)memParams[3]) | 256;
                }
                if (strcmp(cmd,"NOSUSPEND") == 0) {
                    *((int*)memParams[3]) = *((int*)memParams[3]) | 512;
                }
                if (strcmp(cmd,"MASSINSERT") == 0) {
                    *((int*)memParams[3]) = *((int*)memParams[3]) | 1024;
                }
                if (strcmp(cmd,"UPDATE") == 0) {
                    *((int*)memParams[3]) = *((int*)memParams[3]) | 2048;
                }
            }
            if (((*cmdState) == -27) && ((*memParamsState) == 2)) {
                if (currentNames->abendHandlerCnt > 0) {
                    int l = strlen(cmd);
                    int i = 0, j = 0;
                    for (i = 0; i < l && j < 8; i++) {
                        if ((cmd[i] != '\'') && (cmd[i] != ' ')) {
                            currentNames->abendHandlers[currentNames->abendHandlerCnt-1].abendProgname[j] = cmd[i];
                            j++;
                        }         
                    }
                    currentNames->abendHandlers[currentNames->abendHandlerCnt-1].abendProgname[j] = 0x00;
                }
                (*memParamsState) = 10;
            }
            if ((*cmdState) == -27) {
                (*memParamsState) = 10;

                if (strcmp(cmd,"ABEND") == 0) {
                    *((int*)memParams[0]) = 1;
                    (*memParamsState) = 1;
                }
                if (strcmp(cmd,"CANCEL") == 0) {
                    *((int*)memParams[0]) = 1;
                    (*memParamsState) = 1;
                }
                if (strcmp(cmd,"PROGRAM") == 0) {
                    if (*((int*)memParams[0]) > 0) {
                        *((int*)memParams[0]) = 2;
                    }
                    (*memParamsState) = 2;
                }
                if (strcmp(cmd,"LABEL") == 0) {
                    if (*((int*)memParams[0]) > 0) {
                        *((int*)memParams[0]) = 3;
                    }
                    (*memParamsState) = 3;
                }
                if (strcmp(cmd,"RESET") == 0) {
                    if (*((int*)memParams[0]) > 0) {
                        *((int*)memParams[0]) = 4;
                    }
                    (*memParamsState) = 4;
                }
            }

            if (cmdbuf[0] == '\'') {
              // String constant
              write(childfd,"=",1);
            }
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
        } else {
            if (var != NULL) {
                cob_field *cobvar = (cob_field*)var;
                if (((*cmdState) == -1) && ((*memParamsState) == 4)) {
                    sprintf(end,"%s%s",cmd,"=");
                    end = &cmdbuf[strlen(cmdbuf)];
                    FILE *f = fmemopen(end, CMDBUF_SIZE-strlen(cmdbuf), "w");
                    if (COB_FIELD_TYPE(cobvar) == COB_TYPE_ALPHANUMERIC) putc('\'',f);
                    if ((cobvar->data[0] != 0) || (getCobType(cobvar) == COB_TYPE_NUMERIC_BINARY) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_COMP5) ||
                        (getCobType(cobvar) == COB_TYPE_NUMERIC) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_PACKED)) {
                        display_cobfield(cobvar,f);
                    }
                    if (COB_FIELD_TYPE(cobvar) == COB_TYPE_ALPHANUMERIC) putc('\'',f);
                    putc(0x00,f);
                    fclose(f);
                    write(childfd,cmdbuf,strlen(cmdbuf));
                    write(childfd,"\n",1);

                    f = fmemopen(&currentNames->currentMapSet, 9, "w");
                    if (cobvar->data[0] != 0) {
                        display_cobfield(cobvar,f);
                    }
                    putc(0x00,f);
                    fclose(f);
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -1) && ((*memParamsState) == 3)) {
                    sprintf(end,"%s%s",cmd,"=");
                    end = &cmdbuf[strlen(cmdbuf)];
                    FILE *f = fmemopen(end, CMDBUF_SIZE-strlen(cmdbuf), "w");
                    if (COB_FIELD_TYPE(cobvar) == COB_TYPE_ALPHANUMERIC) putc('\'',f);
                    if ((cobvar->data[0] != 0) || (getCobType(cobvar) == COB_TYPE_NUMERIC_BINARY) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_COMP5) ||
                        (getCobType(cobvar) == COB_TYPE_NUMERIC) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_PACKED)) {
                        display_cobfield(cobvar,f);
                    }
                    if (COB_FIELD_TYPE(cobvar) == COB_TYPE_ALPHANUMERIC) putc('\'',f);
                    putc(0x00,f);
                    fclose(f);
                    write(childfd,cmdbuf,strlen(cmdbuf));
                    write(childfd,"\n",1);

                    f = fmemopen(&currentNames->currentMap, 9, "w");
                    if (cobvar->data[0] != 0) {
                        display_cobfield(cobvar,f);
                    }
                    putc(0x00,f);
                    fclose(f);
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -1) && ((*memParamsState) == 2)) {
                    memParams[1] = (void*)cobvar;
                    (*memParamsState) = 10;
                    char str[20];
                    sprintf((char*)&str,"%s\n","SIZE");
                    write(childfd,str,strlen(str));
                    sprintf((char*)&str,"%s%d\n","=",(int)cobvar->size);
                    write(childfd,str,strlen(str));
                }
                if (((*cmdState) == -1) && ((*memParamsState) == 1)) {
                    //SEND LENGTH
                    sprintf(end,"%s%s",cmd,"=");
                    end = &cmdbuf[strlen(cmdbuf)];
                    FILE *f = fmemopen(end, CMDBUF_SIZE-strlen(cmdbuf), "w");
                    if ((cobvar->data[0] != 0) || (getCobType(cobvar) == COB_TYPE_NUMERIC_BINARY) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_COMP5) ||
                        (getCobType(cobvar) == COB_TYPE_NUMERIC) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_PACKED)) {
                        display_cobfield(cobvar,f);
                    }
                    putc(0x00,f);
                    fclose(f);
                    write(childfd,cmdbuf,strlen(cmdbuf));
                    write(childfd,"\n",1);
                    (*((int*)memParams[0])) = atoi(end);
                    (*memParamsState) = 10;
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
                     //RECEIVE LENGTH
                    sprintf(end,"%s%s",cmd,"=");
                    end = &cmdbuf[strlen(cmdbuf)];
                    FILE *f = fmemopen(end, CMDBUF_SIZE-strlen(cmdbuf), "w");
                    if ((cobvar->data[0] != 0) || (getCobType(cobvar) == COB_TYPE_NUMERIC_BINARY) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_COMP5) ||
                        (getCobType(cobvar) == COB_TYPE_NUMERIC) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_PACKED)) {
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
                    FILE *f = fmemopen(end, CMDBUF_SIZE-strlen(cmdbuf), "w");
                    if (COB_FIELD_TYPE(cobvar) == COB_TYPE_ALPHANUMERIC) putc('\'',f);
                    if ((cobvar->data[0] != 0) || (getCobType(cobvar) == COB_TYPE_NUMERIC_BINARY) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_COMP5) ||
                        (getCobType(cobvar) == COB_TYPE_NUMERIC) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_PACKED)) {
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
                    FILE *f = fmemopen(end, CMDBUF_SIZE-strlen(cmdbuf), "w");
                    if (COB_FIELD_TYPE(cobvar) == COB_TYPE_ALPHANUMERIC) putc('\'',f);
                    if ((cobvar->data[0] != 0) || (getCobType(cobvar) == COB_TYPE_NUMERIC_BINARY) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_COMP5) ||
                        (getCobType(cobvar) == COB_TYPE_NUMERIC) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_PACKED)) {
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
                    xctlParams[1] = (char*)cobvar;
                    if (((int)cobvar->size >= 0) && (cobvar->size < 32768)) {
                        for (int i = 0; i < cobvar->size; i++) {
                          commArea[i] = cobvar->data[i];
                        }
                    }
                    (*xctlState) = 10;
                }
                if (((*cmdState) < -5) &&
                    !(((*cmdState) == -9) && ((*memParamsState) == 1)) &&
                    !(((*cmdState) == -9) && ((*memParamsState) == 2)) &&
                    !(((*cmdState) == -10) && ((*memParamsState) == 1)) &&
                    !(((*cmdState) == -10) && ((*memParamsState) == 2)) &&
                    !(((*cmdState) == -10) && ((*memParamsState) == 3)) &&
                    !(((*cmdState) == -6) && ((*memParamsState) == 2))  &&
                    !(((*cmdState) == -11) && ((*memParamsState) == 2)) &&
                    !(((*cmdState) == -12) && ((*memParamsState) == 2)) &&
                    !(((*cmdState) == -14) && ((*memParamsState) == 0)) &&
                    !(((*cmdState) == -14) && ((*memParamsState) == 1)) &&
                    !(((*cmdState) == -15) && ((*memParamsState) == 0)) &&
                    !(((*cmdState) == -15) && ((*memParamsState) == 1)) &&
                    !(((*cmdState) == -15) && ((*memParamsState) == 2)) &&
                    !(((*cmdState) == -17) && ((*memParamsState) == 1)) &&
                    !(((*cmdState) == -18) && ((*memParamsState) == 0)) &&
                    !(((*cmdState) == -18) && ((*memParamsState) == 1)) &&
                    !(((*cmdState) == -19) && ((*memParamsState) == 1)) &&
                    !(((*cmdState) == -19) && ((*memParamsState) == 2)) &&
                    !(((*cmdState) == -19) && ((*memParamsState) == 3)) &&
                    !(((*cmdState) == -21) && ((*memParamsState) == 1)) &&
                    !(((*cmdState) == -21) && ((*memParamsState) == 2)) &&
                    !(((*cmdState) == -23) && ((*memParamsState) == 1)) &&
                    !(((*cmdState) == -23) && ((*memParamsState) == 2)) &&
                    !(((*cmdState) == -23) && ((*memParamsState) == 3)) &&
                    !(((*cmdState) == -23) && ((*memParamsState) == 4)) &&
                    !(((*cmdState) == -24) && ((*memParamsState) == 1)) &&
                    !(((*cmdState) == -24) && ((*memParamsState) == 2)) &&
                    !(((*cmdState) == -24) && ((*memParamsState) == 3)) &&
                    !(((*cmdState) == -24) && ((*memParamsState) == 4)) &&
                    !(((*cmdState) == -25) && ((*memParamsState) == 1)) &&
                    !(((*cmdState) == -25) && ((*memParamsState) == 2)) &&
                    !(((*cmdState) == -26) && ((*memParamsState) == 1)) &&
                    !(((*cmdState) == -26) && ((*memParamsState) == 2)) &&
                    !(((*cmdState) == -26) && ((*memParamsState) == 3)) &&
                    !(((*cmdState) == -26) && ((*memParamsState) == 4)) &&
                    !(((*cmdState) == -26) && ((*memParamsState) == 5)) &&
                    !(((*cmdState) == -26) && ((*memParamsState) == 6)) &&
                    !(((*cmdState) == -26) && ((*memParamsState) == 7)) &&
                    !(((*cmdState) == -26) && ((*memParamsState) == 8)) &&
                    !(((*cmdState) == -27) && ((*memParamsState) == 2))) {
                    sprintf(end,"%s%s",cmd,"=");
                    end = &cmdbuf[strlen(cmdbuf)];
                    FILE *f = fmemopen(end, CMDBUF_SIZE-strlen(cmdbuf), "w");
                    if (COB_FIELD_TYPE(cobvar) == COB_TYPE_ALPHANUMERIC) putc('\'',f);
                    if ((cobvar->data[0] != 0) || (getCobType(cobvar) == COB_TYPE_NUMERIC_BINARY) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_COMP5) ||
                        (getCobType(cobvar) == COB_TYPE_NUMERIC) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_PACKED)) {
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
                    FILE *f = fmemopen(end, CMDBUF_SIZE-strlen(cmdbuf), "w");
                    if ((cobvar->data[0] != 0) || (getCobType(cobvar) == COB_TYPE_NUMERIC_BINARY) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_COMP5) ||
                        (getCobType(cobvar) == COB_TYPE_NUMERIC) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_PACKED)) {
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
                    FILE *f = fmemopen(end, CMDBUF_SIZE-strlen(cmdbuf), "w");
                    if ((cobvar->data[0] != 0) || (getCobType(cobvar) == COB_TYPE_NUMERIC_BINARY) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_COMP5) ||
                        (getCobType(cobvar) == COB_TYPE_NUMERIC) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_PACKED)) {
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
                    FILE *f = fmemopen(end, CMDBUF_SIZE-strlen(cmdbuf), "w");
                    if ((cobvar->data[0] != 0) || (getCobType(cobvar) == COB_TYPE_NUMERIC_BINARY) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_COMP5) ||
                        (getCobType(cobvar) == COB_TYPE_NUMERIC) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_PACKED)) {
                        display_cobfield(cobvar,f);
                    }
                    putc(0x00,f);
                    fclose(f);
                    write(childfd,cmdbuf,strlen(cmdbuf));
                    write(childfd,"\n",1);
                    (*((int*)memParams[0])) = atoi(end);
                    memParams[3] = (void*)cobvar;
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
                if (((*cmdState) == -10) && ((*memParamsState) == 3)) {
                    memParams[2] = (void*)cobvar;
                    (*memParamsState) = 10;
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
                    FILE *f = fmemopen(end, CMDBUF_SIZE-strlen(cmdbuf), "w");
                    if ((cobvar->data[0] != 0) || (getCobType(cobvar) == COB_TYPE_NUMERIC_BINARY) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_COMP5) ||
                        (getCobType(cobvar) == COB_TYPE_NUMERIC) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_PACKED)) {
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
                    FILE *f = fmemopen(end, CMDBUF_SIZE-strlen(cmdbuf), "w");
                    if ((cobvar->data[0] != 0) || (getCobType(cobvar) == COB_TYPE_NUMERIC_BINARY) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_COMP5) ||
                        (getCobType(cobvar) == COB_TYPE_NUMERIC) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_PACKED)) {
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
                    FILE *f = fmemopen(end, CMDBUF_SIZE-strlen(cmdbuf), "w");
                    if ((cobvar->data[0] != 0) || (getCobType(cobvar) == COB_TYPE_NUMERIC_BINARY) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_COMP5) ||
                        (getCobType(cobvar) == COB_TYPE_NUMERIC) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_PACKED)) {
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
                    // READQ LENGTH
                    sprintf(end,"%s%s",cmd,"=");
                    end = &cmdbuf[strlen(cmdbuf)];
                    FILE *f = fmemopen(end, CMDBUF_SIZE-strlen(cmdbuf), "w");
                    if ((cobvar->data[0] != 0) || (getCobType(cobvar) == COB_TYPE_NUMERIC_BINARY) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_COMP5) ||
                        (getCobType(cobvar) == COB_TYPE_NUMERIC) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_PACKED)) {
                        display_cobfield(cobvar,f);
                    }
                    putc(0x00,f);
                    fclose(f);
                    write(childfd,cmdbuf,strlen(cmdbuf));
                    write(childfd,"\n",1);
                    (*((int*)memParams[0])) = atoi(end);
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -27) && ((*memParamsState) == 1)) {
                    FILE *f = fmemopen(&currentNames->abcode, 5, "w");
                    if (cobvar->data[0] != 0) {
                        display_cobfield(cobvar,f);
                    }
                    putc(0x00,f);
                    fclose(f);
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
                    if (COB_FIELD_TYPE(cobvar) == COB_TYPE_NUMERIC_PACKED) {
                      long v = atol(buf);
                      cob_put_s64_comp3(v,cobvar->data,cobvar->size);
                    }
                    if (getCobType(cobvar) == COB_TYPE_NUMERIC_BINARY) {
                      long v = atol(buf);
                      cob_put_u64_compx(v,cobvar->data,cobvar->size);
                    }
                    if (getCobType(cobvar) == COB_TYPE_NUMERIC_COMP5) {
                      long v = atol(buf);
                      cob_put_s64_comp5(v,cobvar->data,cobvar->size);
                    }
                }
                if (((*cmdState) == -18) && ((*memParamsState) == 1)) {
                    (*memParamsState) = 0;
                }
                if (((*cmdState) == -19) && ((*memParamsState) == 3)) {
                    // START TRANSID REQID
                    write(childfd,"=",1);
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
                    FILE *f = fmemopen(end, CMDBUF_SIZE-strlen(cmdbuf), "w");
                    if ((cobvar->data[0] != 0) || (getCobType(cobvar) == COB_TYPE_NUMERIC_BINARY) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_COMP5) ||
                        (getCobType(cobvar) == COB_TYPE_NUMERIC) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_PACKED)) {
                        display_cobfield(cobvar,f);
                    }
                    putc(0x00,f);
                    fclose(f);
                    write(childfd,cmdbuf,strlen(cmdbuf));
                    write(childfd,"\n",1);
                    (*((int*)memParams[0])) = atoi(end);
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -21) && ((*memParamsState) == 1)) {
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -21) && ((*memParamsState) == 2)) {
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -23) && ((*memParamsState) == 1)) {
                    memParams[1] = (void*)cobvar;
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -23) && ((*memParamsState) == 2)) {
                    memParams[2] = (void*)cobvar;
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -23) && ((*memParamsState) == 3)) {
                    memParams[3] = (void*)cobvar;
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -23) && ((*memParamsState) == 4)) {
                    memParams[4] = (void*)cobvar;
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -24) && ((*memParamsState) == 1)) {
                    sprintf(end,"%s%s",cmd,"=");
                    end = &cmdbuf[strlen(cmdbuf)];
                    FILE *f = fmemopen(end, CMDBUF_SIZE-strlen(cmdbuf), "w");
                    if ((cobvar->data[0] != 0) || (getCobType(cobvar) == COB_TYPE_NUMERIC_BINARY) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_COMP5) ||
                        (getCobType(cobvar) == COB_TYPE_NUMERIC) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_PACKED)) {
                        display_cobfield(cobvar,f);
                    }
                    putc(0x00,f);
                    fclose(f);
                    write(childfd,cmdbuf,strlen(cmdbuf));
                    write(childfd,"\n",1);
                    (*((int*)memParams[0])) = atoi(end);
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -24) && ((*memParamsState) == 2)) {
                    FILE *f = fmemopen(&currentNames->fileName, 46, "w");
                    if (cobvar->data[0] != 0) {
                        display_cobfield(cobvar,f);
                    }
                    putc(0x00,f);
                    fclose(f);
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -24) && ((*memParamsState) == 3)) {
                    memParams[1] = (void*)cobvar;
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -24) && ((*memParamsState) == 4)) {
                    sprintf(end,"%s%s",cmd,"=");
                    end = &cmdbuf[strlen(cmdbuf)];
                    FILE *f = fmemopen(end, CMDBUF_SIZE-strlen(cmdbuf), "w");
                    if ((cobvar->data[0] != 0) || (getCobType(cobvar) == COB_TYPE_NUMERIC_BINARY) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_COMP5) ||
                        (getCobType(cobvar) == COB_TYPE_NUMERIC) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_PACKED)) {
                        display_cobfield(cobvar,f);
                    }
                    putc(0x00,f);
                    fclose(f);
                    write(childfd,cmdbuf,strlen(cmdbuf));
                    write(childfd,"\n",1);
                    (*((int*)memParams[2])) = atoi(end);
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -25) && ((*memParamsState) == 1)) {
                    FILE *f = fmemopen(&currentNames->fileName, 46, "w");
                    if (cobvar->data[0] != 0) {
                        display_cobfield(cobvar,f);
                    }
                    putc(0x00,f);
                    fclose(f);
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -25) && ((*memParamsState) == 2)) {
                    sprintf(end,"%s%s",cmd,"=");
                    end = &cmdbuf[strlen(cmdbuf)];
                    FILE *f = fmemopen(end, CMDBUF_SIZE-strlen(cmdbuf), "w");
                    if ((cobvar->data[0] != 0) || (getCobType(cobvar) == COB_TYPE_NUMERIC_BINARY) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_COMP5) ||
                        (getCobType(cobvar) == COB_TYPE_NUMERIC) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_PACKED)) {
                        display_cobfield(cobvar,f);
                    }
                    putc(0x00,f);
                    fclose(f);
                    write(childfd,cmdbuf,strlen(cmdbuf));
                    write(childfd,"\n",1);
                    (*((int*)memParams[1])) = atoi(end);
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -26) && ((*memParamsState) == 1)) {
                    sprintf(end,"%s%s",cmd,"=");
                    end = &cmdbuf[strlen(cmdbuf)];
                    FILE *f = fmemopen(end, CMDBUF_SIZE-strlen(cmdbuf), "w");
                    if ((cobvar->data[0] != 0) || (getCobType(cobvar) == COB_TYPE_NUMERIC_BINARY) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_COMP5) ||
                        (getCobType(cobvar) == COB_TYPE_NUMERIC) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_PACKED)) {
                        display_cobfield(cobvar,f);
                    }
                    putc(0x00,f);
                    fclose(f);
                    write(childfd,cmdbuf,strlen(cmdbuf));
                    write(childfd,"\n",1);
                    (*((int*)memParams[0])) = atoi(end);
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -26) && ((*memParamsState) == 2)) {
                    FILE *f = fmemopen(&currentNames->fileName, 46, "w");
                    if (cobvar->data[0] != 0) {
                        display_cobfield(cobvar,f);
                    }
                    putc(0x00,f);
                    fclose(f);
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -26) && ((*memParamsState) == 3)) {
                    memParams[1] = (void*)cobvar;
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -26) && ((*memParamsState) == 4)) {
                    sprintf(end,"%s%s",cmd,"=");
                    end = &cmdbuf[strlen(cmdbuf)];
                    FILE *f = fmemopen(end, CMDBUF_SIZE-strlen(cmdbuf), "w");
                    if ((cobvar->data[0] != 0) || (getCobType(cobvar) == COB_TYPE_NUMERIC_BINARY) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_COMP5) ||
                        (getCobType(cobvar) == COB_TYPE_NUMERIC) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_PACKED)) {
                        display_cobfield(cobvar,f);
                    }
                    putc(0x00,f);
                    fclose(f);
                    write(childfd,cmdbuf,strlen(cmdbuf));
                    write(childfd,"\n",1);
                    (*((int*)memParams[2])) = atoi(end);
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -26) && ((*memParamsState) == 5)) {
                    memParams[5] = (void*)cobvar;
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -26) && ((*memParamsState) == 6)) {
                    memParams[6] = (void*)cobvar;
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -26) && ((*memParamsState) == 7)) {
                    // LENGTH param
                    sprintf(end,"%s%s",cmd,"=");
                    end = &cmdbuf[strlen(cmdbuf)];
                    FILE *f = fmemopen(end, CMDBUF_SIZE-strlen(cmdbuf), "w");
                    if ((cobvar->data[0] != 0) || (getCobType(cobvar) == COB_TYPE_NUMERIC_BINARY) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_COMP5) ||
                        (getCobType(cobvar) == COB_TYPE_NUMERIC) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_PACKED)) {
                        display_cobfield(cobvar,f);
                    }
                    putc(0x00,f);
                    fclose(f);
                    write(childfd,cmdbuf,strlen(cmdbuf));
                    write(childfd,"\n",1);
                    (*((int*)memParams[7])) = atoi(end);
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -26) && ((*memParamsState) == 8)) {
                    memParams[8] = (void*)cobvar;
                    (*memParamsState) = 10;
                }
                if (((*cmdState) == -27) && ((*memParamsState) == 2)) {
                    memParams[1] = (void*)cobvar;
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
                if (COB_FIELD_TYPE(cobvar) == COB_TYPE_GROUP) {
		            // Treat as VARCHAR field
		            unsigned int l = (unsigned int)cobvar->data[0];	
                    l = (l << 8) | (unsigned int)cobvar->data[1];
		            if (l > (cobvar->size-2)) {
                       l = cobvar->size-2;
                    }
                    end[0] = '\'';
                    int i = 0, j = 1;
                    for (i = 0; i < l; i++, j++) {
                        unsigned char c = cobvar->data[i+2];
                        if (c == 0x00) {
                           end[j] = '\\';
                           j++;
                           end[j] = '0';  
                           continue; 
                        }   
                        if ((c & 0x80) == 0) {
                           // Plain ASCII
                           end[j] = c; 
                        } else {
                           // Convert ext. ASCII to UTF-8
                           unsigned char c1 = 0xC0;
                           c1 = c1 | ((c & 0xC0) >> 6);
                           end[j] = c1; 
                           j++;
                           c1 = 0x80;
                           c1 = c1 | (c & 0x3F);
                           end[j] = c1;
                        }
                    }
                    end[j] = '\'';
                    end[j+1] = ' ';
                    end[j+2] = 0x00;
                } else
                if (COB_FIELD_TYPE(cobvar) == COB_TYPE_ALPHANUMERIC) {
                    char *str = adjustDateFormatToDb((char*)cobvar->data,cobvar->size);
                    if ((cobvar->size == 5) || (cobvar->size == 10) || 
                        ((cobvar->size == 26) && (str == (char*)cobvar->data))) {
                        str = adjustTimeFormatToDb((char*)cobvar->data,cobvar->size);
                    }
                    end[0] = '\'';
                    int i = 0, j = 1;
                    for (i = 0; i < cobvar->size; i++, j++) {
                        unsigned char c = str[i];
                        if (c == 0x00) {
                           end[j] = '\\';
                           j++;
                           end[j] = '0';  
                           continue; 
                        }
                        if ((c & 0x80) == 0) {
                           // Plain ASCII
                           end[j] = c; 
                        } else {
                           // Convert ext. ASCII to UTF-8
                           unsigned char c1 = 0xC0;
                           c1 = c1 | ((c & 0xC0) >> 6);
                           end[j] = c1; 
                           j++;
                           c1 = 0x80;
                           c1 = c1 | (c & 0x3F);
                           end[j] = c1;
                        }
                    }
                    end[j] = '\'';
                    end[j+1] = ' ';
                    end[j+2] = 0x00;
                } else {
                    FILE *f = fmemopen(end, CMDBUF_SIZE-strlen(cmdbuf), "w");
                    if ((getCobType(cobvar) == COB_TYPE_NUMERIC_BINARY) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_COMP5) ||
                        (getCobType(cobvar) == COB_TYPE_NUMERIC) || 
                        (getCobType(cobvar) == COB_TYPE_NUMERIC_PACKED)) {
                       displayNumeric(cobvar,f);
                    }
                    putc(' ',f);
                    putc(0x00,f);
                    fclose(f);
                }
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
                if (strcmp("IFNULL",cmd) == 0) {
                    sprintf(end,"%s%s","COALESCE"," ");
                } else {
                    sprintf(end,"%s%s",cmd," ");
                }
            }
        }
    }
    return 1;
}


static void segv_handler(int signo) {
    if (signo == SIGSEGV) {
        printf("Segmentation fault in QWICS tpmserver, abending task\n");
        int *runState = (int*)pthread_getspecific(runStateKey);
        (*runState) = 3;
        int *respFieldsState = (int*)pthread_getspecific(respFieldsStateKey);
        (*respFieldsState) = 0;
        abend(16,1);
    }
    exit(0);
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
    pthread_key_create(&runStateKey, NULL);
    pthread_key_create(&cobFieldKey, NULL);
    pthread_key_create(&xctlStateKey, NULL);
    pthread_key_create(&xctlParamsKey, NULL);
    pthread_key_create(&eibbufKey, NULL);
    pthread_key_create(&eibaidKey, NULL);
    pthread_key_create(&linkAreaKey, NULL);
    pthread_key_create(&linkAreaPtrKey, NULL);
    pthread_key_create(&linkAreaAdrKey, NULL);
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
    pthread_key_create(&callStackKey, NULL);
    pthread_key_create(&callStackPtrKey, NULL);
    pthread_key_create(&chnBufListKey, NULL);
    pthread_key_create(&chnBufListPtrKey, NULL);
    pthread_key_create(&isamTxKey, NULL);
    pthread_key_create(&currentNamesKey, NULL);
    pthread_key_create(&callbackFuncKey, NULL);

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
    pthread_setspecific(connKey, NULL);

    GETENV_STRING(cobDateFormat,"QWICS_COBDATEFORMAT","YYYY-MM-dd.hh:mm:ss.uuuu");
    startIsamDB(GETENV_STRING(datasetDir,"QWICS_DATASET_DIR","../dataset"));
}


void clearExec(int initCons) {
#ifndef _USE_ONLY_PROCESSES_
    pthread_cond_destroy(&waitForModuleChange);
#endif
    stopIsamDB();
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


void execTransaction(char *name, void *fd, int setCommArea, int parCount) {
    char cmdbuf[CMDBUF_SIZE];
    int cmdState = 0;
    int runState = 0;
    int xctlState = 0;
    char progname[9];
    char *xctlParams[10];
    char eibbuf[150];
    char eibaid[2];
    char *linkArea = malloc(16000000);
    char commArea[32768];
    int linkAreaPtr = 0;
    char *linkAreaAdr = linkArea;
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
    struct callLoadlib callStack[1024];
    int callStackPtr = 0;
    int chnBufListPtr = 0;
    struct chnBuf chnBufList[256];
    void *isamTx = NULL;
    struct currentNamesType currentNames;
    int i = 0;
    for (i= 0; i < 150; i++) eibbuf[i] = 0;
    xctlParams[0] = progname;
    memParams[0] = &memParam;
    cob_field* outputVars[100];
    outputVars[0] = NULL; // NULL terminated list
    cmdbuf[0] = 0x00;
    currentNames.currentMap[0] = 0x00;
    currentNames.currentMapSet[0] = 0x00;
    currentNames.fileName[0] = 0x00;
    currentNames.abendHandlerCnt = 0;
    currentNames.abcode[0] = 0x00;
    pthread_setspecific(childfdKey, fd);
    pthread_setspecific(cmdbufKey, &cmdbuf);
    pthread_setspecific(cmdStateKey, &cmdState);
    pthread_setspecific(runStateKey, &runState);
    pthread_setspecific(cobFieldKey, &outputVars);
    pthread_setspecific(xctlStateKey, &xctlState);
    pthread_setspecific(xctlParamsKey, &xctlParams);
    pthread_setspecific(eibbufKey, &eibbuf);
    pthread_setspecific(eibaidKey, &eibaid);
    pthread_setspecific(linkAreaKey, linkArea);
    pthread_setspecific(linkAreaPtrKey, &linkAreaPtr);
    pthread_setspecific(linkAreaAdrKey, &linkAreaAdr);
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
    pthread_setspecific(callStackKey, &callStack);
    pthread_setspecific(callStackPtrKey, &callStackPtr);
    pthread_setspecific(chnBufListKey, &chnBufList);
    pthread_setspecific(chnBufListPtrKey, &chnBufListPtr);
    pthread_setspecific(currentNamesKey, &currentNames);

    // Optionally read in content of commarea
    if (setCommArea == 1) {
      write(*(int*)fd,"COMMAREA\n",9);
      char c = 0x00;
      for (i = 0; i < 32768; ) {
        int n = read(*(int*)fd,&c,1);
        if (n == 1) {
          commArea[i] = c;
          i++;
        }
      }
    }

    cob_get_global_ptr()->cob_current_module = &thisModule;
    cob_get_global_ptr()->cob_call_params = 1;

    // Optionally initialize call params
    if ((parCount > 0) && (parCount <= 10)) {
        cob_get_global_ptr ()->cob_call_params = cob_get_global_ptr ()->cob_call_params + parCount;
        for (i = 0; i < parCount; i++) {
            char len[10];
            char c = 0x00;
            int pos = 0;
            while (c != '\n') {
                int n = read(*(int*)fd,&c,1);
                if ((n == 1) && (c != '\n') && (c != '\r') && (c != '\'') && (pos < 10)) {
                    len[pos] = c;
                    pos++;
                }
            }
            len[pos] = 0x00; 
            paramList[i] = (void*)&linkArea[linkAreaPtr];
            linkAreaAdr = &linkArea[linkAreaPtr];
            linkAreaPtr += atoi(len);
        }
    }

    // Set signal handler for SIGSEGV (in case of mem leak in load module)
    struct sigaction a;
    a.sa_handler = segv_handler;
    a.sa_flags = 0;
    sigemptyset( &a.sa_mask );
    sigaction( SIGSEGV, &a, NULL );

    isamTx = pthread_getspecific(isamTxKey);
    if (isamTx != NULL) {
        endTransaction(isamTx,1);
    }
    isamTx = beginTransaction();
    pthread_setspecific(isamTxKey, isamTx);
    initOpenDatasets(currentNames.openDatasets);
    PGconn *conn = getDBConnection();
    pthread_setspecific(connKey, (void*)conn);
    initMain();
    execLoadModule(name,0,parCount);
    releaseLocks(TASK,taskLocks);
    globalCallCleanup();
    clearMain();
    free(allocMem);
    free(linkArea);
    clearChnBufList();
    returnDBConnection(conn,1);
    pthread_setspecific(connKey, NULL);
    closeOpenDatasets(currentNames.openDatasets);
    endTransaction(isamTx,1);
    pthread_setspecific(isamTxKey, NULL);
    // Flush output buffers
    fflush(stdout);
    fflush(stderr);
}


// Exec COBOL module within an existing DB transaction
void execInTransaction(char *name, void *fd, int setCommArea, int parCount) {
    char *cmdbuf = (char*)malloc(CMDBUF_SIZE);
    int cmdState = 0;
    int runState = 0;
    int xctlState = 0;
    char *xctlParams[10];
    char eibbuf[150];
    char eibaid[2];
    char *linkArea = (char*)malloc(16000000);
    char *commArea = (char*)malloc(32768);
    int linkAreaPtr = 0;
    char *linkAreaAdr = linkArea;
    int commAreaPtr = 0;
    int areaMode = 0;
    char progname[9];
    char linkStack[900];
    int linkStackPtr = 0;
    int memParamsState = 0;
    void *memParams[10];
    int memParam = 0;
    char *twa = malloc(32768);
    char tua[256];
    void** allocMem = (void**)malloc(MEM_POOL_SIZE*sizeof(void*));
    int allocMemPtr = 0;
    int respFieldsState = 0;
    void *respFields[2];
    struct taskLock *taskLocks = createTaskLocks();
    struct callLoadlib callStack[1024];
    int *callStackPtr = (int*)malloc(sizeof(int));
    (*callStackPtr) = 0;
    int chnBufListPtr = 0;
    struct chnBuf chnBufList[256];
    void *isamTx = NULL;
    struct currentNamesType currentNames;
    int i = 0;
    for (i= 0; i < 150; i++) eibbuf[i] = 0;
    xctlParams[0] = progname;
    memParams[0] = &memParam;
    cob_field* outputVars[100];
    outputVars[0] = NULL; // NULL terminated list
    cmdbuf[0] = 0x00;
    currentNames.currentMap[0] = 0x00;
    currentNames.currentMapSet[0] = 0x00;
    currentNames.fileName[0] = 0x00;
    currentNames.abendHandlerCnt = 0;
    currentNames.abcode[0] = 0x00;
    pthread_setspecific(childfdKey, fd);
    pthread_setspecific(cmdbufKey, cmdbuf);
    pthread_setspecific(cmdStateKey, &cmdState);
    pthread_setspecific(runStateKey, &runState);
    pthread_setspecific(cobFieldKey, &outputVars);
    pthread_setspecific(xctlStateKey, &xctlState);
    pthread_setspecific(xctlParamsKey, &xctlParams);
    pthread_setspecific(eibbufKey, &eibbuf);
    pthread_setspecific(eibaidKey, &eibaid);
    pthread_setspecific(linkAreaKey, linkArea);
    pthread_setspecific(linkAreaPtrKey, &linkAreaPtr);
    pthread_setspecific(linkAreaAdrKey, &linkAreaAdr);
    pthread_setspecific(commAreaKey, commArea);
    pthread_setspecific(commAreaPtrKey, &commAreaPtr);
    pthread_setspecific(areaModeKey, &areaMode);
    pthread_setspecific(linkStackKey, &linkStack);
    pthread_setspecific(linkStackPtrKey, &linkStackPtr);
    pthread_setspecific(memParamsKey, &memParams);
    pthread_setspecific(memParamsStateKey, &memParamsState);
    pthread_setspecific(twaKey, twa);
    pthread_setspecific(tuaKey, &tua);
    pthread_setspecific(allocMemKey, allocMem);
    pthread_setspecific(allocMemPtrKey, &allocMemPtr);
    pthread_setspecific(respFieldsStateKey, &respFieldsState);
    pthread_setspecific(respFieldsKey, &respFields);
    pthread_setspecific(taskLocksKey, taskLocks);
    pthread_setspecific(callStackKey, &callStack);
    pthread_setspecific(callStackPtrKey, callStackPtr);
    pthread_setspecific(chnBufListKey, &chnBufList);
    pthread_setspecific(chnBufListPtrKey, &chnBufListPtr);
    pthread_setspecific(currentNamesKey, &currentNames);
    
    // Oprionally read in content of commarea
    if (setCommArea == 1) {
      write(*(int*)fd,"COMMAREA\n",9);
      char c = 0x00;
      for (i = 0; i < 32768; ) {
        int n = read(*(int*)fd,&c,1);
        if (n == 1) {
          commArea[i] = c;
          i++;
        }
      }
    }
    
    cob_get_global_ptr()->cob_current_module = &thisModule;
    cob_get_global_ptr()->cob_call_params = 1;

    // Optionally initialize call params
    if ((parCount > 0) && (parCount <= 10)) {
        cob_get_global_ptr ()->cob_call_params = cob_get_global_ptr ()->cob_call_params + parCount;
        for (i = 0; i < parCount; i++) {
            char len[10];
            char c = 0x00;
            int pos = 0;
            while (c != '\n') {
                int n = read(*(int*)fd,&c,1);
                if ((n == 1) && (c != '\n') && (c != '\r') && (c != '\'') && (pos < 10)) {
                    len[pos] = c;
                    pos++;
                }
            }
            len[pos] = 0x00; 
            paramList[i] = (void*)&linkArea[linkAreaPtr];
            linkAreaAdr = &linkArea[linkAreaPtr];
            linkAreaPtr += atoi(len);
        }
    }

    // Set signal handler for SIGSEGV (in case of mem leak in load module)
    struct sigaction a;
    a.sa_handler = segv_handler;
    a.sa_flags = 0;
    sigemptyset( &a.sa_mask );
    sigaction( SIGSEGV, &a, NULL );

    initOpenDatasets(currentNames.openDatasets);
    initMain();
    execLoadModule(name,0,parCount);
    releaseLocks(TASK,taskLocks);
    globalCallCleanup();
    clearMain();
    free(allocMem);
    free(linkArea);
    free(commArea);
    free(twa);
    free(cmdbuf);
    clearChnBufList();
    closeOpenDatasets(currentNames.openDatasets);
    // Flush output buffers
    fflush(stdout);
    fflush(stderr);

    pthread_setspecific(childfdKey, NULL);
    pthread_setspecific(cmdbufKey, NULL);
    pthread_setspecific(cmdStateKey, NULL);
    pthread_setspecific(runStateKey, NULL);
    pthread_setspecific(cobFieldKey, NULL);
    pthread_setspecific(xctlStateKey, NULL);
    pthread_setspecific(xctlParamsKey, NULL);
    pthread_setspecific(eibbufKey, NULL);
    pthread_setspecific(eibaidKey, NULL);
    pthread_setspecific(linkAreaKey, NULL);
    pthread_setspecific(linkAreaPtrKey, NULL);
    pthread_setspecific(linkAreaAdrKey, NULL);
    pthread_setspecific(commAreaKey, NULL);
    pthread_setspecific(commAreaPtrKey, NULL);
    pthread_setspecific(areaModeKey, NULL);
    pthread_setspecific(linkStackKey, NULL);
    pthread_setspecific(linkStackPtrKey, NULL);
    pthread_setspecific(memParamsKey, NULL);
    pthread_setspecific(memParamsStateKey,NULL);
    pthread_setspecific(twaKey, NULL);
    pthread_setspecific(tuaKey, NULL);
    pthread_setspecific(allocMemKey, NULL);
    pthread_setspecific(allocMemPtrKey, NULL);
    pthread_setspecific(respFieldsStateKey, NULL);
    pthread_setspecific(respFieldsKey, NULL);
    pthread_setspecific(taskLocksKey, NULL);
    pthread_setspecific(callStackKey, NULL);
    pthread_setspecific(callStackPtrKey, NULL);
    pthread_setspecific(chnBufListKey, NULL);
    pthread_setspecific(chnBufListPtrKey, NULL);
    pthread_setspecific(currentNamesKey, NULL);
}


// Execute in existing DB transaction using load module callback for JNI
void execCallbackInTransaction(char *name, void *fd, int setCommArea, int parCount, void *callbackFunc) {
    pthread_setspecific(callbackFuncKey, callbackFunc);
    execInTransaction(name,fd,setCommArea,parCount);
    pthread_setspecific(callbackFuncKey, NULL);
}
