/*******************************************************************************************/
/*   QWICS Server COBOL Message Queueing Manager                                               */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 21.10.2018                                  */
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
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

// Declared in cobexec.c
extern pthread_key_t childfdKey;


void readParam(int childfd, char *buf) {
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


#define flipint(val) (((val & 0xFF000000) >> 24) | ((val & 0x00FF0000) >> 8) | ((val & 0x0000FF00) << 8) | ((val & 0x000000FF) << 24))


int qopen(int32_t *con, unsigned char *objDesc, int32_t *opts, int32_t *obj,
         int32_t *compcode, int32_t *reason) {
  char param[100];
  // Get connection to caller program
  int i,childfd = *((int*)pthread_getspecific(childfdKey));
  write(childfd,"QOPEN\n",6);

  int32_t *objType = (int32_t*)&objDesc[8];
  sprintf(param,"%d\n",flipint(*objType));
  write(childfd,param,strlen(param));

  char *objName = (char*)&objDesc[12];
  write(childfd,objName,48);
  write(childfd,"\n",1);

  sprintf(param,"%d\n",flipint(*opts));
  write(childfd,param,strlen(param));

  *con = 4711;

  readParam(childfd,param);
  *obj = flipint((int32_t)atoi(param));

  readParam(childfd,param);
  *compcode = flipint((int32_t)atoi(param));

  readParam(childfd,param);
  *reason = flipint((int32_t)atoi(param));

  write(childfd,"\n",1);
  return *compcode;
}


int qclose(int32_t *con, int32_t *obj, int32_t *opts,
         int32_t *compcode, int32_t *reason) {
  char param[100];
  // Get connection to caller program
  int childfd = *((int*)pthread_getspecific(childfdKey));
  write(childfd,"QCLOSE\n",7);

  sprintf(param,"%d\n",flipint(*obj));
  write(childfd,param,strlen(param));

  sprintf(param,"%d\n",flipint(*opts));
  write(childfd,param,strlen(param));

  readParam(childfd,param);
  *compcode = flipint((int32_t)atoi(param));

  readParam(childfd,param);
  *reason = flipint((int32_t)atoi(param));

  write(childfd,"\n",1);
  return *compcode;
}


int qget(int32_t *con, int32_t *obj, unsigned char *msgDesc, unsigned char *msgOpts,
         int32_t *buflen, unsigned char *msgBuf, int32_t *datalen,
         int32_t *compcode, int32_t *reason) {
  char param[100];
  // Get connection to caller program
  int i,childfd = *((int*)pthread_getspecific(childfdKey));
  write(childfd,"QGET\n",5);

  sprintf(param,"%d\n",flipint(*obj));
  write(childfd,param,strlen(param));

  for (i = 0; i < 364; i++) {
    write(childfd,&msgDesc[i],1);
  }
  for (i = 0; i < 100; i++) {
    write(childfd,&msgOpts[i],1);
  }

  int l = flipint(*buflen);
  sprintf(param,"%d\n",l);
  write(childfd,param,strlen(param));

  char c;
  for (i = 0; i < 364; i++) {
      int r = read(childfd,&c,1);
      msgDesc[i] = c;
  }

  for (i = 0; i < l; i++) {
      int r = read(childfd,&c,1);
      msgBuf[i] = c;
  }

  readParam(childfd,param);
  *datalen = flipint((int32_t)atoi(param));

  readParam(childfd,param);
  *compcode = flipint((int32_t)atoi(param));

  readParam(childfd,param);
  *reason = flipint((int32_t)atoi(param));

  write(childfd,"\n",1);
  return *compcode;
}


int qput(int32_t *con, int32_t *obj, unsigned char *msgDesc, unsigned char *msgOpts,
         int32_t *buflen, unsigned char *msgBuf,
         int32_t *compcode, int32_t *reason) {
  char param[100];
  // Get connection to caller program
  int i,childfd = *((int*)pthread_getspecific(childfdKey));
  write(childfd,"QPUT\n",5);

  sprintf(param,"%d\n",flipint(*obj));
  write(childfd,param,strlen(param));

  for (i = 0; i < 364; i++) {
    write(childfd,&msgDesc[i],1);
  }
  for (i = 0; i < 152; i++) {
    write(childfd,&msgOpts[i],1);
  }

  sprintf(param,"%d\n",flipint(*buflen));
  write(childfd,param,strlen(param));
  write(childfd,msgBuf,flipint(*buflen));

  char c;
  for (i = 0; i < 364; i++) {
      int r = read(childfd,&c,1);
      msgDesc[i] = c;
  }

  readParam(childfd,param);
  *compcode = flipint((int32_t)atoi(param));

  readParam(childfd,param);
  *reason = flipint((int32_t)atoi(param));

  write(childfd,"\n",1);
  return *compcode;
}


void* callCallback(char *name) {
    char ucname[20];
    int i = 0;
    for (i = 0; i < strlen(name); i++) {
      ucname[i] = toupper(name[i]);
    }
    ucname[i] = 0x00;

    if (strcmp("MQOPEN",ucname) == 0) {
        return (void*)&qopen;
    }
    if (strcmp("MQCLOSE",ucname) == 0) {
        return (void*)&qclose;
    }
    if (strcmp("MQGET",ucname) == 0) {
        return (void*)&qget;
    }
    if (strcmp("MQPUT",ucname) == 0) {
        return (void*)&qput;
    }

    return NULL;
}
