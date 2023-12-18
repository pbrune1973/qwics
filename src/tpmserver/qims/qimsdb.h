/*******************************************************************************************/
/*   QWICS Server COBOL Information Management Service DB Interface                       */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 26.05.2021                                  */
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

#ifndef _qimsdb_h
#define _qimsdb_h

#define MAX_CURSORS 10

struct seg_info_t {
  char key_name[9];
  char key_type;
  int key_pos;
  int key_len;
  int seg_len;
};

struct seg_area_t {
    char segment[9];
    char sqlCond[1024];
    char cursors[MAX_CURSORS][1024];
    struct seg_info_t segInfo;
};

void initSegArea(struct seg_area_t *seg_area);

int cbltdli_db(int32_t parcnt, char *fn, unsigned char *pcbMask, 
               unsigned char *ioArea, unsigned char *key1, unsigned char *key2, 
               unsigned char *key3, unsigned char *key4, unsigned char *key5);

#endif
