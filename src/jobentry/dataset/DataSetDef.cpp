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

#include "stdlib.h"
#include "DataSetDef.h"
#include "Concatenation.h"


DataSetDef::DataSetDef(Parameters *ddParams) {
  char *val;
  Parameters *pval;
  
  next = NULL;
  dataSet = NULL;
  c = Catalog::masterCatalog;
  defType = DEFTYPE_NONE;
  
  val = ddParams->getValue(0);
  if ((val != NULL) && (strcmp(val,"DUMMY") == 0)) {
    defType = DEFTYPE_DUMMY;
  } else {
    val = ddParams->getValue("TERM",0); 
    if (val != NULL) {
      defType = DEFTYPE_TERM;
      sprintf(dsn,"%s",val);
    } else {
      val = ddParams->getValue("DSN",0); 
      if (val != NULL) {
        defType = DEFTYPE_DSN;
        sprintf(dsn,"%s",val);
      }
    }
  }

  if (defType == DEFTYPE_DSN) {
    // Evaluate VOL parameter
    pval = ddParams->getPValue("VOL",0);
    if (pval == NULL) {
      pval = ddParams->getPValue("VOLUME",0);
    }

    volume[0] = 0x00;
    if (pval != NULL) {
      val = pval->getValue("SER",0);   
      if ((val != NULL) && (strlen(val) <= 6)) {
        sprintf(volume,"%s",val);
      }
    }
    // Ignore VOL=SER
    sprintf(volume,"%s",Catalog::defaultVolume); 

    // Evaluate DCB parameters
    pval = ddParams->getPValue("DCB",0);
    if (pval == NULL) {
      pval = ddParams;
    }

    val = pval->getValue("RECFM",0);
    if (val != NULL) {
      if ((strcmp(val,"F") == 0) ||
          (strcmp(val,"FB") == 0)) {
        format = 'F';
      } else
      if ((strcmp(val,"V") == 0) ||
          (strcmp(val,"VB") == 0) || 
          (strcmp(val,"VS") == 0) ||    
          (strcmp(val,"VBS") == 0)) {
        format = 'V';
      } else
      if (strcmp(val,"U") == 0) {
        format = 'U';
      } else
        format = 'F';
    } else {
      format = 'F';
    }

    if (format == 'F') {
      val = pval->getValue("LRECL",0);
      if ((val != NULL) && isNumber(val)) {
        recSize = atoi(val);
      } else {
        recSize = 80;     
      }
    } else {
      recSize = 80;
    }

    val = pval->getValue("BLKSIZE",0);
    if ((val != NULL) && isNumber(val)) {
      blockSize = atoi(val);
    } else {
      blockSize = recSize*100;     
    }
    
    // Evaluate SPACE parameters
    spaceType = SPACETYPE_TRK;
    size = 100;
    extend = 100;
    dirSize = 0;
    
    pval = ddParams->getPValue("SPACE",0);
    if (pval != NULL) {
      val = pval->getValue(0);
      if (val != NULL) {      
        if (strcmp(val,"BLK") == 0) spaceType = SPACETYPE_BLK;
        if (strcmp(val,"TRK") == 0) spaceType = SPACETYPE_TRK;
        if (strcmp(val,"CYL") == 0) spaceType = SPACETYPE_CYL;
      }

      pval = pval->getPValue(1);
      if (pval != NULL) {      
        val = pval->getValue(0);
        if ((val != NULL) && isNumber(val)) {      
          size = atoi(val);
        }

        val = pval->getValue(1);
        if ((val != NULL) && isNumber(val)) {      
          extend = atoi(val);
        }

        val = pval->getValue(2);
        if ((val != NULL) && isNumber(val)) {      
          dirSize = atoi(val);
        }
      }
    }
        
    // Evaluate DISP parameters
    pval = ddParams->getPValue("DISP",0);
    if (pval != NULL) {
      val = pval->getValue(0);
    } else {
      val = ddParams->getValue("DISP",0);
    }

    disp = DISP_SHR;
    if (val != NULL) {
      if (strcmp(val,"NEW") == 0) disp = DISP_NEW;
      if (strcmp(val,"MOD") == 0) disp = DISP_MOD;
      if (strcmp(val,"OLD") == 0) disp = DISP_OLD;
      if (strcmp(val,"SHR") == 0) disp = DISP_SHR;
    } 

    cleanupDisp = DISP_PASS;    
    errorDisp = DISP_PASS;
    if (pval != NULL) {
      val = pval->getValue(1);
      if (val != NULL) {
        if (strcmp(val,"CATLG") == 0) cleanupDisp = DISP_CATLG;
        if (strcmp(val,"DELETE") == 0) cleanupDisp = DISP_DELETE;
        if (strcmp(val,"PASS") == 0) cleanupDisp = DISP_PASS;
      }

      val = pval->getValue(2);
      if (val != NULL) {
        if (strcmp(val,"DELETE") == 0) errorDisp = DISP_DELETE;
        if (strcmp(val,"PASS") == 0) errorDisp = DISP_PASS;
      }
    }

    // Evaluate LABEL parameters
    modeMask = ACCESS_LOCK | ACCESS_EXCL | ACCESS_WRITE;
    pval = ddParams->getPValue("LABEL",0);
    if (pval != NULL) {
      val = pval->getValue(3);
      if ((val != NULL) && (strcmp(val,"IN") == 0)) {
        modeMask = ACCESS_LOCK | ACCESS_EXCL;
      }
    }
    
    if (volume[0] == 0x00) {
      char *v = c->getVolume(dsn);
      if (v != NULL) {
        sprintf(volume,"%s",v);
        toc = TOC::getToc(volume);
      } else {
        toc = NULL;
      }
    } else {
      toc = TOC::getToc(volume);
    }
  }
}


