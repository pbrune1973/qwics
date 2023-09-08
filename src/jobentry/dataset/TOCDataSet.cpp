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

#include <iostream>
#include <unistd.h>
#include <cstring>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "TOCDataSet.h"
#include "ebcdic.h"
extern "C" {
#include "../../tpmserver/shm/shmtpm.h"
}

using namespace std;


TOCDataSet::TOCDataSet(char *path, struct TocEntry &entry, int accessMode) : 
    DataSet() {
  int n,i;
  struct RequestParameters r;
  
  stateDataPtr = (struct DataSetState*)sharedMalloc(11,sizeof(struct DataSetState));
  this->entry = &(stateDataPtr->entry);
  this->tocPos = &(stateDataPtr->tocPos);
  this->currentRec = &(stateDataPtr->currentRec);
  this->currentPos = &(stateDataPtr->currentPos);
  this->currentBlock = &(stateDataPtr->currentBlock);
  this->varBlockSize = &(stateDataPtr->varBlockSize);
  this->eodPos = &(stateDataPtr->eodPos);
  this->recOffset = &(stateDataPtr->recOffset);

  lockManager = LockManager::getLockManager();
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
  pthread_mutex_init(&dataSetMutex,&attr);
  type = 'N';
  this->toc = this;
  (*this->tocPos) = 0;
  (*this->recOffset) = 0;
  this->accessMode = accessMode;
  this->translationMode = XMODE_RAW;
  this->writeImmediate = 0;
  this->isTOCCreation = 0;
  (*this->currentRec) = -1;
  (*this->currentPos) = -1;
  (*this->currentBlock) = -1;

  if (accessMode & ACCESS_WRITE) { 
    if (accessMode & ACCESS_LOCK) {
      if (accessMode & ACCESS_EXCL) 
        lockManager->getLock(path,LOCK_EXCLUSIVE);    
      else
        lockManager->getLock(path,LOCK_SHARED);    
    }

    if (access(path,F_OK) < 0) {
      (*this->entry) = entry;
      if ((*this->entry).format == 'U') (*this->entry).recSize = (*this->entry).blockSize;
      if ((*this->entry).format == 'V') {
        (*this->entry).blockSize = (*this->entry).blockSize+4;
        (*this->entry).recSize = (*this->entry).blockSize;
      }
      if ((*this->entry).format != 'V') {
        (*eodPos) = 0;
        (*this->entry).eodPos = 0;
      } else {
        (*eodPos) = 4;
        (*this->entry).eodPos = 4;
      }
      sprintf((*this->entry).path,"%s",path);
      this->recsInBlock = (*this->entry).blockSize / (*this->entry).recSize;
      this->block = (unsigned char*)sharedMalloc(12,(*this->entry).blockSize);  
      this->emptyBlock = new unsigned char[(*this->entry).blockSize];
      for (n = 0; n < (*this->entry).blockSize; n++) this->emptyBlock[n] = 0x00;

      dataFile = fopen(path,"wb+");
      this->point((long)0);
      this->put((unsigned char*)&((*this->entry)));
    } else {
      dataFile = fopen(path,"rb+");  
      fread(&((*this->entry)),sizeof(struct TocEntry),1,dataFile);
      fseek(dataFile,0,SEEK_SET);
      this->recsInBlock = (*this->entry).blockSize / (*this->entry).recSize;
      this->block = (unsigned char*)sharedMalloc(12,(*this->entry).blockSize);           
      this->emptyBlock = new unsigned char[(*this->entry).blockSize];
      for (n = 0; n < (*this->entry).blockSize; n++) this->emptyBlock[n] = 0x00;
    }
  } else {
    if (accessMode & ACCESS_LOCK) {
      if (accessMode & ACCESS_EXCL) 
        lockManager->getLock(path,LOCK_EXCLUSIVE);    
      else
        lockManager->getLock(path,LOCK_SHARED);    
    }

    dataFile = fopen(path,"rb");
    fread(&((*this->entry)),sizeof(struct TocEntry),1,dataFile);
    fseek(dataFile,0,SEEK_SET);

    this->block = (unsigned char*)sharedMalloc(12,(*this->entry).blockSize);          
    this->emptyBlock = new unsigned char[(*this->entry).blockSize];
    for (n = 0; n < (*this->entry).blockSize; n++) this->emptyBlock[n] = 0x00;
  }
cout << (*this->entry).path << endl;
cout << (*this->entry).format << endl;
cout << (*this->entry).recSize << endl;
cout << (*this->entry).blockSize << endl;
cout << (*this->entry).numOfBlocks << endl;
cout << (*this->entry).eodPos << endl;
cout << (*this->entry).maxExtends << endl;
cout << (*this->entry).numOfExtends << endl;
  this->eod = new unsigned char[entry.recSize];
  if (this->eod != NULL) {
    for (i = 0; i < (*this->entry).recSize; i++) this->eod[i] = 0x00;
    for (i = 0; i < 8; i++) this->eod[i] = 0xFF;  
  }
  (*this->eodPos) = (*this->entry).eodPos;

  if ((*this->entry).format == 'F') {
    this->point((long)0);
  } else {
    r.mode = RPMODE_DRP | RPMODE_NUL;
    this->point(&r);
  }
}


