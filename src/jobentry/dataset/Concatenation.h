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

#ifndef _Concatenation_h
#define _Concatenation_h

#include "PartitionedDataSet.h"

struct ConcatDS {
  DataSet *dataSet;
  struct ConcatDS *next;
};


class Concatenation : public PartitionedDataSet {
 private: 
  struct ConcatDS firstDS,*lastDS,*currentDS; 

  void deleteDS(ConcatDS *ds);
 
 public:
  Concatenation(DataSet *firstDataSet);
  virtual ~Concatenation();

  int addDataSet(DataSet *newDataSet);
  virtual int point(long recNr);
  virtual int point(struct RequestParameters *rpl);
  virtual int get(unsigned char *record);
  virtual int get(struct RequestParameters *rpl);
  virtual DataSet *findMember(char *name, struct PdsDirEntry *e);
};

#endif
