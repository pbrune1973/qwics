/*******************************************************************************************/
/*   QWICS Server COBOL Information Management Service DB Interface                             */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 27.05.2021                                  */
/*                                                                                         */
/*   Copyright (C) 2021 by Philipp Brune  Email: Philipp.Brune@hs-neu-ulm.de               */
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
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <libcob.h>
#include "../db/conpool.h"
#include "qimsdb.h"
#include "../env/envconf.h"

#ifdef __APPLE__
#include "../macosx/fmemopen.h"
#endif

extern pthread_key_t connKey;
extern pthread_key_t qimsSegAreaKey;
extern int getCobType(cob_field *f);


int getSegInfo(char *seg, struct seg_info_t *seg_info, PGconn *conn) {
  char sql[200];
  sprintf(sql,"%s%s%s","SELECT key_name, key_type, key_pos, key_len, seg_len FROM seg_info WHERE seg_name='",seg,"' AND key_role=0");
printf("%s\n",sql);
  PGresult *res = execSQLQuery(conn, sql);
  if ((res != NULL) && (PQresultStatus(res) == PGRES_TUPLES_OK)) {
    int cols = PQnfields(res);
    int rows = PQntuples(res);
    if ((cols == 5) && (rows == 1)) {
      seg_info->key_name[8] = 0x00;
      memcpy(seg_info->key_name,PQgetvalue(res,0,0),8);
      seg_info->key_type = ((char*)PQgetvalue(res,0,1))[0];
      seg_info->key_pos = atoi((char*)PQgetvalue(res,0,2));
      seg_info->key_len = atoi((char*)PQgetvalue(res,0,3));
      seg_info->seg_len = atoi((char*)PQgetvalue(res,0,4));
      PQclear(res);
      return 1;
    }
  }
  PQclear(res);
  return 0;
}


int getSegAltKeyInfo(char *seg, char *key_name, struct seg_info_t *seg_info, PGconn *conn) {
  char sql[200];
  sprintf(sql,"%s%s%s%s%s","SELECT key_name, key_type, key_pos, key_len, seg_len FROM seg_info WHERE seg_name='",seg,
                       "' AND key_name='",key_name,"' AND key_role=1");
printf("%s\n",sql);
  PGresult *res = execSQLQuery(conn, sql);
  if ((res != NULL) && (PQresultStatus(res) == PGRES_TUPLES_OK)) {
    int cols = PQnfields(res);
    int rows = PQntuples(res);
    if ((cols == 5) && (rows == 1)) {
      seg_info->key_name[8] = 0x00;
      memcpy(seg_info->key_name,PQgetvalue(res,0,0),8);
      seg_info->key_type = ((char*)PQgetvalue(res,0,1))[0];
      seg_info->key_pos = atoi((char*)PQgetvalue(res,0,2));
      seg_info->key_len = atoi((char*)PQgetvalue(res,0,3));
      seg_info->seg_len = atoi((char*)PQgetvalue(res,0,4));
      PQclear(res);
      return 1;
    }
  }
  PQclear(res);
  return 0;
}


void initSegArea(struct seg_area_t *segArea) {
  int i;
  for (i = 0; i < MAX_CURSORS; i++) {
    segArea->cursors[i][0] = 0x00;
  }
  segArea->segment[0] = 0x00;
}


int getCurName(struct seg_area_t *segArea, char *seg, char* sqlCond, char *curName) {
  char curLabel[1024];
  sprintf(curLabel,"%s%s%s",seg,",",sqlCond);
  int i;
  for (i = 0; i < MAX_CURSORS; i++) {
    if (strcmp(curLabel,segArea->cursors[i]) == 0) {
      sprintf(curName,"%s%d","C",i);
      return 2;
    }
  }
  for (i = 0; i < MAX_CURSORS; i++) {
    if (strlen(segArea->cursors[i]) == 0) {
      sprintf(segArea->cursors[i],"%s",curLabel);
      sprintf(curName,"%s%d","C",i);
      return 1;
    }
  }
  return 0;
}


