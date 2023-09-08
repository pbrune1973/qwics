/*******************************************************************************************/
/*   QWICS Batch Job Entry System                                                          */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 08.09.2023                                  */
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

#ifndef _TOCDataSet_h
#define _TOCDataSet_h

#include <stdio.h>
#include <pthread.h>
#include "DataSet.h"

class TOCDataSet : public DataSet {
 private:
  struct DataSetState *stateDataPtr;

 public:
  TOCDataSet(char *path, struct TocEntry &entry, int accessMode);
  ~TOCDataSet();
  virtual int read(unsigned long blockNr, unsigned char *block);
  virtual int write(unsigned long blockNr, unsigned char *block);
};

#endif