TOCDataSet::~TOCDataSet() {
  if (block != NULL) {
    sharedFree(block,(*this->entry).blockSize);    
  }
  sharedFree(stateDataPtr,sizeof(struct DataSetState));
}


int TOCDataSet::read(unsigned long blockNr, unsigned char *block) {
  if (blockNr < entry->numOfBlocks) {
    fseek(dataFile,blockNr*entry->blockSize,SEEK_SET);
    fread(block,entry->blockSize,1,dataFile);
  
    if ((*this->entry).blockSize == (*this->entry).recSize) {
      // recNr 1 is in second block
      if (blockNr == 1) {
        memcpy(this->entry,block,sizeof(struct TocEntry));
      }
    } else {
      // blockSize > recSize, recNr 1 is in first block
      if (blockNr == 0) {
        memcpy(this->entry,&block[(*this->entry).recSize],sizeof(struct TocEntry));
      }
    }

    if (ferror(dataFile)) {
      return -1;
    } 

    if (entry->format == 'V') {
      (*varBlockSize) = (long)block[0];
      (*varBlockSize) = ((*varBlockSize) << 8) | (long)block[1];
      if ((*varBlockSize) < 4) (*varBlockSize) = 4;
    }
  } else {
    return -1;
  }
  
  return 0;
}


int TOCDataSet::write(unsigned long blockNr, unsigned char *block) {
  unsigned long cnt;
  int i,j,n,tocUpdate;
  struct TocEntry currentEntry;
  
  if ((accessMode & ACCESS_WRITE) == 0) return -1;
  tocUpdate = 0;

  if ((*this->entry).blockSize == (*this->entry).recSize) {
    // recNr 1 is in second block
    if (blockNr == 1) {
      memcpy(block,this->entry,sizeof(struct TocEntry));
    }
  } else {
    // blockSize > recSize, recNr 1 is in first block
    if (blockNr == 0) {
      memcpy(&block[(*this->entry).recSize],this->entry,sizeof(struct TocEntry));
    }
  }

  if ((long)blockNr > entry->lastBlockNr) {
    entry->lastBlockNr = blockNr;
    tocUpdate = 1; 
  }
  
  if (blockNr >= entry->numOfBlocks) {
    cnt = entry->numOfBlocks;
    
    for (i = entry->numOfExtends; i < entry->maxExtends; i++) {
      cnt = cnt + entry->extends[i].sizeInBlocks;
      if (blockNr < cnt) break;
    }   
cout << "extend " << i << " " << entry->maxExtends << " " << entry->numOfExtends << endl;
    if (i < entry->maxExtends) {
      i++;

      if (i > entry->numOfExtends) {
        fseek(dataFile,entry->extends[entry->numOfExtends].startPos,SEEK_SET);
        for (j = entry->numOfExtends; j < i ; j++) {
cout << "extend " << j << " " << entry->extends[j].sizeInBlocks << " " << entry->blockSize << endl;
          for (n = 0; n < entry->extends[j].sizeInBlocks; n++) {
            fwrite(emptyBlock,entry->blockSize,1,dataFile);
          }
        } 
      
        entry->numOfExtends = i;
        entry->numOfBlocks = cnt;
        tocUpdate = 1;
      }
    } else {
      cout << "write error 1" << endl;
      return -1;
    }
  }

  if ((toc != NULL) && !isTOCCreation) {
    currentEntry = *entry;
    toc->point(*tocPos);
    toc->get((unsigned char*)&currentEntry);
  
    if (currentEntry.lastBlockNr > entry->lastBlockNr) {
      entry->lastBlockNr = currentEntry.lastBlockNr;
    }

    if (currentEntry.numOfExtends > entry->numOfExtends) {
      entry->numOfExtends = currentEntry.numOfExtends;
    }

    if (currentEntry.numOfBlocks > entry->numOfBlocks) {
      entry->numOfBlocks = currentEntry.numOfBlocks;
    }

    if ((*eodPos) >= 0) {
      if (currentEntry.eodPos > (*eodPos)) {
        entry->eodPos = currentEntry.eodPos; 
        (*eodPos) = entry->eodPos;
      } else {
        if ((*eodPos) > entry->eodPos) {
          entry->eodPos = (*eodPos);
          tocUpdate = 1;
        }
      }
    }
    
  //cout << "tocUpdate " << tocUpdate << " " << entry->numOfBlocks << " " << entry->numOfExtends << " " << *tocPos << " " << entry->eodPos << endl; 
    if (tocUpdate) {
      if (toc->point(*tocPos) < 0) { return -1; }
  //cout << "Huhu" << endl;
      if (toc->put((unsigned char*)entry) < 0) { return -1; }
    } 
  //cout << "tocUpdate 2 " << tocUpdate << " " << entry->numOfBlocks << " " << entry->numOfExtends << " " << *tocPos << " " << entry->eodPos << endl;     
  }

  if (entry->format == 'V') {
    block[0] = (unsigned char)(((*varBlockSize) >> 8) & 0xFF);
    block[1] = (unsigned char)((*varBlockSize) & 0xFF);
    block[2] = 0x00;
    block[3] = 0x00;
  }

  fseek(dataFile,blockNr*entry->blockSize,SEEK_SET);
  fwrite(block,entry->blockSize,1,dataFile);
  fflush(dataFile);
  
  if (ferror(dataFile)) {
    cout << "write error 2" << endl;
    return -1;
  }

  return 0;
}