int releaseCurName(struct seg_area_t *segArea, char *curName) {
  int i = atoi(&curName[1]);
  if (i >= 0 && i < MAX_CURSORS) {
    segArea->cursors[i][0] = 0x00;
    return 1;
  }
  return 0;
}


int holdRecord(char *seg, unsigned char *ioArea, char *sql, struct seg_area_t *segArea, unsigned char *pcbMask, PGconn *conn) {
  struct seg_info_t seg_info;
  if (getSegInfo(seg,&seg_info,conn)) {
    int end = strlen(sql);
    FILE *f1 = fmemopen(&sql[end], 1024-end, "w");

    cob_field_attr a_1 = {COB_TYPE_ALPHANUMERIC, 0, 0, 0x1000, NULL};
    switch (seg_info.key_type) {
        case 'P': a_1.type = COB_TYPE_NUMERIC_PACKED; break;
        case 'Z': a_1.type = COB_TYPE_NUMERIC; break;
        case 'H': a_1.type = COB_TYPE_NUMERIC_BINARY; break;
        case 'F': a_1.type = COB_TYPE_NUMERIC_BINARY; break;
    }
    cob_field keyval = {seg_info.key_len, NULL, &a_1};
    keyval.data = &ioArea[seg_info.key_pos];

    // PostgreSQL does not support - in names
    int j = 0;
    for (j = 0; j < 8; j++) {
      if (seg_info.key_name[j] == '-') {
        seg_info.key_name[j] = '_';
      }
    }

    if (sql[end-1] == ')') {
      fputs(" AND",f1);   
    } else {
      fputs(" WHERE ",f1);
    }
    fputs(" (",f1);
    fputs(seg_info.key_name,f1);
    putc('=',f1);
    if (COB_FIELD_TYPE(&keyval) == COB_TYPE_ALPHANUMERIC) putc('\'',f1);
    display_cobfield(&keyval,f1);
    if (COB_FIELD_TYPE(&keyval) == COB_TYPE_ALPHANUMERIC) putc('\'',f1);
    fputs(") FOR UPDATE",f1);

    putc(0x00,f1);
    fclose(f1);

    PGresult *res = PQexec(conn,sql);
    if ((res != NULL) && (PQresultStatus(res) == PGRES_TUPLES_OK)) {
      int cols = PQnfields(res);
      int rows = PQntuples(res);
      if ((cols == 1) && (rows == 1)) {
        memcpy(ioArea,PQgetvalue(res,0,0),seg_info.seg_len);
        PQclear(res);
        memcpy(segArea->segment,seg,8);
        segArea->segment[8] = 0x00;
        segArea->segInfo = seg_info;
        sprintf(segArea->sqlCond,"%s",strstr(sql,"WHERE"));
        return 1;
      } else {        
        pcbMask[10] = 'G';
        pcbMask[11] = 'E';
      }
    } else {
      printf("ERROR: %s", PQerrorMessage(conn));
      pcbMask[10] = 'G';
      pcbMask[11] = 'E';
    }
    PQclear(res);
  }
  return 0;
}


