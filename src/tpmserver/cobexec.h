/*******************************************************************************************/
/*   QWICS Server COBOL load module executor                                               */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 29.01.2020                                  */
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

#ifndef _cobexec_h
#define _cobexec_h

// Manage load module executor
void initExec(int initCons);
void clearExec(int initCons);

// Execute COBOL loadmod in transaction
void execTransaction(char *name, void *fd, int setCommArea);

// Exec COBOL module within an existing DB transaction
void execInTransaction(char *name, void *fd, int setCommArea);

// Execute SQL pure instruction
void _execSql(char *sql, void *fd, int sendRes);
#define execSql(sql, fd) _execSql(sql, fd, 1)

#endif
