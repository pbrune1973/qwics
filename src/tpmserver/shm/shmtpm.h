/*******************************************************************************************/
/*   QWICS Server COBOL shared memory handling                                               */
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

#ifndef _shmtpm_h
#define _shmtpm_h

#define SHM_SIZE 500000
#define SHM_ID 4711

#define BLOCKNUM  960
#define BLOCKSIZE 512

// Global shared memory area for all workers
extern int shmId;
extern key_t shmKey;
extern void *shmPtr;

void initSharedMalloc();
void *sharedMalloc(unsigned char id, int size);
int sharedFree(void *ptr, int size);

#endif
