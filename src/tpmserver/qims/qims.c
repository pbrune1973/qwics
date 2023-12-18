/*******************************************************************************************/
/*   QWICS Server COBOL Information Management Service Functionality                               */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 18.05.2021                                  */
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
#include "qims.h"
#include "qimsdb.h"
#include "../env/envconf.h"

extern int execCallback(char *cmd, void *var);
extern pthread_key_t qimsInMsgCountKey;
extern pthread_key_t qimsOutMsgCountKey;
extern pthread_key_t qimsCIKey;


int cbltdli(unsigned char *commArea, int32_t *parcnt, unsigned char *func, unsigned char *pcbMask, 
            unsigned char *ioArea, unsigned char *key1, unsigned char *key2, 
            unsigned char *key3, unsigned char *key4, unsigned char *key5) {
  char fn[5];
  int32_t pars = 3;
  int *inMsgCount = (int*)pthread_getspecific(qimsInMsgCountKey);
  int *outMsgCount = (int*)pthread_getspecific(qimsOutMsgCountKey);
  int *ci = (int*)pthread_getspecific(qimsCIKey);
  // Check if parcnt is not present
  memcpy((void*)fn,(void*)parcnt,4);
  fn[4] = 0x00;
  if ((strcmp("GU  ",fn) == 0) || (strcmp("GHU ",fn) == 0) ||
      (strcmp("GN  ",fn) == 0) || (strcmp("GHN ",fn) == 0) ||
      (strcmp("GNP ",fn) == 0) || (strcmp("GHNP",fn) == 0) ||
      (strcmp("ISRT",fn) == 0) || (strcmp("DLET",fn) == 0) ||
      (strcmp("REPL",fn) == 0) || (strcmp("XRST",fn) == 0) ||
      (strcmp("CHKP",fn) == 0) || (strcmp("ROLB",fn) == 0) ||
      (strcmp("PURG",fn) == 0) || (strcmp("CHNG",fn) == 0)) {
    key5 = key4;
    key4 = key3;
    key3 = key2;
    key2 = key1;
    key1 = ioArea;
    ioArea = pcbMask;
    pcbMask = func;
    func = (unsigned char*)parcnt; 
    parcnt = &pars;
  } else {
    sprintf(fn,"%s",(char*)func);
    if (*parcnt < 3) {
      parcnt = &pars;
    }
  }
  printf("%d %s\n",*parcnt,fn);

  if (pcbMask[9] == 'D') {
    return cbltdli_db(*parcnt,fn,pcbMask,ioArea,key1,key2,key3,key4,key5);
  }
  
  pcbMask[10] = ' ';
  pcbMask[11] = ' ';

  char segment[9];
  memcpy((void*)segment,(void*)pcbMask,8);
  int p = 7;
  while ((p >= 0) && (segment[p] == ' ')) {
    p--;
  }
  segment[p+1] = 0x00;

  int resp = 0;
  int resp2 = 0;
  const cob_field_attr a_1 = {COB_TYPE_ALPHANUMERIC, 0, 0, 0x1000, NULL};
  const cob_field_attr a_2 = {COB_TYPE_NUMERIC_BINARY, 8, 0, 0x1000, NULL};
  cob_field buf = {2, (cob_u8_ptr)ioArea, &a_1};
  cob_field fresp = {4, (cob_u8_ptr)&resp, &a_2};
  cob_field fresp2 = {4, (cob_u8_ptr)&resp2, &a_2};
  if (strcmp("GU  ",fn) == 0) {
    *inMsgCount = 0;
    if ((key1 != NULL) && (*parcnt > 3)) {
      // Copy MOD name field from PCB if required
      memcpy((void*)key1,(void*)&pcbMask[24],8);
    }
    if (pcbMask[8] == 'C') {
      // Conversational program, read SPA first
      if (*ci == 0) {
        *inMsgCount = 1000;
      }
      *ci = cob_get_u64_compx((cob_u8_ptr)&ioArea[2], 4);
    }
printf("%s %d %d\n","GU",*inMsgCount,*ci);
    char msgStr[20];
    char channelStr[20];
    if (*inMsgCount >= 1000) {
      sprintf(channelStr,"%s","'QIMSSPA'");
      sprintf(msgStr,"%s%d%s","'MSG",(*inMsgCount)-1000,"'");
    } else {
      sprintf(channelStr,"%s","'QIMSIN'");
      sprintf(msgStr,"%s%d%s","'MSG",(*inMsgCount),"'");
    }
    execCallback("EXEC",NULL);   
    execCallback("CICS",NULL);   
    execCallback("GET",NULL);
    execCallback("CONTAINER",NULL);
    execCallback(msgStr,NULL);
    execCallback("CHANNEL",NULL);
    execCallback(channelStr,NULL);
    execCallback("INTO",NULL);
    execCallback("",&buf);
    execCallback("RESP",&fresp);   
    execCallback("RESP2",&fresp2);   
    execCallback("END-EXEC",NULL);   
    printf("RESP %d %d\n",resp,resp2);
    if ((resp > 0) && (resp != 22)) {
      pcbMask[10] = 'Q';
      pcbMask[11] = 'C';
      return 0;
    }
    int ll = cob_get_u64_compx((cob_u8_ptr)ioArea, 2);
    buf.size = ll;
    execCallback("EXEC",NULL);   
    execCallback("CICS",NULL);   
    execCallback("GET",NULL);
    execCallback("CONTAINER",NULL);
    execCallback(msgStr,NULL);
    execCallback("CHANNEL",NULL);
    execCallback(channelStr,NULL);
    execCallback("INTO",NULL);
    execCallback("",&buf);
    execCallback("RESP",&fresp);   
    execCallback("RESP2",&fresp2);   
    execCallback("END-EXEC",NULL);   
    printf("RESP %d %d\n",resp,resp2);
    if (resp > 0) {
      pcbMask[10] = 'Q';
      pcbMask[11] = 'C';
      return 0;
    }
    (*inMsgCount)++;
  }
  if (strcmp("GN  ",fn) == 0) {
    if (*inMsgCount < 1) {
      pcbMask[10] = 'Q';
      pcbMask[11] = 'E';
      return 0;
    }
    char msgStr[20];
    char channelStr[20];
    if (*inMsgCount >= 1000) {
      sprintf(channelStr,"%s","'QIMSSPA'");
      sprintf(msgStr,"%s%d%s","'MSG",(*inMsgCount)-1000,"'");
    } else {
      sprintf(channelStr,"%s","'QIMSIN'");
      sprintf(msgStr,"%s%d%s","'MSG",(*inMsgCount),"'");
    }
    execCallback("EXEC",NULL);   
    execCallback("CICS",NULL);   
    execCallback("GET",NULL);
    execCallback("CONTAINER",NULL);
    execCallback(msgStr,NULL);
    execCallback("CHANNEL",NULL);
    execCallback(channelStr,NULL);
    execCallback("INTO",NULL);
    execCallback("",&buf);
    execCallback("RESP",&fresp);   
    execCallback("RESP2",&fresp2);   
    execCallback("END-EXEC",NULL);   
    printf("RESP %d %d\n",resp,resp2);
    if ((resp > 0) && (resp != 22)) {
      pcbMask[10] = 'Q';
      pcbMask[11] = 'D';
      return 0;
    }
    int ll = cob_get_u64_compx((cob_u8_ptr)ioArea, 2);
    buf.size = ll;
    execCallback("EXEC",NULL);   
    execCallback("CICS",NULL);   
    execCallback("GET",NULL);
    execCallback("CONTAINER",NULL);
    execCallback(msgStr,NULL);
    execCallback("CHANNEL",NULL);
    execCallback(channelStr,NULL);
    execCallback("INTO",NULL);
    execCallback("",&buf);
    execCallback("RESP",&fresp);   
    execCallback("RESP2",&fresp2);   
    execCallback("END-EXEC",NULL);   
    printf("RESP %d %d\n",resp,resp2);
    if (resp > 0) {
      pcbMask[10] = 'Q';
      pcbMask[11] = 'D';
      return 0;
    }
    (*inMsgCount)++;
  }
  if (strcmp("ISRT",fn) == 0) {
    int ll = cob_get_u64_compx((cob_u8_ptr)ioArea, 2);
    buf.size = ll;
    int zz = cob_get_u64_compx((cob_u8_ptr)&ioArea[2], 4);
    char msgStr[20];
    char channelStr[20];
    if ((key1 != NULL) && (*parcnt > 3)) {
      // Copy MOD name field to PCB if required
      memcpy((void*)&pcbMask[24],(void*)key1,8);
    }
    if (zz == *ci) {
      sprintf(channelStr,"%s","'QIMSSPA'");
      if (*outMsgCount < 1000) {
        *outMsgCount = 1000;
      }
      sprintf(msgStr,"%s%d%s","'MSG",(*outMsgCount)-1000,"'");
    } else {
      sprintf(channelStr,"%s","'QIMSOUT'");
      if (*outMsgCount >= 1000) {
        *outMsgCount = 0;
      }
      sprintf(msgStr,"%s%d%s","'MSG",*outMsgCount,"'");
      if (pcbMask[9] == 'A') {
        char dest[13];
        sprintf(dest,"%s%s","DEST",segment);
        cob_field destf = {12, (cob_u8_ptr)&dest, &a_1};
        execCallback("EXEC",NULL);   
        execCallback("CICS",NULL);   
        execCallback("PUT",NULL);
        execCallback("CONTAINER",NULL);
        execCallback(msgStr,NULL);
        execCallback("CHANNEL",NULL);
        execCallback(channelStr,NULL);
        execCallback("FROM",NULL);
        execCallback("",&destf);
        execCallback("RESP",&fresp);   
        execCallback("RESP2",&fresp2);   
        execCallback("END-EXEC",NULL);   
        printf("RESP %d %d\n",resp,resp2);
        if (resp > 0) {
          pcbMask[10] = 'A';
          pcbMask[11] = 'Z';
          return 0;
        }
        (*outMsgCount)++;
        sprintf(msgStr,"%s%d%s","'MSG",*outMsgCount,"'");
      }
    }
printf("%s %d %d\n","ISRT",*outMsgCount,zz);
    execCallback("EXEC",NULL);   
    execCallback("CICS",NULL);   
    execCallback("PUT",NULL);
    execCallback("CONTAINER",NULL);
    execCallback(msgStr,NULL);
    execCallback("CHANNEL",NULL);
    execCallback(channelStr,NULL);
    execCallback("FROM",NULL);
    execCallback("",&buf);
    execCallback("RESP",&fresp);   
    execCallback("RESP2",&fresp2);   
    execCallback("END-EXEC",NULL);   
    printf("RESP %d %d\n",resp,resp2);
    if (resp > 0) {
      pcbMask[10] = 'A';
      pcbMask[11] = 'Z';
      return 0;
    }
    (*outMsgCount)++;
  }
  if (strcmp("CHKP",fn) == 0) {
    execCallback("EXEC",NULL);   
    execCallback("CICS",NULL);   
    execCallback("SYNCPOINT",NULL);   
    execCallback("END-EXEC",NULL);   
  }
  if (strcmp("ROLB",fn) == 0) {
    execCallback("EXEC",NULL);   
    execCallback("CICS",NULL);   
    execCallback("SYNCPOINT",NULL);   
    execCallback("ROLLBACK",NULL);   
    execCallback("END-EXEC",NULL);   
  }
  return 0;
}