DataSetDef::DataSetDef(char *fileName, Parameters *ddParams) {
  char *val;
  Parameters *pval;
  unsigned long start;
  int i;
  
  next = NULL;
  c = Catalog::masterCatalog;
  defType = DEFTYPE_FILE;
  
  volume[0] = 0x00;

  // Evaluate DCB parameters
  pval = ddParams->getPValue("DCB",0);
  if (pval == NULL) {
    pval = ddParams;
  }

  val = pval->getValue("RECFM",0);
  if (val != NULL) {
    if ((strcmp(val,"F") == 0) ||
        (strcmp(val,"FB") == 0)) {
      format = 'F';
    } else
    if ((strcmp(val,"V") == 0) ||
        (strcmp(val,"VB") == 0) || 
        (strcmp(val,"VS") == 0) ||    
        (strcmp(val,"VBS") == 0)) {
      format = 'V';
    } else
    if (strcmp(val,"U") == 0) {
      format = 'U';
    } else
      format = 'F';
  } else {
    format = 'F';
  }

  if (format == 'F') {
    val = pval->getValue("LRECL",0);
    if ((val != NULL) && isNumber(val)) {
      recSize = atoi(val);
    } else {
      recSize = 80;     
    }
  } else {
    recSize = 80;
  }

  val = pval->getValue("BLKSIZE",0);
  if ((val != NULL) && isNumber(val)) {
    blockSize = atoi(val);
  } else {
    blockSize = recSize*100;     
  }
    
  // Evaluate SPACE parameters
  spaceType = SPACETYPE_TRK;
  size = 100;
  extend = 100;
  dirSize = 0;
    
  pval = ddParams->getPValue("SPACE",0);
  if (pval != NULL) {
    val = pval->getValue(0);
    if (val != NULL) {      
      if (strcmp(val,"BLK") == 0) spaceType = SPACETYPE_BLK;
      if (strcmp(val,"TRK") == 0) spaceType = SPACETYPE_TRK;
      if (strcmp(val,"CYL") == 0) spaceType = SPACETYPE_CYL;
    }

    pval = pval->getPValue(1);
    if (pval != NULL) {      
      val = pval->getValue(0);
      if ((val != NULL) && isNumber(val)) {      
        size = atoi(val);
      }

      val = pval->getValue(1);
      if ((val != NULL) && isNumber(val)) {      
        extend = atoi(val);
      }

      val = pval->getValue(2);
      if ((val != NULL) && isNumber(val)) {      
        dirSize = atoi(val);
      }
    }
  }
        
  disp = DISP_SHR;
  cleanupDisp = DISP_PASS;    
  errorDisp = DISP_PASS;
  modeMask = ACCESS_WRITE;
  toc = NULL; 

  switch (spaceType) {
    case SPACETYPE_BLK : break;
    case SPACETYPE_TRK : size = size*56664;
                         extend = extend*56664; 
                         break;
    case SPACETYPE_CYL : size = size*56664*15;
                         extend = extend*56664*15; 
                         break;
    case SPACETYPE_MB :  size = size*1024*1024/blockSize+blockSize;
                         extend = extend*1024*1024/blockSize+blockSize;
                         break;
  }
  
  sprintf(entry.path,"%s",fileName);  
  entry.format = format;
  entry.dirSize = dirSize;
  entry.recSize = recSize;
  entry.blockSize = blockSize;
  entry.lastBlockNr = -1;  
  entry.numOfBlocks = 0;
  entry.eodPos = 0;
  entry.maxExtends = 16;
  entry.numOfExtends = 0;
  start = 0;
  entry.extends[0].startPos = start;
  entry.extends[0].sizeInBlocks = size;
  start = start + entry.extends[0].sizeInBlocks*entry.blockSize;
  for (i = 1; i < entry.maxExtends; i++) {
    entry.extends[i].startPos = start;
    entry.extends[i].sizeInBlocks = extend;
    start = start + entry.extends[i].sizeInBlocks*entry.blockSize;
  }
  entry.nextEntries[0] = 0;
  entry.nextEntries[1] = 0;
  entry.nextEntries[2] = 0;
  entry.nextEntries[3] = 0;
  entry.nextEntries[4] = 0;
}


