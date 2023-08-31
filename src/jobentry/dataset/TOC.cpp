/*******************************************************************************************/
/*   QWICS Batch Job Entry System                                                          */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 31.08.2023                                  */
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
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "TOC.h"
#include "PartitionedDataSet.h"

using namespace std;


struct TOCVolume TOC::firstTOCVolume;
struct TOCVolume *TOC::lastTOCVolume = &TOC::firstTOCVolume;
char *TOC::workVolume = "TEST01";


TOC *TOC::getToc(char *volume) {
  struct TOCVolume *tv = firstTOCVolume.next;
  
  if (strcmp(volume,workVolume) == 0) volume = workVolume;

  while (tv != NULL) {
    if (strcmp(tv->volume,volume) == 0) {
      return tv->toc;
    }

    tv = tv->next;
  }

  return NULL;
}


TOC *TOC::addToc(char *volume) {
  char path[100];
  struct TOCVolume *tv;

  if (strcmp(volume,workVolume) == 0) volume = workVolume;  
  sprintf(path,"%s",volume);
  if (access(path,F_OK) < 0) {
    return NULL;
  }  
  
  tv = new struct TOCVolume;
  sprintf(tv->volume,"%s",volume);
  tv->toc = new TOC(volume);
  tv->next = NULL;
  
  lastTOCVolume->next = tv;
  lastTOCVolume = tv;
  return tv->toc;
}


TOC::TOC(char *volume) {
  char path[100];
  unsigned long start;
  int i;
  FILE *toc;
  unsigned char memberName[9];
  
  sprintf(this->volume,"%s",volume);
  sprintf(path,"%s/VTOC",volume);
cout << "Path " << path << " " << access(path,F_OK) << endl;
  if (access(path,F_OK) < 0) {
    convertDsn((unsigned char*)volume,newEntry.dsn,memberName);
    sprintf(newEntry.path,"%s",path); 
    newEntry.format = 'F';
    newEntry.recSize = sizeof(struct TocEntry);
    newEntry.blockSize = newEntry.recSize*10;
    newEntry.numOfBlocks = 0;
    newEntry.eodPos = 0;
    newEntry.lastBlockNr = -1;
    newEntry.maxExtends = 16;
    newEntry.numOfExtends = 0;
    start = 0;
    newEntry.extends[0].startPos = start;
    newEntry.extends[0].sizeInBlocks = 100;
    start = start + newEntry.extends[0].sizeInBlocks*newEntry.blockSize;
    for (i = 1; i < newEntry.maxExtends; i++) {
      newEntry.extends[i].startPos = start;
      newEntry.extends[i].sizeInBlocks = 100;
      start = start + newEntry.extends[i].sizeInBlocks*newEntry.blockSize;
    }
    newEntry.nextEntries[0] = 0;
    newEntry.nextEntries[1] = 0;
    newEntry.nextEntries[2] = 0;
    newEntry.nextEntries[3] = 0;
    newEntry.nextEntries[4] = 0;

    tocData = new DataSet(path,newEntry,ACCESS_WRITE | ACCESS_LOCK | ACCESS_EXCL);
    tocData->setWriteImmediate(1);
    tocData->setTOCCreation(1);
    tocData->setTocPos(1);
    convertDsn((unsigned char*)"$ROOT",newEntry.dsn,memberName);
    sprintf(newEntry.path,"%s","$ROOT");
    tocData->point(1);
    tocData->put((unsigned char*)&newEntry);
    tocData->setTOCCreation(0);
  } else {
    tocData = new DataSet(path,newEntry,ACCESS_WRITE | ACCESS_LOCK | ACCESS_EXCL);
    tocData->setWriteImmediate(1);
    tocData->setTocPos(1);
  }

  prevTocPos = 0;
  tocPos = 1;  
}


TOC::~TOC() {
  if (tocData != NULL) delete tocData;
}


void TOC::convertDsn(unsigned char *dsn, unsigned char *dsnOut, unsigned char *memberName) {
  int i,j,n,seg;

  i = 0;
  j = 0;
  seg = 0;
  memberName[0] = 0x00; 

  do {
    if (dsn[i] == '.') {
      dsnOut[seg*9+j] = 0x00;
      seg++;
      j = 0;
      i++;
    } else
    if (dsn[i] == '(') {
      dsnOut[seg*9+j] = 0x00;
      i++;
      n = 0;
      while ((dsn[i] != ')') && (n < 8)) {
        memberName[n] = toupper(dsn[i]);
        i++;
        n++;
      }      
      memberName[n] = 0x00;
      break;
    }

    dsnOut[seg*9+j] = toupper(dsn[i]);
    i++; 
    if (j < 8) j++;    
  } while ((i < 44) && (dsn[i] != 0x00));

  dsnOut[seg*9+j] = 0x00;
}


