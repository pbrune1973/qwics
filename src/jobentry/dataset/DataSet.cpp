/*******************************************************************************************/
/*   QWICS Batch Job Entry System                                                          */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 07.09.2023                                  */
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
#include "DataSet.h"
#include "ebcdic.h"

using namespace std;


DataSet::DataSet() {
  // Dummy constructor for full flexibility in subclasses
}


DataSet::DataSet(struct TocEntry &entry, int accessMode) {
  int n,i,mod = 0;
  struct RequestParameters r;

  this->entry = &(stateData.entry);
  this->tocPos = &(stateData.tocPos);
  this->currentRec = &(stateData.currentRec);
  this->currentPos = &(stateData.currentPos);
  this->currentBlock = &(stateData.currentBlock);
  this->varBlockSize = &(stateData.varBlockSize);
  this->eodPos = &(stateData.eodPos);
  this->recOffset = &(stateData.recOffset);

  lockManager = LockManager::getLockManager();
  pthread_mutex_init(&dataSetMutex,NULL);
  type = 'N';
  this->toc = NULL;
  (*this->tocPos) = 0;
  (*this->entry) = entry;
  if ((*this->entry).format == 'U') (*this->entry).recSize = (*this->entry).blockSize;
  if ((*this->entry).format == 'V') {
    (*this->entry).blockSize = (*this->entry).blockSize+4;
    (*this->entry).recSize = (*this->entry).blockSize;
  }
  this->recsInBlock = entry.blockSize / entry.recSize;
  (*this->recOffset) = 0;
  this->accessMode = accessMode;
  this->translationMode = XMODE_RAW;
  this->writeImmediate = 0;
  this->isTOCCreation = 0;
  (*this->currentRec) = -1;
  (*this->currentPos) = -1;
  (*this->currentBlock) = -1;
  (*this->eodPos) = entry.eodPos;
  this->block = new unsigned char[(*this->entry).blockSize];  
  this->emptyBlock = new unsigned char[(*this->entry).blockSize];
  for (n = 0; n < (*this->entry).blockSize; n++) this->emptyBlock[n] = 0x00;

  if (accessMode & ACCESS_WRITE) { 
    if (accessMode & ACCESS_LOCK) {
      if (accessMode & ACCESS_EXCL) 
        lockManager->getLock((*this->entry).path,LOCK_EXCLUSIVE);    
      else
        lockManager->getLock((*this->entry).path,LOCK_SHARED);    
    }
    
    if (access(entry.path,F_OK) < 0) {
      dataFile = fopen(entry.path,"wb+");
      if ((*this->entry).format != 'V') {
        (*eodPos) = 0;
        (*this->entry).eodPos = 0;
      } else {
        (*eodPos) = 4;
        (*this->entry).eodPos = 4;
      }
    } else {
      dataFile = fopen(entry.path,"rb+");   
      if (accessMode & ACCESS_MOD) {
        mod = 1;
      }
    }
  } else {
    if (accessMode & ACCESS_LOCK) {
      if (accessMode & ACCESS_EXCL) 
        lockManager->getLock((*this->entry).path,LOCK_EXCLUSIVE);    
      else
        lockManager->getLock((*this->entry).path,LOCK_SHARED);    
    }
    dataFile = fopen(entry.path,"rb");
  }

  this->eod = new unsigned char[entry.recSize];
  if (this->eod != NULL) {
    for (i = 0; i < (*this->entry).recSize; i++) this->eod[i] = 0x00;
    for (i = 0; i < 8; i++) this->eod[i] = 0xFF;  
  }

  if ((*this->entry).format == 'F') {
    if (mod) {
cout << "eodPos = " << this->entry->eodPos << " " << this->entry->recSize << endl;
      this->point(this->entry->eodPos/this->entry->recSize);
    } else {
      this->point((long)0);
    }
  } else {
    r.mode = RPMODE_DRP | RPMODE_NUL;
    if (mod) {
      r.mode = r.mode | RPMODE_EOD;
    }
    this->point(&r);
  }
}


