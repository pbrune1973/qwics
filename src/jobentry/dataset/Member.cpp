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

#include <iostream>
#include "Member.h"

using namespace std;


Member::Member(long startBlockNr, DataSet *toc, unsigned long tocPos, 
               int accessMode, PartitionedDataSet *pds, int isNew) : 
        DataSet(toc,tocPos,accessMode & ACCESS_WRITE) {
  this->startBlockNr = startBlockNr; 
  this->autoDeletePDS = 1;
  this->pds = pds;

  if (isNew) {
    if (this->entry.format != 'V') {
      eodPos = 0;
      this->entry.eodPos = 0;
    } else {
      eodPos = 4;
      this->entry.eodPos = 4;
    }
  } else {
    eodPos = -1;
  }
cout << "Member " << startBlockNr << " " << tocPos << endl;  
}


Member::~Member() {
  if (autoDeletePDS) delete pds;
}


long Member::getStartBlockNr() {
  return startBlockNr;
}


void Member::setAutoDeletePDS(int autoDeletePDS) {
  this->autoDeletePDS = autoDeletePDS;
}


int Member::read(unsigned long blockNr, unsigned char *block) {
  return DataSet::read(blockNr+startBlockNr,block);
}


int Member::write(unsigned long blockNr, unsigned char *block) {
  return DataSet::write(blockNr+startBlockNr,block);
}


int Member::point(long recNr) {
  if (recNr != currentRec) {
    unsigned long blockNr = recNr / recsInBlock;

    if (blockNr != currentBlock) {
      if (!writeImmediate) {
        if (flush() < 0) {
cout << "point error 1 " << currentBlock << endl;
          return -1;
        }
      }
      
      if (blockNr+startBlockNr < entry.numOfBlocks) {
        if (read(blockNr,block) < 0) {
cout << "point error 2 " << entry.numOfBlocks << endl;
          return -1;
        }
      }
      
      currentBlock = blockNr;
    }    

    recOffset = (recNr % recsInBlock) * entry.recSize;
    currentRec = recNr; 
  }

  return 0;
}


int Member::point(struct RequestParameters *rpl) {
  long recNr;
  long pos;
  
  if (rpl->mode & RPMODE_DRR) {  
    if (rpl->mode & RPMODE_LRE) {
      recNr = currentRec + rpl->arg;
      if (recNr < 0) recNr = 0;  
    } else   
    if (rpl->mode & RPMODE_LAB) {
      recNr = rpl->arg;
    } else   
    if (rpl->mode & RPMODE_LRD) {
      if (eodPos >= 0) 
        recNr = eodPos/entry.recSize-1;
      else
        recNr = currentRec;
    } else   
    if (rpl->mode & RPMODE_EOD) {
      if (eodPos >= 0) 
        recNr = eodPos/entry.recSize;
      else
        recNr = currentRec;
    } else   
    if (rpl->mode & RPMODE_NUL) {
      recNr = 0;
    } 

    return this->point(recNr);
  } else
  if (rpl->mode & RPMODE_DRP) {  
    if (rpl->mode & RPMODE_LRE) {
      pos = currentPos + rpl->arg;
      if (pos < 0) pos = 0;  
    } else   
    if (rpl->mode & RPMODE_LAB) {
      pos = rpl->arg;
    } else   
    if (rpl->mode & RPMODE_EOD) {
      if (eodPos >= 0) 
        pos = eodPos;
      else
        pos = currentPos;
    } else   
    if (rpl->mode & RPMODE_NUL) {
      if (entry.format != 'V') 
        pos = 0;
      else
        pos = 4;
    } 
 
    if (pos != currentPos) {
      unsigned long blockNr = pos / entry.blockSize;

      if (blockNr != currentBlock) {
        if (!writeImmediate) {
          if (flush() < 0) {
cout << "point error 1 " << currentBlock << endl;
            return -1;
          }
        }

        if (entry.format == 'V') varBlockSize = 4;
      
        if (blockNr+startBlockNr < entry.numOfBlocks) {
          if (read(blockNr,block) < 0) {
cout << "point error 2 " << entry.numOfBlocks << endl;
            return -1;
          }
        }
      
        currentBlock = blockNr;
      }    

      recOffset = pos % entry.blockSize;
      currentPos = pos; 
    }
  }

  return 0;
}
