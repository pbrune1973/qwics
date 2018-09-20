/*******************************************************************************************/
/*   QWICS Server COBOL load module executor                                               */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 20.09.2018                                  */
/*                                                                                         */
/*   Copyright (C) 2018 by Philipp Brune  Email: Philipp.Brune@hs-neu-ulm.de               */
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
#include "db/conpool.h"

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
pthread_key_t xctlParamsKey;
pthread_key_t eibbufKey;
pthread_key_t linkAreaKey;
pthread_key_t linkAreaPtrKey;
pthread_key_t commAreaKey;
pthread_key_t commAreaPtrKey;
pthread_key_t areaModeKey;

// Callback function declared in libcob
extern int (*performEXEC)(char*, void*);

// SQLCA
cob_field *sqlcode = NULL;
char currentMap[9];


void writeJson(char *map, char *mapset, int childfd) {
    int n = 0, l = strlen(map), found = 0, brackets = 0;
    write(childfd,"JSON=",5);
    char jsonFile[255];
    sprintf(jsonFile,"%s%s%s","../copybooks/",mapset,".js");
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
                            cob_put_picx(outputVars[i]->data,outputVars[i]->size,PQgetvalue(res, 0, i));
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


// Execute COBOL loadmod in transaction
void execLoadModule(char *name, int mode) {
    int (*loadmod)();
    char fname[255];
    char response[1024];
    int childfd = *((int*)pthread_getspecific(childfdKey));
    char *commArea = (char*)pthread_getspecific(commAreaKey);

    sprintf(fname,"%s%s%s","../loadmod/",name,".so");
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
        } else {
            if (mode == 0) {
                sprintf(response,"%s\n","OK");
                write(childfd,&response,strlen(response));
            }
            (*loadmod)(commArea);
            if (mode == 0) {
                sprintf(response,"\n%s\n","STOP");
                write(childfd,&response,strlen(response));
            }
        }
        dlclose(sdl_library);
    }
}


