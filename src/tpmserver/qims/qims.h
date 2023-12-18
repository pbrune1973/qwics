/*******************************************************************************************/
/*   QWICS Server COBOL Information Management Service Functionality                       */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 30.04.2021                                  */
/*                                                                                         */
/*   Copyright (C) 2021 by Philipp Brune  Email: Philipp.Brune@hs-neu-ulm.de               */
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

#ifndef _qims_h
#define _qims_h

int cbltdli(unsigned char *commArea, int32_t *parcnt, unsigned char *func, unsigned char *pcbMask, 
            unsigned char *ioArea, unsigned char *key1, unsigned char *key2, 
            unsigned char *key3, unsigned char *key4, unsigned char *key5);

#endif
