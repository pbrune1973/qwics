/*******************************************************************************************/
/*   QWICS Server COBOL embedded SQL executor                                              */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 20.08.2023                                  */
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
#include <time.h>
#include <dlfcn.h>
#include <errno.h>

#include <libcob.h>
#include "../tpmserver/config.h"
#include "../tpmserver/env/envconf.h"
#include "db.h"
#include "libjclpars.h"

#ifdef __APPLE__
#include "macosx/fmemopen.h"
#endif

#define CMDBUF_SIZE 65536

// SQLCA
cob_field *sqlcode = NULL;

char *connectStr = NULL;
PGconn *conn = NULL;
char _cmdbuf[CMDBUF_SIZE];
int _cmdState = 0;
char *end = NULL;
cob_field* outputVars[100];

cob_module thisModule;

char *cobDateFormat = "YYYY-MM-dd-hh.mm.ss.uuuuu";
//char *dbDateFormat = "dd-MM-YYYY hh:mm:ss.uuu";
char *dbDateFormat = "YYYY-MM-dd hh:mm:ss.uuu";
char result[30];
char *loadmodDir = NULL;

// Callback function declared in libcob
extern int (*performEXEC)(char*, void*);


void init() {
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
    return 1;
}


int execLoadModule(char *name) {
    int ret = 0;
    int (*loadmod)();
    char fname[255];

    #ifdef __APPLE__
    sprintf(fname,"%s%s%s%s",GETENV_STRING(loadmodDir,"QWICS_LOADMODDIR","../loadmod"),"/",name,".dylib");
    #else
    sprintf(fname,"%s%s%s%s",GETENV_STRING(loadmodDir,"QWICS_LOADMODDIR","../loadmod"),"/",name,".so");
    #endif

    char *error = "";
    void* sdl_library = dlopen(fname, RTLD_LAZY);
    if ((error = dlerror()) != NULL || sdl_library == NULL)  {
        printf("%s%s%s%s\n","ERROR: ",error," for load module ",fname);
        return -1;
    } else {
        dlerror();
        *(void**)(&loadmod) = dlsym(sdl_library,name);
        if ((error = dlerror()) != NULL)  {
            printf("%s%s\n","ERROR: ",error);
            dlclose(sdl_library);
            return -1;
        }

        cob_get_global_ptr()->cob_current_module = &thisModule;
        cob_get_global_ptr()->cob_call_params = 0;
        ret = (*loadmod)();
        dlclose(sdl_library);
    }

    return ret;
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
                if (COB_FIELD_TYPE(cobvar) == COB_TYPE_GROUP) {
		            // Treat as VARCHAR field
		            unsigned int l = (unsigned int)cobvar->data[0];	
                    l = (l << 8) | (unsigned int)cobvar->data[1];
		            if (l > (cobvar->size-2)) {
                       l = cobvar->size-2;
                    }
                    end[0] = '\'';
                    int i = 0;
                    for (i = 0; i < l; i++) {
                        unsigned char c = cobvar->data[i+2];
                        if ((c & 0x80) == 0) {
                           // Plain ASCII
                           end[1+i] = c; 
                        } else {
                           // Convert ext. ASCII to UTF-8
                           unsigned char c1 = 0xC0;
                           c1 = c1 | ((c & 0xC0) >> 6);
                           end[1+i] = c1; 
                           i++;
                           c1 = 0x80;
                           c1 = c1 | (c & 0x3F);
                           end[1+i] = c1;
                        }
                    }
                    end[1+i] = '\'';
                    end[2+i] = ' ';
                    end[3+i] = 0x00;
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


int batchrun(int argc, char *argv[]) {
    int ret = 0;

    if (argc == 5) {
        job = argv[2];
        step = argv[3];
        pgm = argv[4];
    }

    GETENV_STRING(cobDateFormat,"QWICS_COBDATEFORMAT","YYYY-MM-dd.hh:mm:ss.uuuu");

    fprintf(stdout,"Starting batchrun of %s\n",argv[1]);
    performEXEC = &execCallback;
    outputVars[0] = NULL; // NULL terminated list
    _cmdbuf[0] = 0x00;

    cob_init(0, NULL);
     
    // Opening DB connection
    conn = getDBConnection(GETENV_STRING(connectStr,"QWICS_DB_CONNECTSTR","dbname=qwics"));

    // Run the loadmod
    ret = execLoadModule(argv[1]);

    PGresult *res = PQexec(conn, "COMMIT");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        fprintf(stderr,"ERROR: COMMIT failed:\n%s", PQerrorMessage(conn));
        PQexec(conn, "ROLLBACK");
    }
    PQclear(res);

    PQfinish(conn);
    cob_stop_run(ret);

    return ret;
}