int cbltdli_db(int32_t parcnt, char *fn, unsigned char *pcbMask, 
               unsigned char *ioArea, unsigned char *key1, unsigned char *key2, 
               unsigned char *key3, unsigned char *key4, unsigned char *key5) {              
  printf("DB %d %s %x %x %x\n",parcnt,fn,key1,key2,key3);
  unsigned char *keys[5] = { key1, key2, key3, key4, key5 };

  if (pcbMask[9] != 'D') {
    return 0;
  }
  if ((parcnt < 4) && (strcmp("REPL",fn) != 0)) {
    if (strcmp("GU  ",fn) == 0) {
      parcnt = 4;
    } else {
      parcnt = 4;
      int i = 0;
      while ((i < 5) && (keys[i][8] != ' ')) {
        // Key is qualified SSA
        parcnt++;
        i++;
      }
    }
  }  

  pcbMask[10] = ' ';
  pcbMask[11] = ' ';

  PGconn *conn = (PGconn*)pthread_getspecific(connKey);
  struct seg_area_t *segArea = (struct seg_area_t*)pthread_getspecific(qimsSegAreaKey);

  char tableName[9];
  tableName[0] = 0x00;
  tableName[8] = 0x00;
  if (parcnt >= 4) {
    memcpy(tableName,keys[parcnt-4],8);
  } else {
    memcpy(tableName,segArea->segment,8);
  }

  char sqlStr1[1024];
  char sqlStr2[1024];
  sqlStr1[0] = 0x00;
  sqlStr2[0] = 0x00;
  FILE *f1 = fmemopen(sqlStr1, 1024, "w");
  FILE *f2 = fmemopen(sqlStr2, 1024, "w");

  unsigned char key_fb[256];
  int32_t key_fb_len = 0;
  struct seg_info_t seg_info;
  int i = 0;
  for (i = 0; i < (parcnt-3); i++) {
    char seg[9];
    memcpy(seg,keys[i],8);
    seg[8] = 0x00;
    if (getSegInfo(seg,&seg_info,conn)) {
      char keyName[9];
      keyName[0] = 0x00;
      keyName[8] = 0x00;
      if (keys[i][8] == '(') {
        memcpy(keyName,&keys[i][9],8);
        if (strcmp(keyName,seg_info.key_name) != 0) {
          if (!getSegAltKeyInfo(seg,keyName,&seg_info,conn)) {
            pcbMask[10] = 'A';
            pcbMask[11] = 'K';
            putc(0x00,f1);
            putc(0x00,f2);
            fclose(f1);
            fclose(f2);
            return 0;      
          }
        }
      }

      cob_field_attr a_1 = {COB_TYPE_ALPHANUMERIC, 0, 0, 0x1000, NULL};
      switch (seg_info.key_type) {
        case 'P': a_1.type = COB_TYPE_NUMERIC_PACKED; break;
        case 'Z': a_1.type = COB_TYPE_NUMERIC; break;
        case 'H': a_1.type = COB_TYPE_NUMERIC_BINARY; break;
        case 'F': a_1.type = COB_TYPE_NUMERIC_BINARY; break;
      }
      cob_field keyval = {seg_info.key_len, NULL, &a_1};
      if (keys[i][8] != '(') {
        // Unqualified SSA
        keyval.data = &ioArea[seg_info.key_pos];
      } else {
        keyval.data = &keys[i][19];
      }
      // PostgreSQL does not support - in names
      int j = 0;
      for (j = 0; j < 8; j++) {
        if (seg_info.key_name[j] == '-') {
          seg_info.key_name[j] = '_';
        }
      }
printf("%s %s %d %d\n","seg_info: ",seg_info.key_name,seg_info.seg_len,seg_info.key_len);
      memcpy(&key_fb[key_fb_len],keyval.data,keyval.size);
      key_fb_len += keyval.size;

      if (strcmp("ISRT",fn) == 0) {
        if (i > 0) {
          putc(',',f1);
          putc(',',f2);
        }
        fputs(seg_info.key_name,f1);
        if (COB_FIELD_TYPE(&keyval) == COB_TYPE_ALPHANUMERIC) putc('\'',f2);
        display_cobfield(&keyval,f2);
        if (COB_FIELD_TYPE(&keyval) == COB_TYPE_ALPHANUMERIC) putc('\'',f2);
      } else {
        if (keys[i][8] == '(') {
          // Only qualified SSA add selection criteria
          if (i > 0) fputs(" AND ",f1);
          fputs("(",f1);
          fputs(seg_info.key_name,f1);
          putc(keys[i][17],f1);
          putc(keys[i][18],f1);
          if (COB_FIELD_TYPE(&keyval) == COB_TYPE_ALPHANUMERIC) putc('\'',f1);
          display_cobfield(&keyval,f1);
          if (COB_FIELD_TYPE(&keyval) == COB_TYPE_ALPHANUMERIC) putc('\'',f1);

          int ssaPos = 19 + keyval.size;
          while (keys[i][ssaPos] != ')') {
            if (keys[i][ssaPos] == '&') {
              fputs(" AND ",f1);
            } else {
              fputs(" OR ",f1);
            }
            ssaPos++;

          }
          fputs(")",f1);
        }
      }
    } else {
      pcbMask[10] = 'A';
      pcbMask[11] = 'K';
      putc(0x00,f1);
      putc(0x00,f2);
      fclose(f1);
      fclose(f2);
      return 0;      
    }
  }

  putc(0x00,f1);
  putc(0x00,f2);
  fclose(f1);
  fclose(f2);

  if (strcmp("ISRT",fn) == 0) {
    char sql[1024];
    sprintf(sql,"%s%s%s%s%s%s%s","INSERT INTO ",tableName,"(",sqlStr1,",data) VALUES(",
            sqlStr2,",$1::bytea)");
    char *paramValues[1];
    paramValues[0] = (char*)ioArea;
    int paramLengths[1];
    paramLengths[0] = seg_info.seg_len;
    int paramFormats[1];
    paramFormats[0] = 1;
printf("%s\n",sql);
    PGresult *res = PQexecParams(conn,sql,1,NULL,paramValues,paramLengths,paramFormats,1);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
      printf("%s\n",PQerrorMessage(conn));
    }
    PQclear(res);
  }
  if (strcmp("REPL",fn) == 0) {
    char sql[1024];
    char keyUpdate[1024];
    struct seg_info_t seg_info = segArea->segInfo;
    FILE *f1 = fmemopen(keyUpdate, 1024, "w");

    cob_field_attr a_1 = {COB_TYPE_ALPHANUMERIC, 0, 0, 0x1000, NULL};
    switch (seg_info.key_type) {
        case 'P': a_1.type = COB_TYPE_NUMERIC_PACKED; break;
        case 'Z': a_1.type = COB_TYPE_NUMERIC; break;
        case 'H': a_1.type = COB_TYPE_NUMERIC_BINARY; break;
        case 'F': a_1.type = COB_TYPE_NUMERIC_BINARY; break;
    }
    cob_field keyval = {seg_info.key_len, NULL, &a_1};
    keyval.data = &ioArea[seg_info.key_pos];

    // PostgreSQL does not support - in names
    int j = 0;
    for (j = 0; j < 8; j++) {
      if (seg_info.key_name[j] == '-') {
        seg_info.key_name[j] = '_';
      }
    }

    fputs(seg_info.key_name,f1);
    putc('=',f1);
    if (COB_FIELD_TYPE(&keyval) == COB_TYPE_ALPHANUMERIC) putc('\'',f1);
    display_cobfield(&keyval,f1);
    if (COB_FIELD_TYPE(&keyval) == COB_TYPE_ALPHANUMERIC) putc('\'',f1);

    putc(0x00,f1);
    fclose(f1);

    sprintf(sql,"%s%s%s%s%s%s","UPDATE ",segArea->segment," SET data=1::bytea,",keyUpdate," ",
                segArea->sqlCond);
    char *paramValues[1];
    paramValues[0] = (char*)ioArea;
    int paramLengths[1];
    paramLengths[0] = seg_info.seg_len;
    int paramFormats[1];
    paramFormats[0] = 1;
printf("%s\n",sql);
    PGresult *res = PQexecParams(conn,sql,1,NULL,paramValues,paramLengths,paramFormats,1);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
      printf("%s\n",PQerrorMessage(conn));
    }
    PQclear(res);
  }
  if (strcmp("DLET",fn) == 0) {
    char sql[1024];
    struct seg_info_t seg_info = segArea->segInfo;

    sprintf(sql,"%s%s%s%s","DELETE FROM ",segArea->segment," ",segArea->sqlCond);
printf("%s\n",sql);
    PGresult *res = PQexec(conn,sql);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
      printf("%s\n",PQerrorMessage(conn));
    }
    PQclear(res);
  }
  if ((strcmp("GU  ",fn) == 0) || (strcmp("GHU ",fn) == 0)) {
    char sql[1024];
    if (strlen(sqlStr1) > 0) {
      sprintf(sql,"%s%s%s%s","SELECT data FROM ",tableName," WHERE ",sqlStr1);
    } else {
      sprintf(sql,"%s%s","SELECT data FROM ",tableName);
    }
printf("%s\n",sql);

    PGresult *res = PQexec(conn,sql);
    if ((res != NULL) && (PQresultStatus(res) == PGRES_TUPLES_OK)) {
      int cols = PQnfields(res);
      int rows = PQntuples(res);
      if ((cols == 1) && (rows >= 1)) {
        memcpy(ioArea,PQgetvalue(res,0,0),seg_info.seg_len);
      } else {        
        pcbMask[10] = 'G';
        pcbMask[11] = 'E';
      }
    } else {
      printf("ERROR: %s", PQerrorMessage(conn));
      pcbMask[10] = 'G';
      pcbMask[11] = 'E';
    }
    PQclear(res);

    if (fn[1] == 'H') {
      holdRecord(tableName,ioArea,sql,segArea,pcbMask,conn);  
    }
  }
  if ((strcmp("GN  ",fn) == 0) || (strcmp("GHN ",fn) == 0) || 
      (strcmp("GNP ",fn) == 0) || (strcmp("GHNP",fn) == 0)) {
    char sql[1024];
    char curName[9];
    int r = getCurName(segArea,tableName,sqlStr1,curName);
    if (r == 2) {
      // Open cursor
      if (strlen(sqlStr1) > 0) {
        sprintf(sql,"%s%s%s%s%s%s","DECLARE ",curName," CURSOR FOR SELECT data FROM ",
                    tableName," WHERE ",sqlStr1);
      } else {
        sprintf(sql,"%s%s%s%s","DECLARE ",curName," CURSOR FOR SELECT data FROM ",
                    tableName);
      }
printf("%s\n",sql);
      PGresult *res = PQexec(conn,sql);
      if ((res != NULL) && (PQresultStatus(res) != PGRES_COMMAND_OK)) {
        releaseCurName(segArea,curName);        
        printf("ERROR: %s", PQerrorMessage(conn));
        pcbMask[10] = 'G';
        pcbMask[11] = 'E';
        r = 0;
      }
      PQclear(res);
    }
    if (r > 0) {
      // Fetch cursor
      sprintf(sql,"%s%s","FETCH NEXT FROM ",curName);
printf("%s\n",sql);
      PGresult *res = PQexec(conn,sql);
      if ((res != NULL) && (PQresultStatus(res) == PGRES_TUPLES_OK)) {
        int cols = PQnfields(res);
        int rows = PQntuples(res);
        if ((cols == 1) && (rows == 1)) {
          memcpy(ioArea,PQgetvalue(res,0,0),seg_info.seg_len);
          if (fn[1] == 'H') {
            holdRecord(tableName,ioArea,sql,segArea,pcbMask,conn);  
          }
        } else {        
          // Close cursor
          PQclear(res);
          sprintf(sql,"%s%s","CLOSE ",curName);
printf("%s\n",sql);
          PGresult *res = PQexec(conn,sql);
          if ((res != NULL) && (PQresultStatus(res) != PGRES_COMMAND_OK)) {
            printf("ERROR: %s", PQerrorMessage(conn));
          }
          releaseCurName(segArea,curName);  
          pcbMask[10] = 'G';
          pcbMask[11] = 'B';
        }
      } else {
        releaseCurName(segArea,curName);        
        printf("ERROR: %s", PQerrorMessage(conn));
        pcbMask[10] = 'G';
        pcbMask[11] = 'E';
      }
      PQclear(res);
    } else {
      // Error
      pcbMask[10] = 'G';
      pcbMask[11] = 'E';
    }
  }

  memcpy(&pcbMask[28],&key_fb_len,4);
  memcpy(&pcbMask[36],key_fb,key_fb_len);
  return 0;
}
