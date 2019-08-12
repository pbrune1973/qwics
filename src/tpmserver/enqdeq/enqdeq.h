/*******************************************************************************************/
/*   QWICS Server COBOL ENQ/DEQ Synchonisation                                               */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 04.08.2019                                  */
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

#ifndef _enqdeq_h
#define _enqdeq_h

#define MAX_ENQRES 100
#define UOW  0
#define TASK 1

struct taskLock {
    int count;
    int type;
};

struct taskLock *createTaskLocks();
void initEnqResources(int initRes);
int enq(char *resource, int len, int nosuspend, int type, struct taskLock *taskLocks);
int deq(char *resource, int len, int type, struct taskLock *taskLocks);
void releaseLocks(int type, struct taskLock *taskLocks);

#endif
