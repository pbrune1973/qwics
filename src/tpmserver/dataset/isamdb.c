/*******************************************************************************************/
/*   QWICS Server Dataset-based ISAM DB (VSAM replacement)                                 */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 27.07.2023                                  */
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

#include <string.h>
#include <ctype.h>
#include <db.h>

#include "isamdb.h"

DB_ENV *envp = NULL;


int startIsamDB(char *dir) {
	int ret = 0;
	ret = db_env_create(&envp, 0);
	if (ret != 0) {
 		fprintf(stderr, "Error creating environment handle: %s\n",
 				db_strerror(ret));
 		return ret;
 	}
	ret = envp->open(envp, dir, DB_CREATE | DB_INIT_TXN | DB_INIT_LOCK | DB_INIT_LOG | DB_INIT_MPOOL, 0);
	if (ret != 0) {
 		fprintf(stderr, "Error opening environment: %s\n",
 				db_strerror(ret));
 	}
	return ret;
}

int stopIsamDB() {
	int ret = 0;
	if (envp != NULL) {
 		ret = envp->close(envp, 0);
 		if (ret != 0) {
 			fprintf(stderr, "Environment close failed: %s\n",
 					db_strerror(ret));
		}
	} 
	return ret;
}

void* openDataset(char *name) {
	DB *dbp = NULL;
	int ret = 0;
	if ((ret = db_create(&dbp, envp, 0)) != 0) {
		fprintf(stderr, "db_create: %s\n", db_strerror(ret));
        return NULL;
	}
	if ((ret = dbp->open(dbp, NULL, name, NULL, DB_BTREE, DB_CREATE | DB_AUTO_COMMIT, 0644)) != 0) {
		dbp->err(dbp, ret, "%s", name);
		return NULL;
	}
	return (void*)dbp;
}

int put(void *dsptr, void *txptr, void *curptr, unsigned char *rid, int idlen, unsigned char *rec, int lrecl) {
	DB *dbp = (DB*)dsptr;
	DB_TXN *txn = (DB_TXN*)txptr;
	DBC *cur = (DBC*)curptr;
	DBT key, data;
	int ret = 0;

	memset(&key, 0, sizeof(DBT));
 	memset(&data, 0, sizeof(DBT));
 	key.data = rid;
 	key.size = idlen;
 	data.data = rec;
 	data.size = lrecl;

    if (curptr == NULL) {
		ret = dbp->put(dbp, txn, &key, &data, 0);		
	} else {
		ret = cur->put(cur, &key, &data, DB_KEYFIRST);
	}
 	if (ret != 0) {
 		envp->err(envp, ret, "Database put failed.");
 	}
	return ret;
}

int get(void *dsptr, void *txptr, void *curptr, unsigned char *rid, int idlen, unsigned char *rec, int lrecl, int mode) {
	DB *dbp = (DB*)dsptr;
	DB_TXN *txn = (DB_TXN*)txptr;
	DBC *cur = (DBC*)curptr;
	DBT key, data;
	int ret = 0;

	memset(&key, 0, sizeof(DBT));
 	memset(&data, 0, sizeof(DBT));
	if ((rid != NULL) && (mode == MODE_SET)) {
	 	key.data = rid;
 		key.size = idlen;
	}
	if (rec != NULL) {
 		data.data = rec;
 		data.size = lrecl;
	}

    if (curptr == NULL) {
		ret = dbp->get(dbp, txn, &key, &data, 0);
	} else {
		int flags = DB_SET;
		if (mode == MODE_NEXT) {
			flags = DB_NEXT;
		}
		if (mode == MODE_PREV) {
			flags = DB_PREV;
		}
		ret = cur->get(cur, &key, &data, flags);
	}
 	if (ret != 0) {
 		envp->err(envp, ret, "Database put failed.");
 	}
	return ret;
}

int del(void *dsptr, void *txptr, unsigned char *rid, int idlen) {
	DB *dbp = (DB*)dsptr;
	DB_TXN *txn = (DB_TXN*)txptr;
	DBT key;
	int ret = 0;

	memset(&key, 0, sizeof(DBT));
	key.data = rid;
 	key.size = idlen;

	ret = dbp->del(dbp, txn, &key, 0);
	if (ret != 0) {
 		envp->err(envp, ret, "Database delete failed.");
 	}
	return ret;
}

int openCursor(void *dsptr, void *txptr, void** curptr, int update, int dirtyread) {
	DB *dbp = (DB*)dsptr;
	DB_TXN *txn = (DB_TXN*)txptr;
	int ret = 0, flags = 0;
	if (update) {
		flags = flags | DB_WRITECURSOR;
	}
	if (dirtyread) {
		flags = flags | DB_DIRTY_READ;
	}
	ret = dbp->cursor(dbp, txn, (DBC**)curptr, flags);
	if (ret != 0) {
 		envp->err(envp, ret, "Database open cursor failed.");
 	}
	return ret;
}

int closeCursor(void* curptr) {
	DBC *cur = (DBC*)curptr;
	int ret = 0;
	ret = cur->close(cur);
	if (ret != 0) {
 		envp->err(envp, ret, "Database close cursor failed.");
 	}
	return ret;
}

int closeDataset(void *dsptr) {
	DB *dbp = (DB*)dsptr;
	int ret = 0;
	if (dbp != NULL) {
 		ret = dbp->close(dbp, 0);
 		if (ret != 0) {
 			envp->err(envp, ret, "Database close failed.");
 		}
 	}
	return ret;
}

void* beginTransaction() {
	DB_TXN *txn = NULL;
	int ret = 0;
 	ret = envp->txn_begin(envp, NULL, &txn, 0);
	if (ret != 0) {
 		envp->err(envp, ret, "Transaction begin failed.");
		return NULL;
	}
	return txn;
}

int endTransaction(void *txptr, int commit) {
	DB_TXN *txn = (DB_TXN*)txptr;
	int ret = 0;
	if (commit > 0) {
		ret = txn->commit(txn, 0);
		if (ret != 0) {
			envp->err(envp, ret, "Transaction commit failed.");
			ret = txn->abort(txn);
		}
	} else {
		ret = txn->abort(txn);
	}
	if (ret != 0) {
		envp->err(envp, ret, "End transaction failed.");
 	}
	return ret;
}
