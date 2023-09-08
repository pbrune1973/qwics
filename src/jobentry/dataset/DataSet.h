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

#ifndef _DataSet_h
#define _DataSet_h

#include <stdio.h>
#include <pthread.h>
#include "LockManager.h"

#define XMODE_RAW    0
#define XMODE_ASCII  1

#define ACCESS_READ    0
#define ACCESS_WRITE   1
#define ACCESS_LOCK    2
#define ACCESS_SHARED  0
#define ACCESS_EXCL    4
#define ACCESS_MOD     8

#define RPMODE_LRD  0x00000001
#define RPMODE_LRE  0x00000002
#define RPMODE_LAB  0x00000004
#define RPMODE_DRR  0x00000008
#define RPMODE_DRP  0x00000010
#define RPMODE_FWD  0x00000020
#define RPMODE_BWD  0x00000040
#define RPMODE_UPD  0x00000080
#define RPMODE_LEN  0x00000100
#define RPMODE_EOD  0x00000200
#define RPMODE_NUL  0x00000400


struct TocEntry {
  unsigned char dsn[45];
  char path[100];
  char format;
  int dirSize;
  int recSize;
  int blockSize;
  unsigned long numOfBlocks;
  long lastBlockNr;
  long eodPos;

  int maxExtends;
  int numOfExtends;
  
  struct extend {
    unsigned long startPos;
    unsigned long sizeInBlocks;    
  } extends[16];  

  unsigned long nextEntries[5];
};


struct RequestParameters {
  int mode;
  unsigned char *area;
  int areaLen;
  long arg;
};


struct DataSetState {
  struct TocEntry entry;
  unsigned long tocPos;
  long currentRec;
  long currentPos;
  long currentBlock;
  long varBlockSize;
  long eodPos;
  int recOffset;
};


class DataSet {
 protected: 
  struct DataSetState stateData;
  LockManager *lockManager;
  pthread_mutex_t dataSetMutex;
  char type;
  struct TocEntry *entry;
  DataSet *toc;
  unsigned long *tocPos;
  FILE *dataFile;
  long *currentRec;
  long *currentPos;
  long *currentBlock;
  long *varBlockSize;
  long *eodPos;
  int *recOffset;
  unsigned char *eod;
  unsigned char *block;
  unsigned char *emptyBlock;
  int recsInBlock;
  int accessMode;
  int translationMode;
  int writeImmediate;
  int isTOCCreation; 

 public:
  DataSet();
  DataSet(struct TocEntry &entry, int accessMode);
  DataSet(DataSet *toc, unsigned long tocPos, int accessMode);
  DataSet(char *path, struct TocEntry &entry, int accessMode);
  ~DataSet();

  void lock();
  void unlock();
  void setTranslationMode(int translationMode);
  void setWriteImmediate(int writeImmediate);
  void setTOCCreation(int isTOCCreation);
  void setTocPos(int tocPos);

  struct TocEntry* getEntry();
  int getRecSize();
  int getFormat();
  int isPartitionedDataSet();  
  int flush();
  virtual int read(unsigned long blockNr, unsigned char *block);
  virtual int write(unsigned long blockNr, unsigned char *block);
  virtual int point(long recNr);
  virtual int point(struct RequestParameters *rpl);
  virtual int put(unsigned char *record);
  virtual int put(struct RequestParameters *rpl);
  virtual int get(unsigned char *record);
  virtual int get(struct RequestParameters *rpl);
};

#endif
