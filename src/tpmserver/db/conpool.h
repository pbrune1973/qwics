/*******************************************************************************************/
/*   QWICS Server Database Connection Pool (currently PostgreSQL only)                     */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 03.08.2019                                  */
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

#ifndef _conpool_h
#define _conpool_h

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>
#include <unistd.h>
#include <libpq-fe.h>


// Pool management
void setUpPool(int numCon, char *conInfo, int openCons);
void tearDownPool(int closeCons);

// Pool usage: Used connection always forms one transaction
PGconn *getDBConnection();
int returnDBConnection(PGconn *conn, int commit);
int execSQL(PGconn *conn, char *sql);
PGresult* execSQLQuery(PGconn *conn, char *sql);
char* execSQLCmd(PGconn *conn, char *sql);

#endif
