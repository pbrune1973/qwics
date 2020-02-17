/*******************************************************************************************/
/*   QWICS Server Main Tcp Connection Handler                                              */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 30.01.20120                                  */
/*                                                                                         */
/*   Copyright (C) 2018,2019 by Philipp Brune  Email: Philipp.Brune@qwics.org              */
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
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

#include "shm/shmtpm.h"
#include "env/envconf.h"
#include "config.h"
#include "cobexec.h"


// Global shared memory area for all workers, as desclared in shmtpm.h
int shmId;
key_t shmKey;
void *shmPtr;


void *handle_client(void *fd) {
  int childfd = *((int*)fd);
  char buf[2048];
  buf[0] = 0x00;

  while(strstr(buf,"quit") == NULL) {
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
      printf("%s\n",buf);
      char *cmd = strstr(buf,"exec");
      if (cmd) {
        char *name = cmd+5;
        execTransaction(name, &childfd, 0);
      } else {
        char *cmd = strstr(buf,"sql");
        if (cmd) {
            char *sql = cmd+4;
            execSql(sql, &childfd);
        } else {
            cmd = strstr(buf,"PROGRAM");
            if (cmd) {
                char *name = cmd+8;
                execInTransaction(name, &childfd, 0);
            }
            cmd = strstr(buf,"CAPROG");
            if (cmd) {
                char *name = cmd+7;
                execInTransaction(name, &childfd, 1);
            }
        }
      }
    }
  }
  _execSql("COMMIT", &childfd, 0);
  close(childfd);
  return NULL;
}


static void sig_handler(int signo)
{
    printf("Stopping QWICS tpmserver...\n");
    if (signo == SIGINT) {
        clearExec(1);
        shmdt(shmPtr);
        shmctl(shmId,IPC_RMID,NULL);
    }
    exit(0);
}


int main(int argc, char **argv) {
  int parentfd; /* parent socket */
  int childfd; /* child socket */
  int portno; /* port to listen on */
  int workers = 1; /* tpmserver processes started in parallel */
  int clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */

  /*
   * check command line arguments
   */
#ifndef _USE_ONLY_PROCESSES_
  if ((argc < 2) || (argc > 3)) {
    fprintf(stderr, "usage: %s <port> [<workers>]\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);
  if (argc == 3) {
    workers = atoi(argv[2]);
  }
  if (workers < 1) {
    workers = 1;
  }

  int pid;
  for (int i = 1; i < workers; i++) {
      pid = fork();
      if (pid == 0) {
        portno = portno + i;
        // Child procs quit loop;
        break;
      }
  }
  printf("%s %d %d\n","Started worker process: pid=",pid,portno);
#else
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);
#endif

  // Init shared memory area
  if ((shmKey = ftok(argv[0], SHM_ID)) == -1) {
      fprintf(stderr, "Failed to make a key with file %s.\n", argv[1]);
      return -1;
  }
  // printf("shm key=%x\n",shmKey);
  if ((shmId = shmget(shmKey, SHM_SIZE, 0777 | IPC_CREAT | IPC_EXCL)) == -1) {
      fprintf(stderr, "Failed to get shared memory segment. errno=%d %d %d %d\n",errno,ENOMEM,EACCES,EEXIST);
      return -1;
  }
  else if ((shmPtr = shmat(shmId, NULL, 0)) == (void *) -1) {
      fprintf(stderr, "Failed to attach shared memory segment.\n");
      return -1;
  }

  /*
   * socket: create the parent socket
   */
  parentfd = socket(AF_INET, SOCK_STREAM, 0);
  if (parentfd < 0) {
    printf("%s\n","ERROR opening socket");
    exit(1);
  }

  /* setsockopt: Handy debugging trick that lets
   * us rerun the server immediately after we kill it;
   * otherwise we have to wait about 20 secs.
   * Eliminates "ERROR on binding: Address already in use" error.
   */
  optval = 1;
  setsockopt(parentfd, SOL_SOCKET, SO_REUSEADDR,
	     (const void *)&optval , sizeof(int));

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));

  /* this is an Internet address */
  serveraddr.sin_family = AF_INET;

  /* let the system figure out our IP address */
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);

  /* this is the port we will listen on */
  serveraddr.sin_port = htons((unsigned short)portno);

  /*
   * bind: associate the parent socket with a port
   */
  if (bind(parentfd, (struct sockaddr *) &serveraddr,
	   sizeof(serveraddr)) < 0) {
    printf("%s\n","ERROR on binding");
    exit(1);
  }

  /*
   * listen: make this socket ready to accept connection requests
   */
  if (listen(parentfd, 5) < 0) {
    printf("%s\n","ERROR on listen");
    exit(1);
  }

  initExec(1);
  clientlen = sizeof(clientaddr);

  // Set signal handler for SIGINT (proper shutdown of server)
  struct sigaction a;
  a.sa_handler = sig_handler;
  a.sa_flags = 0;
  sigemptyset( &a.sa_mask );
  sigaction( SIGINT, &a, NULL );

  while (1) {
    childfd = accept(parentfd, (struct sockaddr *) &clientaddr, (socklen_t*)&clientlen);
    if (childfd == -1) {
      if (errno == EINTR) {
        break;
      }
    }
    if (childfd >= 0) {
#ifndef _USE_ONLY_PROCESSES_
      pthread_t p1;
      pthread_create (&p1, NULL, handle_client, &childfd);
#else
      int pid = fork();
      if (pid == 0) {
        // Child process
        initExec(0);
        handle_client((void*)&childfd);
        clearExec(0);
        shmdt(shmPtr);
        return 0;
      } else {
        close(childfd);
      }
#endif
    }
  }

  clearExec(1);
  shmdt(shmPtr);
  shmctl(shmId,IPC_RMID,NULL);
  return 0;
}