DataSet::DataSet(DataSet *toc, unsigned long tocPos, int accessMode) {
  int n,i,mod = 0;
  struct RequestParameters r;

  this->entry = &(stateData.entry);
  this->tocPos = &(stateData.tocPos);
  this->currentRec = &(stateData.currentRec);
  this->currentPos = &(stateData.currentPos);
  this->currentBlock = &(stateData.currentBlock);
  this->varBlockSize = &(stateData.varBlockSize);
  this->eodPos = &(stateData.eodPos);
  this->recOffset = &(stateData.recOffset);

  lockManager = LockManager::getLockManager();
  pthread_mutex_init(&dataSetMutex,NULL);
  type = 'N';
  this->toc = toc;
  (*this->tocPos) = tocPos;
  this->toc->lock();
  this->toc->point(tocPos);
  this->toc->get((unsigned char*)entry);
  this->toc->unlock();
  (*this->recOffset) = 0;
  this->accessMode = accessMode;
  this->translationMode = XMODE_RAW;
  this->writeImmediate = 0;
  this->isTOCCreation = 0;
  (*this->currentRec) = -1;
  (*this->currentPos) = -1;
  (*this->currentBlock) = -1;
  (*this->eodPos) = (*entry).eodPos;
  this->block = new unsigned char[(*entry).blockSize];  
  this->emptyBlock = new unsigned char[(*this->entry).blockSize];
  for (n = 0; n < (*this->entry).blockSize; n++) this->emptyBlock[n] = 0x00;
 
  if (accessMode & ACCESS_WRITE) { 
    if (accessMode & ACCESS_LOCK) {
      if (accessMode & ACCESS_EXCL) 
        lockManager->getLock((*this->entry).path,LOCK_EXCLUSIVE);    
      else
        lockManager->getLock((*this->entry).path,LOCK_SHARED);    
    }

    if (access(entry->path,F_OK) < 0) {
      dataFile = fopen(entry->path,"wb+");
      if ((*this->entry).format != 'V') {
        (*eodPos) = 0;
        (*this->entry).eodPos = 0;
      } else {
        (*eodPos) = 4;
        (*this->entry).eodPos = 4;
      }
      if ((*this->entry).format == 'U') (*this->entry).recSize = (*this->entry).blockSize;
      if ((*this->entry).format == 'V') {
        (*this->entry).blockSize = (*this->entry).blockSize+4;
        (*this->entry).recSize = (*this->entry).blockSize;
      }
    } else {
      dataFile = fopen(entry->path,"rb+");    
      if (accessMode & ACCESS_MOD) {
        mod = 1;
      }
    }
  } else {
    if (accessMode & ACCESS_LOCK) {
      if (accessMode & ACCESS_EXCL) 
        lockManager->getLock((*this->entry).path,LOCK_EXCLUSIVE);    
      else
        lockManager->getLock((*this->entry).path,LOCK_SHARED);    
    }
    dataFile = fopen(entry->path,"rb");
  }

cout << entry->path << endl;
cout << entry->format << endl;
cout << entry->recSize << endl;
cout << entry->blockSize << endl;
cout << entry->numOfBlocks << endl;
cout << entry->eodPos << endl;
cout << entry->maxExtends << endl;
cout << entry->numOfExtends << endl;
  this->recsInBlock = entry->blockSize / entry->recSize;
  this->eod = new unsigned char[entry->recSize];
  if (this->eod != NULL) {
    for (i = 0; i < (*this->entry).recSize; i++) this->eod[i] = 0x00;
    for (i = 0; i < 8; i++) this->eod[i] = 0xFF;  
  }

  if ((*this->entry).format == 'F') {
    if (mod) {
      cout << "eodPos = " << this->entry->eodPos << " " << this->entry->recSize << endl;
      this->point(this->entry->eodPos/this->entry->recSize);
    } else {
      this->point((long)0);
    }
  } else {
    r.mode = RPMODE_DRP | RPMODE_NUL;
    if (mod) {
      r.mode = r.mode | RPMODE_EOD;
    }
    this->point(&r);
  }
}


