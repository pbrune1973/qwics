/*******************************************************************************************/
/*   QWICS Batch Job Entry System                                                          */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 18.08.2023                                  */
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
#include "../tpmserver/env/envconf.h"

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
    if (bind(parentfd, (struct sockaddr *) &serversockaddr,sizeof(serversockaddr)) < 0) {
      printf("%s\n","ERROR on binding");
      exit(1);
    }

    int sa,sb,i,n = sizeof(sadr),stop = 0;  

    sa = socket(AF_INET,SOCK_STREAM,0);

    if (bind(sa,(struct sockaddr*)&sadr,n) >= 0) {
       listen(sa,10);

       do {
        sb = accept(parentfd, (struct sockaddr *) &serversockaddr, (socklen_t*)&clientlen);

        if (sb >= 0) {
           CardReader *reader = new CardReader(sb,sb);
           runCardReader((void*)reader);
        } else {
           cout << " ERROR: Unable to accept connection!" << endl;              
        }
       } while (!stop);              
    } else
       cout << " ERROR: Unable to bind socket!" << endl;

    close(sa);
}


int main(int argc, char *argv[]) {
  if (argc > 5) {
    TOC::addToc("TEST01");
    SpoolingSystem::create(argv[1],argv[2],argv[3]);
    runJobListener("");
  } else {
    cout << "Missing parameters\n";
  }
}