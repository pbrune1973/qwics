/*******************************************************************************************/
/*   QWICS Server Database Connection Pool (currently PostgreSQL only)                     */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 12.03.2018                                  */
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

#include "conpool.h"


void safeExit(PGconn *conn)
{
    PQfinish(conn);
    exit(1);
}


// Pool datacstructure;
struct conRec {
    PGconn *conn;
    int used;
} *pool;

int poolSize = 0;

sem_t *poolAccess;


// Pool management
void setUpPool(int numCon, char *conInfo) {
    if (conInfo == NULL) {
        conInfo = "dbname = postgres";
    }
    
    pool = malloc(sizeof(struct conRec)*numCon);
    if (pool == NULL) {
        printf("%s%d%s\n","ERROR: Could not allocate connection pool with ",numCon," connections!");
        exit(1);
    }
    poolSize = numCon;
    
    sem_unlink("pool");
    poolAccess = sem_open("pool", O_CREAT, 0600, 1);
    if(poolAccess == SEM_FAILED) {
        printf("%s\n","ERROR: Could not open pool access semaphore");
        exit(1);
    }
    
    // Open connections
    int i;
    for (i = 0; i < poolSize; i++) {
        pool[i].used = 0;
        pool[i].conn = PQconnectdb(conInfo);
        
        if (PQstatus(pool[i].conn) != CONNECTION_OK) {
            printf("ERROR: Connection to database failed: %s",
                    PQerrorMessage(pool[i].conn));
            safeExit(pool[i].conn);
        }
    }
}


void tearDownPool() {
    // Ensure no transcations are currently processed, wait for completion
    sem_wait(poolAccess);
    
    int i;
    for (i = 0; i < poolSize; i++) {
        PQfinish(pool[i].conn);
    }
    free(pool);
    
    sem_post(poolAccess);
    sem_close(poolAccess);
    sem_unlink("pool");
}


// Pool usage: Used connection always forms one transaction
PGconn *getDBConnection() {
    PGconn *conn = NULL;
    while (conn == NULL) {
        sem_wait(poolAccess);
        int i;
        for (i = 0; i < poolSize; i++) {
            if (pool[i].used == 0) {
                pool[i].used = 1;
                conn = pool[i].conn;
                break;
            }
        }
        sem_post(poolAccess);
        if (conn == NULL) {
          sleep(5);
        }
    }

    PGresult *res;
    res = PQexec(conn, "START TRANSACTION ISOLATION LEVEL SERIALIZABLE READ WRITE");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("ERROR: START TRANSACTION failed: %s", PQerrorMessage(conn));
    }
    PQclear(res);
    return conn;
}


int returnDBConnection(PGconn *conn, int commit) {
    int ret = 1;
    PGresult *res;
    
    if (commit) {
        res = PQexec(conn, "COMMIT");
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            ret = 0;
            printf("ERROR: COMMIT command failed: %s", PQerrorMessage(conn));
            PQclear(res);
            res = PQexec(conn, "ROLLBACK");
            if (PQresultStatus(res) != PGRES_COMMAND_OK) {
                printf("ERROR: ROLLBACK command failed: %s", PQerrorMessage(conn));
            }
        }
    } else {
        res = PQexec(conn, "ROLLBACK");
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            printf("ERROR: ROLLBACK command failed: %s", PQerrorMessage(conn));
            ret = 0;
        }
    }
    PQclear(res);

    sem_wait(poolAccess);
    int i;
    for (i = 0; i < poolSize; i++) {
        if (pool[i].conn == conn) {
            pool[i].used = 0;
        }
    }
    sem_post(poolAccess);
    return ret;
}


int execSQL(PGconn *conn, char *sql) {
    int ret = 1;
    PGresult *res;
    res = PQexec(conn, sql);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("ERROR: Failure while executing SQL %s:\n %s", sql, PQerrorMessage(conn));
        ret = 0;
    }
    PQclear(res);
    return ret;
}


PGresult* execSQLQuery(PGconn *conn, char *sql) {
    PGresult *res;
    res = PQexec(conn, sql);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        printf("ERROR: Failure while executing SQL %s:\n %s", sql, PQerrorMessage(conn));
        PQclear(res);
        return NULL;
    }
    return res;
}


char* execSQLCmd(PGconn *conn, char *sql) {
    char *ret = NULL;
    PGresult *res;
    res = PQexec(conn, sql);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("ERROR: Failure while executing SQL %s:\n %s", sql, PQerrorMessage(conn));
        ret = NULL;
    }
    ret = PQcmdTuples(res);
    PQclear(res);
    return ret;
}
