/*******************************************************************************************/
/*   QWICS Database Connection Handler (currently PostgreSQL only)                         */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 05.09.2020                                  */
/*                                                                                         */
/*   Copyright (C) 2020by Philipp Brune  Email: Philipp.Brune@hs-neu-ulm.de                */
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

#include "db.h"


void safeExit(PGconn *conn) {
    PQfinish(conn);
    exit(1);
}


// Pool usage: Used connection always forms one transaction
PGconn *getDBConnection(char *conInfo) {
    if (conInfo == NULL) {
        conInfo = "dbname = postgres";
    }
    PGconn *conn = PQconnectdb(conInfo);

    if (PQstatus(conn) != CONNECTION_OK) {
        printf("ERROR: Connection to database failed: %s",
                PQerrorMessage(conn));
        safeExit(conn);
    }

    PGresult *res;
    res = PQexec(conn, "START TRANSACTION ISOLATION LEVEL READ COMMITTED READ WRITE");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("ERROR: START TRANSACTION failed: %s", PQerrorMessage(conn));
    }
    PQclear(res);
    res = PQexec(conn, "DROP TABLE IF EXISTS qwics_decl");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("ERROR: START TRANSACTION failed: %s", PQerrorMessage(conn));
    }
    PQclear(res);

    // Remember delared cursors in temp table
    res = PQexec(conn, "CREATE TEMP TABLE IF NOT EXISTS qwics_decl(curname text, curhold bool default false)");
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("ERROR: Failure while executing SQL %s:\n %s", 
               "CREATE TEMP TABLE IF NOT EXISTS qwics_decl(curname text, curhold bool default false)", 
               PQerrorMessage(conn));
        PQclear(res);
        return 0;
    }
    PQclear(res);

    return conn;
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

        if (state == 3) {
            char q[255];
            sprintf(q,"DELETE FROM qwics_decl WHERE curname='%s'",token);
            res = PQexec(conn, q);
            if (PQresultStatus(res) != PGRES_COMMAND_OK) {
                printf("ERROR: Failure while executing SQL %s:\n %s", q, PQerrorMessage(conn));
                PQclear(res);
                return 0;
            }

            state++;
        }
        if (state == 1) {
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

            if (strstr(sql,"WITH HOLD") != NULL) {
               sprintf(q,"UPDATE qwics_decl SET curhold=true WHERE curname='%s'",token);
               res = PQexec(conn, q);
               if (PQresultStatus(res) != PGRES_COMMAND_OK) {
                   printf("ERROR: Failure while executing SQL %s:\n %s", q, PQerrorMessage(conn));
               }
               PQclear(res);
            }

            state++;
        }
        if (state == 0) {
            if (strcmp(token,"OPEN") == 0) {
                return 1;
            }
            if (strcmp(token,"CLOSE") == 0) {
                pos = 0;
                state = 3;
                continue;
            }
            if (strcmp(token,"DECLARE") == 0) {
                pos = 0;
                state++;
            } else {
                return 0;
            }
        }
    } while ((state < 2) || (state == 3));

    // Filter out Db2 WITh UR clause, as PosgreSQL does not support it, nor read uncommitted at all
    char *clause = strstr(sql,"WITH UR");
    if (clause != NULL) {
        memset(clause,' ',7);
    }

    return 0;
}


int execSQL(PGconn *conn, char *sql) {
    int ret = 1;
    PGresult *res;
    if (checkSQL(conn,sql) > 0) {
        return ret;
    }

    if (strstr(sql,"COMMIT") || strstr(sql,"ROLLBACK")) {
       res = PQexec(conn, "DELETE FROM qwics_decl WHERE curhold=false");
       if (PQresultStatus(res) != PGRES_COMMAND_OK) {
           printf("ERROR: SYNC connection problem: %s", PQerrorMessage(conn));
       }
       PQclear(res);
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
    } else {
        ret = PQcmdTuples(res);
    }
    PQclear(res);
    return ret;
}