int execCallback(char *cmd, void *var) {
    int childfd = *((int*)pthread_getspecific(childfdKey));
    char *cmdbuf = (char*)pthread_getspecific(cmdbufKey);
    int *cmdState = (int*)pthread_getspecific(cmdStateKey);
    cob_field **outputVars = (cob_field**)pthread_getspecific(cobFieldKey);
    char *end = &cmdbuf[strlen(cmdbuf)];
    int *xctlState = (int*)pthread_getspecific(xctlStateKey);
    char **xctlParams = (char**)pthread_getspecific(xctlParamsKey);
    char *eibbuf = (char*)pthread_getspecific(eibbufKey);
    char *linkArea = (char*)pthread_getspecific(linkAreaKey);
    int *linkAreaPtr = (int*)pthread_getspecific(linkAreaPtrKey);
    char *commArea = (char*)pthread_getspecific(commAreaKey);
    int *commAreaPtr = (int*)pthread_getspecific(commAreaPtrKey);
    int *areaMode = (int*)pthread_getspecific(areaModeKey);

    if (strstr(cmd,"SET SQLCODE") && (var != NULL)) {
        sqlcode = var;
        return 1;
    }
    if (strstr(cmd,"SET EIBCALEN")) {
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
    if (strstr(cmd,"SET EIBAID")) {
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
    if (strstr(cmd,"SET DFHEIBLK")) {
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
        } /* Currently not used
        else {
            cobvar->data = (unsigned char*)&commArea[*commAreaPtr];
            (*commAreaPtr) += (size_t)cobvar->size;
        }
        printf("%s%s%d%s%d%s%d%s%d\n",cmd," ",(long)cobvar->data," ",(int)cobvar->size," ",(*linkAreaPtr)," ",                                      (*commAreaPtr));
        */
    }

    if (strstr(cmd,"CICS")) {
        cmdbuf[0] = 0x00;
        (*cmdState) = -1;
        return 1;
    }
    if ((*cmdState) < 0) {
        if (strstr(cmd,"SEND")) {
            sprintf(cmdbuf,"%s%s",cmd,"\n");
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
            return 1;
        }
        if (strstr(cmd,"RECEIVE")) {
            sprintf(cmdbuf,"%s%s",cmd,"\n");
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
            (*cmdState) = -2;
            return 1;
        }
        if (strstr(cmd,"XCTL")) {
            sprintf(cmdbuf,"%s%s",cmd,"\n");
            write(childfd,cmdbuf,strlen(cmdbuf));
            cmdbuf[0] = 0x00;
            (*cmdState) = -3;
            (*xctlState) = 0;
            return 1;
        }
        if (strstr(cmd,"END-EXEC")) {
            cmdbuf[0] = 0x00;
            outputVars[0] = NULL; // NULL terminated list
            write(childfd,"\n",1);
            if (((*cmdState) == -3) && ((*xctlState) >= 1)) {
                // XCTL
                (*xctlState) = 0;
                (*cmdState) = 0;
                //printf("%s%s\n","XCTL ",xctlParams[0]);
                execLoadModule(xctlParams[0],1);
            }
            (*cmdState) = 0;
            return 1;
        }
        if ((var == NULL) || strstr(cmd,"MAP") || strstr(cmd,"MAPSET") || strstr(cmd,"DATAONLY") ||
            strstr(cmd,"ERASE") || strstr(cmd,"MAPONLY") || strstr(cmd,"RETURN") || strstr(cmd,"FROM") ||
            strstr(cmd,"INTO") || strstr(cmd,"HANDLE") || strstr(cmd,"CONDITION") || strstr(cmd,"ERROR") ||
            strstr(cmd,"INTO") || strstr(cmd,"MAPFAIL") || strstr(cmd,"NOTFND") || strstr(cmd,"ASSIGN") ||
            strstr(cmd,"SYSID") || strstr(cmd,"TRANSID") || strstr(cmd,"COMMAREA") || strstr(cmd,"LENGTH") ||
            strstr(cmd,"CONTROL") || strstr(cmd,"FREEKB") || strstr(cmd,"PROGRAM") || strstr(cmd,"XCTL")) {
            sprintf(end,"%s%s",cmd,"\n");
            if (((*cmdState) == -3) && ((*xctlState) == 1)) {
                // XCTL PROGRAM param value
                char *progname = (cmd+4);
                int l = strlen(progname);
                if (l > 8) l = 8;
                int i = l-1;
                while ((i > 0) &&
                       ((progname[i]==' ') || (progname[i]=='\'') ||
                        (progname[i]==10) || (progname[i]==13)))
                    i--;
                progname[i+1] = 0x00;
                sprintf(xctlParams[0],"%s",progname);
                (*xctlState) = 10;
            }
            if ((*cmdState) == -3) {
                if (strstr(cmd,"PROGRAM")) {
                    (*xctlState) = 1;
                }
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
                    if (cobvar->data[0] != 0) {
                        display_cobfield(cobvar,f);
                    }
                    if (COB_FIELD_TYPE(cobvar) == COB_TYPE_ALPHANUMERIC) putc('\'',f);
                    putc(0x00,f);
                    fclose(f);
                    write(childfd,cmdbuf,strlen(cmdbuf));
                    write(childfd,"\n",1);
                }
                if ((*cmdState) == -2) {
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
                    printf("%s\n",buf);
                    cob_put_picx(cobvar->data,cobvar->size,buf);
                }
                if ((*cmdState) == -3) {
                    sprintf(end,"%s%s",cmd,"=");
                    end = &cmdbuf[strlen(cmdbuf)];
                    FILE *f = fmemopen(end, 2048-strlen(cmdbuf), "w");
                    if (COB_FIELD_TYPE(cobvar) == COB_TYPE_ALPHANUMERIC) putc('\'',f);
                    if (cobvar->data[0] != 0) {
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
                        if (l > 8) l = 8;
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
                if (cobvar->data[0] != 0) {
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
void initExec() {
    performEXEC = &execCallback;
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
    setUpPool(10, "dbname=pbrune");
    currentMap[0] = 0x00;
}


void clearExec() {
    tearDownPool();
}


void execTransaction(char *name, void *fd) {
    char cmdbuf[2048];
    int cmdState = 0;
    int xctlState = 0;
    char progname[9];
    char *xctlParams[10];
    char eibbuf[150];
    char linkArea[4096];
    char commArea[4096];
    int linkAreaPtr = 0;
    int commAreaPtr = 0;
    int areaMode = 0;
    int i = 0;
    for (i= 0; i < 150; i++) eibbuf[i] = 0;
    xctlParams[0] = progname;
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
    PGconn *conn = getDBConnection();
    pthread_setspecific(connKey, (void*)conn);
    execLoadModule(name,0);
    returnDBConnection(conn,1);
}


// Exec COBOL module within an existing DB transaction
void execInTransaction(char *name, void *fd) {
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
    int i = 0;
    for (i= 0; i < 150; i++) eibbuf[i] = 0;
    xctlParams[0] = progname;
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
    execLoadModule(name,0);
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
