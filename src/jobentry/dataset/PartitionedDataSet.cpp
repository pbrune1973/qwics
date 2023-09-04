/*******************************************************************************************/
/*   QWICS Batch Job Entry System                                                          */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 19.08.2023                                  */
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
#include "ebcdic.h"
#include "PartitionedDataSet.h"
#include "Member.h"

using namespace std;


PartitionedDataSet::PartitionedDataSet(struct TocEntry &entry, int accessMode) : 
                    DataSet(entry,accessMode) {
  this->type = 'P';
  dir = NULL;
}


PartitionedDataSet::PartitionedDataSet(DataSet *toc, unsigned long tocPos, 
                                       int accessMode) : 
                    DataSet(toc,tocPos,accessMode) {
  struct TocEntry dirTocEntry;
  int dirBlocks,i;

  this->type = 'P';

  if (entry->lastBlockNr <= 0) {
    dirBlocks = (entry->dirSize+1)*256/entry->blockSize + 1; 
    this->point((long)0);
    for (i = 0; i < dirBlocks; i++) {
      this->write(i,emptyBlock);
    }
    this->point((long)0);
    this->put(eod);
    this->flush();
  }

  this->setTranslationMode(XMODE_ASCII);  

  dirTocEntry = *entry;
  dirTocEntry.recSize = 256;
  dirTocEntry.blockSize = 256;
  dirTocEntry.format = 'F';
  dirTocEntry.eodPos = entry->dirSize*256;
  dirTocEntry.dirSize = 0;
  dir = new DataSet(dirTocEntry,ACCESS_WRITE);  
  dir->setWriteImmediate(1);
}


PartitionedDataSet::~PartitionedDataSet() {
  if (dir != NULL) delete dir;
}


int PartitionedDataSet::compare(unsigned char *n, unsigned char *m, int len) {
  int i;
  
  for (i = 0; i < len; i++) {
    if (n[i] < m[i]) return -1;
    if (n[i] > m[i]) return 1;    
  }

  return 0;
}


int PartitionedDataSet::findEntry(unsigned char *name) {
  struct PdsDirEntry *e;
  int len = 0,pos = 2,found = 0,r = -1,i;
  
  dir->point((long)0);
  dirRecNr = 0;
  
  while (!found && (dir->get(dirRec) >= 0)) {
    len = (int)dirRec[0]*256+(int)dirRec[1];
    pos = 2;
    
    while (pos < len) {
      e = (struct PdsDirEntry*)&dirRec[pos]; 

      r = compare(name,e->name,8);
      if (r <= 0) {
        found = 1;
        break;
      }      

      pos = pos+12+(int)(e->c & 0x1F)*2;
    }   

    if (!found) dirRecNr++; 
  }

  dirBufferPos = pos-2;
  dirBufferLen = 0;
  for (i = 0; i < len-2; i++) {
    dirBuffer[dirBufferLen] = dirRec[2+i];
    dirBufferLen++;
  }

  return r;    
}


int PartitionedDataSet::updateEntries() {
  struct PdsDirEntry *e;
  int i,len,pos;

  if ((accessMode & ACCESS_WRITE) == 0) return -1;
  if (dir->point(-1) < 0) return -1;
  
  do {
    pos = 0;
    while (pos < dirBufferLen) {
      e = (struct PdsDirEntry*)&dirRec[pos]; 
      len = 12+(int)(e->c & 0x1F)*2;
      
      if (pos+len <= 254) {
        for (i = 0; i < len; i++) {
          dirRec[2+pos] = dirBuffer[pos];
          pos++;
        }         
      } else break;
    }
    
    dirRec[0] = (unsigned char)(((pos+2) >> 8) & 0xFF);            
    dirRec[1] = (unsigned char)((pos+2) & 0xFF);

    if (dirRecNr >= entry->dirSize) return -1;
    if (dir->put(dirRec) < 0) return -1;
    dirRecNr++;
    
    for (i = pos; i < dirBufferLen; i++) {
      dirBuffer[i-pos] = dirBuffer[i];
    }
    dirBufferLen = dirBufferLen-pos;
    
    if (dirBufferLen > 0) {
      if (dir->get(dirRec) >= 0) {
        len = (int)dirRec[0]*256+(int)dirRec[1];

        for (i = 0; i < len-2; i++) {
          dirBuffer[dirBufferLen] = dirRec[2+i];
          dirBufferLen++;
        }

        if (dir->point(-1) < 0) return -1;
      }
    }
  } while (dirBufferLen > 0);

  for (i = 0; i < 8; i++) dirRec[i] = 0xFF;
  for (i = 8; i < 256; i++) dirRec[i] = 0x00;
  if (dir->put(dirRec) < 0) return -1;

  return 0;
}


