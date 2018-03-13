/*******************************************************************************************/
/*   QWICS Server COBOL load module executor                                               */
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

#ifndef _cobexec_h
#define _cobexec_h

// Manage load module executor
void initExec();
void clearExec();

// Execute COBOL loadmod in transaction
void execTransaction(char *name, void *fd);

// Exec COBOL module within an existing DB transaction
void execInTransaction(char *name, void *fd);

// Execute SQL pure instruction
void execSql(char *sql, void *fd);

#endif
