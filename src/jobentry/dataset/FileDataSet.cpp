/*******************************************************************************************/
/*   QWICS Batch Job Entry System                                                          */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 06.11.2023                                  */
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

#include <iostream>
#include <unistd.h>
#include <cstring>
#include "FileDataSet.h"
#include "ebcdic.h"

using namespace std;


FileDataSet::FileDataSet(char *path, struct TocEntry &entry) : 
    DataSet() {
  this->entry = &(stateData.entry);
  (*this->entry) = entry;
  block = NULL;
  emptyBlock = NULL;
  eod = NULL;
  writeImmediate = 1;

  dataFile = fopen(path,"rb");
}


FileDataSet::~FileDataSet() {
  if (dataFile != NULL) {
    fclose(dataFile);
    dataFile = NULL;
  }
  accessMode = 0;
}


int FileDataSet::get(unsigned char *record) {
    if (fread(record,entry->recSize,1,dataFile) == 1) {
      return 0;
    }
    return -1;
}