DataSet::DataSet(char *path, struct TocEntry &entry, int accessMode) {
  int n,i;
  struct RequestParameters r;
  
  this->entry = &(stateData.entry);
  this->tocPos = &(stateData.tocPos);
  this->currentRec = &(stateData.currentRec);
  this->currentPos = &(stateData.currentPos);
  this->currentBlock = &(stateData.currentBlock);
  this->varBlockSize = &(stateData.varBlockSize);
  this->eodPos = &(stateData.eodPos);
  this->recOffset = &(stateData.recOffset);

  lockManager = LockManager::getLockManager();
  pthread_mutex_init(&dataSetMutex,NULL);
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
      this->block = new unsigned char[(*this->entry).blockSize];  
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
      this->block = new unsigned char[(*this->entry).blockSize];          
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

    this->block = new unsigned char[(*this->entry).blockSize];          
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


DataSet::~DataSet() {
  struct RequestParameters r;

  if (eod != NULL) {
    if ((*eodPos) >= 0) {
      if (entry->format == 'F') {
        r.mode = RPMODE_DRR | RPMODE_EOD;
        this->point(&r);
        r.mode = RPMODE_LEN;
        r.area = eod;
        r.areaLen = entry->recSize;      
        this->put(&r);  
      } else
      if (entry->format == 'V') {
        r.mode = RPMODE_DRP | RPMODE_EOD;
        this->point(&r);
        r.mode = RPMODE_LEN;
        r.area = eod;
        r.areaLen = 8;      
        this->put(&r);  
      }
    }
    delete eod;
  }    
  if (!writeImmediate) flush();
  if (dataFile != NULL) fclose(dataFile);
  if (block != NULL) delete block;
  if (emptyBlock != NULL) delete emptyBlock;  
  if (accessMode & ACCESS_LOCK) {
    lockManager->releaseLock(entry->path);    
  }
}


void DataSet::lock() {
  pthread_mutex_lock(&dataSetMutex);
}


void DataSet::unlock() {
  pthread_mutex_unlock(&dataSetMutex);
}


void DataSet::setTranslationMode(int translationMode) {
  this->translationMode = translationMode;
}


void DataSet::setWriteImmediate(int writeImmediate) {
  this->writeImmediate = writeImmediate;
}

void DataSet::setTOCCreation(int isTOCCreation) {
  this->isTOCCreation = isTOCCreation;
}

void DataSet::setTocPos(int tocPos) {
  (*this->tocPos) = tocPos;
}

struct TocEntry* DataSet::getEntry() {
  return entry;
}


int DataSet::getRecSize() {
  return entry->recSize;
}


int DataSet::getFormat() {
  return entry->format;
}


int DataSet::isPartitionedDataSet() {
  if (type == 'P') 
    return 1;
  else
    return 0;
}


int DataSet::flush() {
  if ((accessMode & ACCESS_WRITE) && ((*currentBlock) >= 0)) {
    if (write(*currentBlock,block) < 0) return -1;
  }

  return 0;
}


int DataSet::read(unsigned long blockNr, unsigned char *block) {
  if (blockNr < entry->numOfBlocks) {
    fseek(dataFile,blockNr*entry->blockSize,SEEK_SET);
    fread(block,entry->blockSize,1,dataFile);
  
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


int DataSet::write(unsigned long blockNr, unsigned char *block) {
  unsigned long cnt;
  int i,j,n,tocUpdate;
  struct TocEntry currentEntry;
  
  if ((accessMode & ACCESS_WRITE) == 0) return -1;
  tocUpdate = 0;

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
    if (this != toc) {
      toc->lock();
    }
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
      if (toc->point(*tocPos) < 0) { if (this != toc) { toc->unlock(); } return -1; }
  //cout << "Huhu" << endl;
      if (toc->put((unsigned char*)entry) < 0) { if (this != toc) { toc->unlock(); }  return -1; }
    } 
  //cout << "tocUpdate 2 " << tocUpdate << " " << entry->numOfBlocks << " " << entry->numOfExtends << " " << *tocPos << " " << entry->eodPos << endl; 
    
    if (this != toc) { 
      toc->unlock();
    } 
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


int DataSet::point(long recNr) {
  if (recNr < 0) {
    recNr = (*currentRec) + recNr;
    if (recNr < 0) recNr = 0;  
  }
  if (recNr != *currentRec) {
    unsigned long blockNr = recNr / recsInBlock;

    if (blockNr != (*currentBlock)) {
      if (!writeImmediate) {
        if (flush() < 0) {
cout << "point error 1 " << (*currentBlock) << endl;
          return -1;
        }
      }
      
      if (blockNr < entry->numOfBlocks) {
        if (read(blockNr,block) < 0) {
cout << "point error 2 " << entry->numOfBlocks << endl;
          return -1;
        }
      }
      
      (*currentBlock) = blockNr;
    }    

    (*recOffset) = (recNr % recsInBlock) * entry->recSize;
    (*currentRec) = recNr; 
  }

  return 0;
}


int DataSet::point(struct RequestParameters *rpl) {
  long recNr;
  long pos;
  
  if (rpl->mode & RPMODE_DRR) {  
    if (rpl->mode & RPMODE_LRE) {
      recNr = (*currentRec) + rpl->arg;
      if (recNr < 0) recNr = 0;  
    } else   
    if (rpl->mode & RPMODE_LAB) {
      recNr = rpl->arg;
    } else   
    if (rpl->mode & RPMODE_LRD) {
      if ((*eodPos) >= 0) 
        recNr = (*eodPos)/entry->recSize-1;
      else
        recNr = (*currentRec);
    } else   
    if (rpl->mode & RPMODE_EOD) {
      if ((*eodPos) >= 0) 
        recNr = (*eodPos)/entry->recSize;
      else
        recNr = (*currentRec);
    } else   
    if (rpl->mode & RPMODE_NUL) {
      recNr = 0;
    } 

    return this->point(recNr);
  } else
  if (rpl->mode & RPMODE_DRP) {  
    if (rpl->mode & RPMODE_LRE) {
      pos = (*currentPos) + rpl->arg;
      if (pos < 0) pos = 0;  
    } else   
    if (rpl->mode & RPMODE_LAB) {
      pos = rpl->arg;
    } else   
    if (rpl->mode & RPMODE_EOD) {
      if ((*eodPos) >= 0) 
        pos = (*eodPos);
      else
        pos = (*currentPos);
    } else   
    if (rpl->mode & RPMODE_NUL) {
      if (entry->format != 'V') 
        pos = 0;
      else
        pos = 4;
    } 

    if (pos != (*currentPos)) {
      unsigned long blockNr = pos / entry->blockSize;

      if (blockNr != (*currentBlock)) {
        if (!writeImmediate) {
          if (flush() < 0) {
cout << "point error 1 " << (*currentBlock) << endl;
            return -1;
          }
        }

        if (entry->format == 'V') (*varBlockSize) = 4;
      
        if (blockNr < entry->numOfBlocks) {
          if (read(blockNr,block) < 0) {
cout << "point error 2 " << entry->numOfBlocks << endl;
            return -1;
          }
        }
      
        (*currentBlock) = blockNr;
      }    

      (*recOffset) = pos % entry->blockSize;
      (*currentPos) = pos; 
    }
  }

  return 0;
}


int DataSet::put(unsigned char *record) {
  int i;
  unsigned long recNr = (*currentRec);
  
  if ((entry->format != 'F') && (entry->format != 'U')) return -1;
  if ((accessMode & ACCESS_WRITE) == 0) return -1;

  for (i = 0; i < entry->recSize; i++) {
    if (translationMode == XMODE_RAW) 
      block[(*recOffset)+i] = record[i];
    else
      block[(*recOffset)+i] = a2e[record[i]];
  }

  if ((*eodPos) >= 0) {
    if ((recNr+1)*entry->recSize > (*eodPos)) {
      (*eodPos) = (recNr+1)*entry->recSize;
    }
  }
  
  if (writeImmediate) {
    if (flush() < 0) {
cout << "put flush error " << (*currentBlock) << endl;
      return -1;
    }
  }

  if (point(recNr+1) < 0) return -1;
  return 0;  
}


int DataSet::put(struct RequestParameters *rpl) {
  struct RequestParameters r;
  int i,len;
  long recNr,pos;

  if ((accessMode & ACCESS_WRITE) == 0) return -1;
  recNr = (*currentRec);
  pos = (*currentPos);

  if (entry->format != 'V') {
    if (rpl->mode & RPMODE_LEN) {
      len = rpl->areaLen;
      if ((*recOffset)+len > entry->blockSize) len = entry->blockSize-(*recOffset);
    } else {
      len = entry->recSize;
    }

    for (i = 0; i < len; i++) {
      if (translationMode == XMODE_RAW) 
        block[(*recOffset)+i] = rpl->area[i];
      else
        block[(*recOffset)+i] = a2e[rpl->area[i]];
    }
  } else {
    if (strncmp((char*)eod,(char*)&block[(*recOffset)+4],8) == 0) {
      (*varBlockSize) = (*varBlockSize) - 12;
    }

    len = rpl->areaLen+4;
    if ((*recOffset)+len > entry->blockSize) {
      r.arg = entry->blockSize - (*recOffset) + 4;
      r.mode = RPMODE_DRP | RPMODE_LRE;
      if (point(&r) < 0) return -1;
      pos = (*currentPos);
    }

    block[(*recOffset)] = (unsigned char)((len >> 8) & 0xFF);
    block[(*recOffset)+1] = (unsigned char)(len & 0xFF);
    block[(*recOffset)+2] = 0x00;
    block[(*recOffset)+3] = 0x00;

    for (i = 0; i < len-4; i++) {
      if (translationMode == XMODE_RAW) 
        block[(*recOffset)+4+i] = rpl->area[i];
      else
        block[(*recOffset)+4+i] = a2e[rpl->area[i]];
    }     
    
    (*varBlockSize) = (*varBlockSize)+len;
  }

  if ((*eodPos) >= 0) {
    if (rpl->mode & RPMODE_DRR) {
      if ((recNr+1)*entry->recSize > (*eodPos)) {
        (*eodPos) = (recNr+1)*entry->recSize;
      }
    } else
    if (rpl->mode & RPMODE_DRP) {
      if (pos+len > (*eodPos)) {
        (*eodPos) = pos+len;
      }
    }
  }
  
  if (writeImmediate) {
    if (flush() < 0) {
cout << "put flush error " << (*currentBlock) << endl;
      return -1;
    }
  }

  if (rpl->mode & RPMODE_DRR) {
    if (rpl->mode & RPMODE_LRE) 
      rpl->arg = 0;
    else
    if (rpl->mode & RPMODE_LAB) 
      rpl->arg = recNr;

    if (rpl->mode & RPMODE_FWD) 
      rpl->arg = rpl->arg+1;
    else
    if (rpl->mode & RPMODE_BWD) {
      rpl->arg = rpl->arg-1;
      if (rpl->arg < 0) rpl->arg = 0;
    }
  } else
  if (rpl->mode & RPMODE_DRP) {
    if (rpl->mode & RPMODE_LRE) 
      rpl->arg = 0;
    else
    if (rpl->mode & RPMODE_LAB) 
      rpl->arg = pos;

    rpl->arg = rpl->arg+len;
  }

  if (point(rpl) < 0) return -1;
  return 0;  
}


int DataSet::get(unsigned char *record) {
  int i;

  if ((entry->format != 'F') && (entry->format != 'U')) return -1;  
  if (((*eodPos) >= 0) && ((*currentRec)*entry->recSize >= (long)(*eodPos))) {
    return -1;
  }

  if (strncmp((char*)eod,(char*)&block[*recOffset],8) == 0) {
    return -2;
  }
  
  for (i = 0; i < entry->recSize; i++) {
    if (translationMode == XMODE_RAW) 
      record[i] = block[(*recOffset)+i];
    else
      record[i] = e2a[block[(*recOffset)+i]];
  }

  if (point((*currentRec)+1) < 0) return -1;
  return 0;  
}


int DataSet::get(struct RequestParameters *rpl) {
  int i,len;
  unsigned long recNr,pos;
  
  recNr = (*currentRec);
  pos = (*currentPos);
  
  if (entry->format != 'V') {
    if (strncmp((char*)eod,(char*)&block[(*recOffset)],8) == 0) {
      return -2;
    }

    if (rpl->mode & RPMODE_LEN) {
      len = rpl->areaLen;
      if ((*recOffset)+len > entry->blockSize) len = entry->blockSize-(*recOffset);
    } else {
      len = entry->recSize;
    }

    for (i = 0; i < len; i++) {
      if (translationMode == XMODE_RAW) 
        rpl->area[i] = block[(*recOffset)+i];
      else
        rpl->area[i] = e2a[block[(*recOffset)+i]];
    }

    rpl->areaLen = len;
  } else {
    if (strncmp((char*)eod,(char*)&block[(*recOffset)+4],8) == 0) {
      return -2;
    }

    len = (int)block[(*recOffset)];
    len = (len << 8) | (int)block[(*recOffset)+1];

    for (i = 0; i < len-4; i++) {
      if (((rpl->mode & RPMODE_LEN) && (i < rpl->areaLen)) || 
          !(rpl->mode & RPMODE_LEN)) {
        if (translationMode == XMODE_RAW) 
          rpl->area[i] = block[(*recOffset)+4+i];
        else
          rpl->area[i] = e2a[block[(*recOffset)+4+i]];
      }
    }     
    
    rpl->areaLen = len-4;

    if ((*recOffset)+len >= (*varBlockSize)) {
      len = len + entry->blockSize - (*varBlockSize) + 4;
    }
  }
  
  if (rpl->mode & RPMODE_DRR) {
    if (rpl->mode & RPMODE_LRE) 
      rpl->arg = 0;
    else
    if (rpl->mode & RPMODE_LAB) 
      rpl->arg = recNr;

    if (rpl->mode & RPMODE_FWD) 
      rpl->arg = rpl->arg+1;
    else
    if (rpl->mode & RPMODE_BWD) {
      rpl->arg = rpl->arg-1;
      if (rpl->arg < 0) rpl->arg = 0;
    }
  } else
  if (rpl->mode & RPMODE_DRP) {
    if (rpl->mode & RPMODE_LRE) 
      rpl->arg = 0;
    else
    if (rpl->mode & RPMODE_LAB) 
      rpl->arg = pos;

    rpl->arg = rpl->arg+len;
  }

  if (point(rpl) < 0) return -1;
  return 0;  
}
