/*******************************************************************************************/
/*   QWICS Server COBOL embedded SQL executor                                               */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 10.08.2020                                  */
/*                                                                                         */
/*   Copyright (C) 2018 - 2020 by Philipp Brune  Email: Philipp.Brune@qwics.org            */
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
#include <string.h>

#include <libcob.h>
#include "../tpmserver/config.h"
#include "../tpmserver/env/envconf.h"
#include "db.h"

#ifdef __APPLE__
#include "macosx/fmemopen.h"
#endif

// SQLCA
cob_field *sqlcode = NULL;

char *connectStr = NULL;
PGconn *conn = NULL;
char _cmdbuf[2048];
int _cmdState = 0;
char *end = NULL;
cob_field* outputVars[100];

// Callback function declared in libcob
extern int (*performEXEC)(char*, void*);


void init() {
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


void setSQLCA(int code, char *state) {
    if (sqlcode != NULL) {
        cob_field sqlstate = { 5, sqlcode->data+119, NULL };
        cob_set_int(sqlcode,code);
        cob_put_picx(sqlstate.data,sqlstate.size,state);
    }
}


// Callback handler for EXEC statements
int processCmd(char *cmd) {
    char *pos;
    if ((pos=strstr(cmd,"EXEC SQL")) != NULL) {
        char *sql = (char*)pos+9;
        if (conn == NULL) {
            init();
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
    return 1;
}


int execCallback(char *cmd, void *var) {
    char *cmdbuf = _cmdbuf;
    int *cmdState = &_cmdState;
    char *end = &cmdbuf[strlen(cmdbuf)];

    if (strstr(cmd,"SET SQLCODE") && (var != NULL)) {
        sqlcode = var;
        return 1;
    }

    if (strstr(cmd,"END-EXEC")) {
        cmdbuf[strlen(cmdbuf)-1] = '\n';
        cmdbuf[strlen(cmdbuf)] = 0x00;
        cmdbuf[strlen(cmdbuf)-1] = 0x00;
        processCmd(cmdbuf);
        cmdbuf[0] = 0x00;
        (*cmdState) = 0;
        outputVars[0] = NULL; // NULL terminated list
    } else {
        if ((strlen(cmd) == 0) && (var != NULL)) {
            cob_field *cobvar = (cob_field*)var;
            if ((*cmdState) < 2) {
                FILE *f = fmemopen(end, 2048-strlen(cmdbuf), "w");
                if (COB_FIELD_TYPE(cobvar) == COB_TYPE_ALPHANUMERIC) putc('\'',f);
                if ((cobvar->data[0] != 0) || (getCobType(cobvar) == COB_TYPE_NUMERIC_BINARY) || 
                    (getCobType(cobvar) == COB_TYPE_NUMERIC_COMP5) ||
                    (getCobType(cobvar) == COB_TYPE_NUMERIC) || 
                    (getCobType(cobvar) == COB_TYPE_NUMERIC_PACKED)) {
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


static int (*loadmod)();
     

int main(int argc, char *argv[]) {
    int ret = 0;

    if (argc < 2) {
        fprintf(stderr," Usage: batchrun <loadmod>\n");
        return 1;
    } 

    fprintf(stdout,"Starting batchrun of %s\n",argv[1]);
    performEXEC = &execCallback;
    outputVars[0] = NULL; // NULL terminated list
    _cmdbuf[0] = 0x00;

    cob_init(0, NULL);

    loadmod = cob_resolve(argv[1]);     
    if (loadmod == NULL) {
        fprintf(stderr, "%s\n", cob_resolve_error());
        exit(1);
    }
     
    // Opening DB connection
    conn = getDBConnection(GETENV_STRING(connectStr,"QWICS_DB_CONNECTSTR","dbname=qwics"));
    ret = loadmod();
    PQfinish(conn);
    cob_stop_run(ret);

    return ret;
}