int TOC::findDsn(unsigned char *dsn, int dsnSeg) {
  char *nameA,*nameB;
  
  do {
    if (tocData->point(tocPos) < 0) return -1;
    if (tocData->get((unsigned char*)&entry) < 0) return -1;

    nameA = (char*)&(dsn[dsnSeg*9]);
    nameB = (char*)&(entry.dsn[dsnSeg*9]);

    if (strcmp(nameA,nameB) == 0) {
      if (dsnSeg < 4) {
        dsnSeg++;
        return findDsn(dsn,dsnSeg);
      } else {
        return 5;
      }
    }

    if (entry.nextEntries[dsnSeg] > 0) {
      prevTocPos = tocPos;
      tocPos = entry.nextEntries[dsnSeg];
    }
  } while (entry.nextEntries[dsnSeg] > 0);  

  return dsnSeg;
}


unsigned long TOC::newEntryPos() {
  unsigned long pos = 1;
  
  if (tocData->point(pos) < 0) return pos;

  while (tocData->get((unsigned char*)&newEntry) >= 0) {
    if (newEntry.dsn[0] == 0x00) return pos;
    pos++;
  }
  
  return pos;
}


DataSet *TOC::allocate(unsigned char *dsn, char format, int recSize, int blockSize, 
                       int spaceType, int size, int extend, int dirSize, int disp, 
                       int openMode) {
  unsigned long newPos,start;
  int i,n,accessMode;
  unsigned char memberName[9];
  struct PdsDirEntry userEntry;

  userEntry.c = 0x00;   
  if (openMode == OPEN_RDWR) 
    accessMode = ACCESS_WRITE;
  else
    accessMode = ACCESS_READ;
  
  tocData->lock();
cout << "allocate " << dsn << endl;
  prevTocPos = 0;
  tocPos = 1;
  convertDsn(dsn,newEntry.dsn,memberName);
  int dsnSeg = findDsn(newEntry.dsn,0);
cout << "dsnSeg " << dsnSeg << endl;

  if (dsnSeg > 4) {
    cout << "selected tocPos " << tocPos << endl;
    if ((disp == DISP_MOD) || (disp == DISP_OLD)) {
      tocData->unlock();
      if (entry.dirSize > 0) {
        if (memberName[0] != 0x00) {
          PartitionedDataSet *pds = new PartitionedDataSet(tocData,tocPos,accessMode | ACCESS_LOCK | ACCESS_EXCL);
          DataSet *mb = pds->findMember((char*)memberName,&userEntry);
          if (mb == NULL) 
            return pds->addMember((char*)memberName,&userEntry);        
          else
            return mb;  
        } else
          return new PartitionedDataSet(tocData,tocPos,accessMode | ACCESS_LOCK | ACCESS_EXCL);       
      } else 
        return new DataSet(tocData,tocPos,accessMode | ACCESS_LOCK | ACCESS_EXCL);
    } else
    if (disp == DISP_SHR) {
      tocData->unlock();
      if (entry.dirSize > 0) {
        if (memberName[0] != 0x00) {
          PartitionedDataSet *pds = new PartitionedDataSet(tocData,tocPos,accessMode | ACCESS_LOCK | ACCESS_SHARED);
          DataSet *mb = pds->findMember((char*)memberName,&userEntry);
          if (mb == NULL) 
            return pds->addMember((char*)memberName,&userEntry);        
          else
            return mb;  
        } else
          return new PartitionedDataSet(tocData,tocPos,accessMode | ACCESS_LOCK | ACCESS_SHARED);       
      } else 
        return new DataSet(tocData,tocPos,accessMode | ACCESS_LOCK | ACCESS_SHARED);
    } else {
      tocData->unlock();
      return NULL;
    }
  }  

  if ((disp != DISP_NEW) && (disp != DISP_MOD)) {
    tocData->unlock();
    return NULL;
  }
  
  switch (spaceType) {
    case SPACETYPE_BLK : break;
    case SPACETYPE_TRK : size = size*56664;
                         extend = extend*56664; 
                         break;
    case SPACETYPE_CYL : size = size*56664*15;
                         extend = extend*56664*15; 
                         break;
    case SPACETYPE_MB :  size = size*1024*1024/blockSize+blockSize;
                         extend= extend*1024*1024/blockSize+blockSize;
                         break;
  }

  newPos = newEntryPos();
  convertDsn(dsn,newEntry.dsn,memberName);
  sprintf(newEntry.path,"%s/",volume); 
  n = strlen(newEntry.path);
  i = 0;
  while ((dsn[i] != 0x00) && (dsn[i] != '(') && (i < 44)) {
    newEntry.path[n+i] = toupper(dsn[i]);
    i++;
  }
  newEntry.path[n+i] = 0x00;  
  newEntry.format = format;
  newEntry.dirSize = dirSize;
  newEntry.recSize = recSize;
  newEntry.blockSize = blockSize;
  newEntry.lastBlockNr = -1;  
  newEntry.numOfBlocks = 0;
  newEntry.eodPos = 0;
  newEntry.maxExtends = 16;
  newEntry.numOfExtends = 0;
  start = 0;
  newEntry.extends[0].startPos = start;
  newEntry.extends[0].sizeInBlocks = size;
  start = start + newEntry.extends[0].sizeInBlocks*newEntry.blockSize;
  for (i = 1; i < newEntry.maxExtends; i++) {
    newEntry.extends[i].startPos = start;
    newEntry.extends[i].sizeInBlocks = extend;
    start = start + newEntry.extends[i].sizeInBlocks*newEntry.blockSize;
  }
  newEntry.nextEntries[0] = 0;
  newEntry.nextEntries[1] = 0;
  newEntry.nextEntries[2] = 0;
  newEntry.nextEntries[3] = 0;
  newEntry.nextEntries[4] = 0;

  if (tocData->point(newPos) < 0) { tocData->unlock(); return NULL; }
  if (tocData->put((unsigned char*)&newEntry) < 0) { tocData->unlock(); return NULL; }
  
  if (dsnSeg >= 0) {
    entry.nextEntries[dsnSeg] = newPos;
cout << "nextEntries for " << tocPos << " "
                << entry.nextEntries[0] << " "
                << entry.nextEntries[1] << " "
                << entry.nextEntries[2] << " "
                << entry.nextEntries[3] << " "
                << entry.nextEntries[4] << endl;
    if (tocData->point(tocPos) < 0) { tocData->unlock(); return NULL; }
    if (tocData->put((unsigned char*)&entry) < 0) { tocData->unlock(); return NULL; }
  }

  cout << "selected tocPos " << newPos << endl;
  tocData->unlock(); 
  if (newEntry.dirSize > 0) {
    if (memberName[0] != 0x00) {
      PartitionedDataSet *pds = new PartitionedDataSet(tocData,newPos,accessMode | ACCESS_LOCK | ACCESS_EXCL);
      return pds->addMember((char*)memberName,&userEntry);
    } else
      return new PartitionedDataSet(tocData,newPos,accessMode | ACCESS_LOCK | ACCESS_EXCL);       
  } else 
      return new DataSet(tocData,newPos,accessMode | ACCESS_LOCK | ACCESS_EXCL);
}


