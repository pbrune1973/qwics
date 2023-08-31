/*******************************************************************************************/
/*   QWICS Batch Job Entry System                                                          */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 19.08.2023                                  */
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

#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "spool/SpoolingSystem.h"
#include "dataset/TOC.h"
#include "env/envconf.h"

using namespace std;


void *runCardReader(void *reader) {
  ((CardReader*)reader)->run();
  delete ((CardReader*)reader);
  return NULL;
}


void *runJobListener(char *udsfile) {
    int parentfd; /* parent socket */
    int childfd; /* child socket */
    int clientlen; /* byte size of client's address */
    struct sockaddr_in serveraddr; /* server's addr */
    struct sockaddr_un serversockaddr; /* server's addr for domain socket*/

    parentfd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (parentfd < 0) {
      printf("%s\n","ERROR opening socket");
      exit(1);
    }

    unlink(udsfile);
    serversockaddr.sun_family = AF_LOCAL;
    strcpy(serversockaddr.sun_path, udsfile);

    /*
     * bind: associate the parent socket with a port
    */
    if (bind(parentfd, (struct sockaddr *) &serversockaddr, sizeof(serversockaddr)) < 0) {
      printf("%s\n","ERROR on binding");
      exit(1);
    }
    clientlen = sizeof(serversockaddr);

    /*
      * listen: make this socket ready to accept connection requests
     */
    if (listen(parentfd, 5) < 0) {
      printf("%s\n","ERROR on listen");
      exit(1);
    }
    int stop = 0;

    do {
      childfd = accept(parentfd, (struct sockaddr *) &serversockaddr, (socklen_t*)&clientlen);

      if (childfd == -1) {
        if (errno == EINTR) {
          break;
        }
      }

      if (childfd >= 0) {
          CardReader *reader = new CardReader(childfd,childfd);
          runCardReader((void*)reader);
      } else {
          cout << " ERROR: Unable to accept connection!" << endl;
      }
    } while (!stop);

    return NULL;
}


int main(int argc, char **argv) {
    char *datasetDir = NULL;
    char *spoolDir = NULL;
    char *workingDir = NULL;
    char *configFile = NULL;
    char *sockFile = NULL;
    
    TOC::addToc(GETENV_STRING(datasetDir,"QWICS_DATASET_DIR","../dataset"));
    SpoolingSystem::create(GETENV_STRING(configFile,"QWICS_BATCH_CONFIG","../config.txt"),
                           GETENV_STRING(spoolDir,"QWICS_BATCH_SPOOLDIR","../spool"),
                           GETENV_STRING(workingDir,"QWICS_BATCH_WORKDIR","/tmp"));
    runJobListener(GETENV_STRING(sockFile,"QWICS_READER_SOCKETFILE","../comm/sock.reader"));
    return 0;
}
