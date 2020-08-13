/*******************************************************************************************/
/*   QWICS Batch JCL params library                                                        */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 13.08.2020                                  */
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

#ifndef _libjclpars_h
#define _libjclpars_h

extern char *job;
extern char *step;
extern char *pgm;
extern char jobId[9];

char *getJob();
char *getStep();
char *getPgm();
char *getJobId();

#endif
