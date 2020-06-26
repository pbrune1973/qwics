/*******************************************************************************************/
/*   QWICS Server Database Connection Pool (currently PostgreSQL only)                     */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 26.06.20120                                  */
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

#include <string.h>
#include <ctype.h>

#include "conpool.h"
#include "../shm/shmtpm.h"


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
void setUpPool(int numCon, char *conInfo, int openCons) {
    if (conInfo == NULL) {
        conInfo = "dbname = postgres";
    }

    // pool = malloc(sizeof(struct conRec)*numCon);
    pool = sharedMalloc(10,sizeof(struct conRec)*numCon);
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
    if (openCons == 1) {
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
}


void tearDownPool(int closeCons) {
    // Ensure no transcations are currently processed, wait for completion
    sem_wait(poolAccess);

    if (closeCons == 1) {
      int i;
      for (i = 0; i < poolSize; i++) {
          PQfinish(pool[i].conn);
      }
    }
    //free(pool);
    sharedFree(pool,sizeof(struct conRec)*poolSize);

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
    res = PQexec(conn, "DROP TABLE IF EXISTS qwics_decl");
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
            printf("ERROR: ROLLBAGCK command failed: %s", PQerrorMessage(conn));
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


int checkSQL(PGconn *conn, char *sql) {
    // Filter out Db2 statements invalid in PostgreSQL
    char token[255];
    int pos = 0, i = 0, l = strlen(sql), state = 0;
    PGresult *res;

    do {
        while ((i < l) && (sql[i] == ' ')) {
            i++;
        }
        while ((i < l) && (sql[i] != ' ')) {
            token[pos] = toupper(sql[i]);
            pos++;
            i++;
        }
        token[pos] = 0x00;

        if (state == 1) {
            // Remeber delared cursors in temp table
            res = PQexec(conn, "CREATE TEMP TABLE IF NOT EXISTS qwics_decl(curname text)");
            if (PQresultStatus(res) != PGRES_COMMAND_OK) {
                printf("ERROR: Failure while executing SQL %s:\n %s", 
                        "CREATE TEMP TABLE IF NOT EXISTS qwics_decl(curname text)", PQerrorMessage(conn));
                PQclear(res);
                return 0;
            }
            PQclear(res);

            char q[255];
            sprintf(q,"SELECT curname FROM qwics_decl WHERE curname='%s'",token);
            res = PQexec(conn, q);
            if (PQresultStatus(res) != PGRES_TUPLES_OK) {
                printf("ERROR: Failure while executing SQL %s:\n %s", q, PQerrorMessage(conn));
                PQclear(res);
                return 0;
            }
            int rows = PQntuples(res);
            PQclear(res);
            if (rows > 0) {
                return 1;
            }

            sprintf(q,"INSERT INTO qwics_decl(curname) VALUES ('%s')",token);
            res = PQexec(conn, q);
            if (PQresultStatus(res) != PGRES_COMMAND_OK) {
                printf("ERROR: Failure while executing SQL %s:\n %s", q, PQerrorMessage(conn));
            }
            PQclear(res);

            state++;
        }
        if (state == 0) {
            if (strcmp(token,"OPEN") == 0) {
                return 1;
            }
            if (strcmp(token,"DECLARE") == 0) {
                pos = 0;
                state++;
            } else {
                return 0;
            }
        }
    } while (state < 2);

    return 0;
}


int execSQL(PGconn *conn, char *sql) {
    int ret = 1;
    PGresult *res;
    if (checkSQL(conn,sql) > 0) {
        return ret;
    }
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
    if (checkSQL(conn,sql) > 0) {
        return NULL;
    }
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
    if (checkSQL(conn,sql) > 0) {
        return "IGNORED";
    }
    res = PQexec(conn, sql);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("ERROR: Failure while executing SQL %s:\n %s", sql, PQerrorMessage(conn));
        ret = NULL;
    }
    ret = PQcmdTuples(res);
    PQclear(res);
    return ret;
}
