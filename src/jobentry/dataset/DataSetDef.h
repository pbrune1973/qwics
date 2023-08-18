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

#ifndef _DataSetDef_h
#define _DataSetDef_h

#include "DataSet.h"
#include "TOC.h"
#include "../card/Parameters.h"
#include "Catalog.h"

#define DISP_CATLG   0
#define DISP_DELETE  1
#define DISP_PASS    2

#define DEFTYPE_NONE   0   
#define DEFTYPE_DUMMY  1
#define DEFTYPE_TERM   2  
#define DEFTYPE_DSN    3   
#define DEFTYPE_FILE   4


class DataSetDef {
 private: 
  int defType;
  Catalog *c;
  TOC *toc;
  struct TocEntry entry;
  char dsn[55];
  char volume[7];
  char format; 
  int recSize;
  int blockSize; 
  int spaceType; 
  int size;
  int extend; 
  int dirSize; 
  int disp;
  int cleanupDisp;
  int errorDisp;
  int modeMask;
  DataSetDef *next;
  
  int isNumber(char *str);
  
 public:
  DataSetDef(Parameters *ddParams);
  DataSetDef(char *fileName, Parameters *ddParams);
  ~DataSetDef();

  void setNext(DataSetDef *next);
  DataSet *open(int mode);
  int cleanup(int conditionCode);
};

#endif
