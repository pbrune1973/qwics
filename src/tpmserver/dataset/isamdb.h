/*******************************************************************************************/
/*   QWICS Server Dataset-based ISAM DB (VSAM replacement)                                 */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 19.10.2023                                  */
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

#ifndef _isamdb_h
#define _isamdb_h

#include <stdio.h>
#include <stdlib.h>

#define MODE_SET   0
#define MODE_NEXT  1
#define MODE_PREV  2

// Set up environment
int startIsamDB(char *dir);
int stopIsamDB();

// Dataset access
void* openDataset(char *name);
int removeDataset(char *name, void *txptr);
int put(void *dsptr, void *txptr, void *curptr, unsigned char *rid, int idlen, unsigned char *rec, int lrecl);
int get(void *dsptr, void *txptr, void *curptr, unsigned char *rid, int idlen, unsigned char *rec, int lrecl, int mode);
int del(void *dsptr, void *txptr, unsigned char *rid, int idlen);
int openCursor(void *dsptr, void *txptr, void** curptr, int update, int dirtyread);
int closeCursor(void* curptr);
int closeDataset(void *dsptr);

// Transaction control
void* beginTransaction();
int endTransaction(void *txptr, int commit);

#endif
