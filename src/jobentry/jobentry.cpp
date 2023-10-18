/*******************************************************************************************/
/*   QWICS Batch Job Entry System                                                         */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 06.10.2023                                  */
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
#include <sys/ipc.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

#include "spool/SpoolingSystem.h"
#include "dataset/TOC.h"
#include "dataset/Catalog.h"
#include "env/envconf.h"
extern "C" {
#include "../tpmserver/shm/shmtpm.h"

#define SHM_ID_JOBENTRY 4712


void cm(int res) {
    if (res != 0) {
      fprintf(stderr,"%s%d\n","Mutex operation failed: ",res);
    }
}
}

// Global shared memory area for all workers, as desclared in shmtpm.h
int shmId;
key_t shmKey;
void *shmPtr;


using namespace std;


static void sig_handler(int signo) {
    printf("Stopping QWICS jobentry server...\n");
    if (signo == SIGINT) {
    }
    exit(0);
}


void *runCardReader(CardReader *reader) {
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
cout << "accept " << childfd << endl;
      if (childfd == -1) {
        if (errno == EINTR) {
          break;
        }
      }

      if (childfd >= 0) {
          CardReader *reader = new CardReader(childfd,childfd);
          reader->run();
          delete reader;
      } else {
          cout << " ERROR: Unable to accept connection!" << endl;
      }
  cout << "stop " << stop << endl; 
    } while (!stop);

    return NULL;
}


int main(int argc, char **argv) {
    char *datasetDir = NULL;
    char *spoolDir = NULL;
    char *workingDir = NULL;
    char *configFile = NULL;
    char *sockFile = NULL;
    char *volume = GETENV_STRING(datasetDir,"QWICS_DATASET_DIR","../dataset");

    // Init shared memory area
    if ((shmPtr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0)) == (void *) -1) {
      fprintf(stderr, "Failed to attach shared memory segment.\n");
      return -1;
    }

    // Set signal handler for SIGINT (proper shutdown of server)
    struct sigaction a;
    a.sa_handler = sig_handler;
    a.sa_flags = 0;
    sigemptyset( &a.sa_mask );
    sigaction( SIGINT, &a, NULL );

    initSharedMalloc(1);
    TOC::addToc(volume);
    Catalog::create(volume);
    Catalog::defaultVolume = volume;
    SpoolingSystem::create(GETENV_STRING(configFile,"QWICS_BATCH_CONFIG","../config.txt"),
                           GETENV_STRING(spoolDir,"QWICS_BATCH_SPOOLDIR","../spool"),
                           GETENV_STRING(workingDir,"QWICS_BATCH_WORKDIR","/tmp"));
    runJobListener(GETENV_STRING(sockFile,"QWICS_READER_SOCKETFILE","../comm/sock.reader"));
    return 0;
}