int PartitionedDataSet::stow(struct PdsDirEntry *e) {
  int i,len = 12+(int)(e->c & 0x1F)*2;
  unsigned char *eb = (unsigned char*)e;
  
  if ((accessMode & ACCESS_WRITE) == 0) return -1;

  if (findEntry(e->name) != 0) {
    for (i = dirBufferLen-1; i >= dirBufferPos; i--) {
      dirBuffer[i+len] = dirBuffer[i];
    }
    dirBufferLen = dirBufferLen + len;
    
    for (i = 0; i < len; i++) {
      dirBuffer[dirBufferPos+i] = eb[i];
    } 
    
    if (updateEntries() < 0) return -1;
  } else {
    return -1;
  }

  return 0;
}


DataSet *PartitionedDataSet::findMember(char *name, struct PdsDirEntry *e) {
  int i,len;
  long blockNr;
  
  len = strlen(name); if (len > 8) len = 8;
  for (i = 0; i < len; i++) {
    if (translationMode == XMODE_RAW) 
      e->name[i] = (unsigned char)name[i];
    else
      e->name[i] = a2e[(unsigned char)name[i]];
  }  
  for (i = len; i < 8; i++) {
    e->name[i] = a2e[(unsigned char)' '];
  }  
  
  if (findEntry(e->name) == 0) {
    *e = *((struct PdsDirEntry*)&dirBuffer[dirBufferPos]);
    blockNr = ((int)e->ttr[0]*256+(int)e->ttr[1])*150+(int)e->ttr[2];
    return new Member(blockNr,toc,*tocPos,accessMode,this,0);    
  } 

  return NULL;
}


DataSet *PartitionedDataSet::addMember(char *name, struct PdsDirEntry *e) {
  int i,len;
  long blockNr,trackNr;
  
  if ((accessMode & ACCESS_WRITE) == 0) return NULL;

  len = strlen(name); if (len > 8) len = 8;
  for (i = 0; i < len; i++) {
    if (translationMode == XMODE_RAW) 
      e->name[i] = (unsigned char)name[i];
    else
      e->name[i] = a2e[(unsigned char)name[i]];
  }  
  for (i = len; i < 8; i++) {
    e->name[i] = a2e[(unsigned char)' '];
  }  

  blockNr = entry->lastBlockNr+1;
  trackNr = (blockNr / 150) & 0x0000FFFF;

  e->ttr[0] = (unsigned char)((trackNr >> 8) & 0xFF);  
  e->ttr[1] = (unsigned char)(trackNr & 0xFF);  
  e->ttr[2] = (unsigned char)((blockNr-trackNr*150) & 0x000000FF);
  e->c = e->c & 0x7F;
  
  if (stow(e) < 0) return NULL;
  return new Member(blockNr,toc,*tocPos,accessMode,this,1);      
}


int PartitionedDataSet::deleteMember(char *name) {
  struct PdsDirEntry *e;
  int i,len;
  unsigned char *eb = (unsigned char*)e;
  unsigned char n[8];
  
  if ((accessMode & ACCESS_WRITE) == 0) return -1;

  len = strlen(name); if (len > 8) len = 8;
  for (i = 0; i < len; i++) {
    if (translationMode == XMODE_RAW) 
      n[i] = (unsigned char)name[i];
    else
      n[i] = a2e[(unsigned char)name[i]];
  }  
  for (i = len; i < 8; i++) {
    n[i] = a2e[(unsigned char)' '];
  }  
  
  if (findEntry(n) != 0) {
    e = (struct PdsDirEntry*)&dirBuffer[dirBufferPos];
    len = 12+(int)(e->c & 0x1F)*2;

    for (i = dirBufferPos+len; i < dirBufferLen; i++) {
      dirBuffer[i-len] = dirBuffer[i];
    }
    dirBufferLen = dirBufferLen - len;
    
    if (updateEntries() < 0) return -1;
  } else {
    return -1;
  }

  return 0;
}
