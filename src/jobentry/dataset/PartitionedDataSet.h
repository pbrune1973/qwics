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

#ifndef _PartitionedDataSet_h
#define _PartitionedDataSet_h

#include "DataSet.h"


struct PdsDirEntry {
  unsigned char name[8];
  unsigned char ttr[3];
  unsigned char c;
  unsigned char userData[62];           
};


class PartitionedDataSet : public DataSet {
 private:
  DataSet *dir;
  unsigned char dirRec[256];
  unsigned char dirBuffer[512];
  int dirBufferLen;
  int dirBufferPos;
  long dirRecNr;
  
  int compare(unsigned char *n, unsigned char *m, int len);
  int findEntry(unsigned char *name); 
  int updateEntries();
  
 public:
  PartitionedDataSet(struct TocEntry &entry, int accessMode);
  PartitionedDataSet(DataSet *toc, unsigned long tocPos, int accessMode);
  ~PartitionedDataSet();

  int stow(struct PdsDirEntry *e);
  virtual DataSet *findMember(char *name, struct PdsDirEntry *e);
  DataSet *addMember(char *name, struct PdsDirEntry *e);
  int deleteMember(char *name);
};

#endif