DataSetDef::~DataSetDef() {
}


int DataSetDef::isNumber(char *str) {
  int i,l = strlen(str);
  
  for (i = 0; i < l; i++) {
    if ((str[i] < 48) || (str[i] > 57)) return 0;
  }
  
  return 1;
}


void DataSetDef::setNext(DataSetDef *next) {
  this->next = next;
}


DataSet *DataSetDef::open(int mode) {
  DataSet *dataSet = NULL;
  DataSetDef *def;
  
  mode = mode & modeMask;
printf("open datasetdef %d %x %s\n",defType,toc,dsn);
  switch (defType) {
    case DEFTYPE_DSN : if (toc != NULL) {
                         dataSet = toc->allocate((unsigned char*)dsn,format,
                                                 recSize,blockSize,
                                                 spaceType,size,extend,dirSize,
                                                 disp,mode);
                       }
                       break;
    case DEFTYPE_FILE: dataSet = new DataSet(entry,mode);
                       break;
  }
  
  if ((next != NULL) && (dataSet != NULL)) {
    dataSet = new Concatenation(dataSet);
    def = next;
    
    while (def != NULL) {
      if (((Concatenation*)dataSet)->addDataSet(def->open(mode)) < 0) {
        delete dataSet;
        return NULL;
      }
      def = def->next;
    }    
  }
  
  dataSet->setWriteImmediate(1);
  this->dataSet = dataSet;
  return dataSet;
}


DataSet *DataSetDef::getDataSet() {
  return this->dataSet;
}


int DataSetDef::cleanup(int conditionCode) {
  if (defType == DEFTYPE_DSN) {
    if (conditionCode == 0) {
      if (cleanupDisp == DISP_CATLG) {
        return c->catalog(dsn,volume);
      } else
      if (cleanupDisp == DISP_DELETE) {
        if (toc != NULL) {
          return toc->remove((unsigned char*)dsn);
        }
      }
    } else {
      if (errorDisp == DISP_DELETE) {
        if (toc != NULL) {
          return toc->remove((unsigned char*)dsn);
        }
      }
    }
  }
  
  return 0;
}
