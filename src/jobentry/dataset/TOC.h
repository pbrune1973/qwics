/*******************************************************************************************/
/*   QWICS Batch Job Entry System                                                          */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 18.08.2023                                  */
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

#ifndef _TOC_h
#define _TOC_h

#include "DataSet.h"

#define SPACETYPE_CYL  0
#define SPACETYPE_TRK  1
#define SPACETYPE_BLK  2
#define SPACETYPE_MB   3

#define DISP_NEW  0
#define DISP_MOD  1
#define DISP_OLD  2
#define DISP_SHR  3

#define OPEN_RDONLY 0
#define OPEN_RDWR   1


class TOC;


struct TOCVolume {
  char volume[7];
  TOC *toc;
  struct TOCVolume *next;
};


class TOC {
 private: 
  char volume[100];
  DataSet *tocData;
  unsigned long tocPos;
  unsigned long prevTocPos;
  struct TocEntry entry;
  struct TocEntry newEntry;
  struct TocEntry deletedEntry;  
  
  TOC(char *volume);
  ~TOC();
  void convertDsn(unsigned char *dsn, unsigned char *dsnOut, unsigned char *memberName);
  int findDsn(unsigned char *dsn, int dsnSeg);
  unsigned long newEntryPos();

 public:
  static struct TOCVolume firstTOCVolume,*lastTOCVolume;
  static char *workVolume;

  static TOC *getToc(char *volume);
  static TOC *addToc(char *volume);  
  DataSet *allocate(unsigned char *dsn, char format, int recSize, int blockSize, 
                    int spaceType, int size, int extend, int dirSize, int disp, 
                    int openMode);
  int remove(unsigned char *dsn); 
};

#endif
