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

#ifndef _Member_h
#define _Member_h

#include "DataSet.h"
#include "PartitionedDataSet.h"


class Member : public DataSet {
 private: 
  long startBlockNr;
  int autoDeletePDS;
  PartitionedDataSet *pds;
  
 public:
  Member(long startBlockNr, DataSet *toc, unsigned long tocPos, 
         int accessMode, PartitionedDataSet *pds, int isNew);
  ~Member();

  long getStartBlockNr();
  void setAutoDeletePDS(int autoDeletePDS);
  virtual int read(unsigned long blockNr, unsigned char *block);
  virtual int write(unsigned long blockNr, unsigned char *block);
  virtual int point(long recNr);
  virtual int point(struct RequestParameters *rpl);
};

#endif