int TOC::remove(unsigned char *dsn) {
  int dsnSeg,replaceSeg,i;
  unsigned long replaceTocPos;  
  unsigned char memberName[9];
  
  tocData->lock();
cout << "remove " << dsn << endl;
  prevTocPos = 1;
  tocPos = 2;
  convertDsn(dsn,deletedEntry.dsn,memberName);
  dsnSeg = findDsn(deletedEntry.dsn,0);
cout << "dsnSeg " << dsnSeg << endl;
 
  if (dsnSeg > 4) {
    if (memberName[0] != 0x00) {
      if (entry.dirSize <= 0) return -1;
      tocData->unlock();
      PartitionedDataSet *pds = new PartitionedDataSet(tocData,tocPos,ACCESS_WRITE | ACCESS_LOCK | ACCESS_EXCL);
      pds->deleteMember((char*)memberName);
      delete pds;
      return 0;
    }

    deletedEntry = entry;

    entry.dsn[0] = 0x00;
    if (tocData->point(tocPos) < 0) { tocData->unlock(); return -1; }
    if (tocData->put((unsigned char*)&entry) < 0) { tocData->unlock(); return -1; }    

    if (tocData->point(prevTocPos) < 0) { tocData->unlock(); return -1; }
    if (tocData->get((unsigned char*)&entry) < 0) { tocData->unlock(); return -1; }    

    for (i = 4; i >= 0; i--) {
      replaceSeg = i;
      replaceTocPos = deletedEntry.nextEntries[i];
      if (replaceTocPos > 0) break;
    }

    for (i = 0; i < 5; i++) {
      if (entry.nextEntries[i] == tocPos) {
        entry.nextEntries[i] = replaceTocPos; 
        break;
      }
    }
    
    if (tocData->point(prevTocPos) < 0) { tocData->unlock(); return -1; }
    if (tocData->put((unsigned char*)&entry) < 0) { tocData->unlock(); return -1; }    

    if (replaceSeg > 0) {
      if (tocData->point(replaceTocPos) < 0) { tocData->unlock(); return -1; }
      if (tocData->get((unsigned char*)&entry) < 0) { tocData->unlock(); return -1; }    

      for (i = replaceSeg-1; i >= 0; i--) {
        entry.nextEntries[i] = deletedEntry.nextEntries[i];
      }

      if (tocData->point(replaceTocPos) < 0) { tocData->unlock(); return -1; }
      if (tocData->put((unsigned char*)&entry) < 0) { tocData->unlock(); return -1; }     
    }
    
    if (unlink(deletedEntry.path) < 0) { tocData->unlock(); return -1; }   
  }  

  tocData->unlock();
  return 0;
}

