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

#include "JobCard.h"
#include "../spool/SpoolingSystem.h"
#include "../dataset/DataSetDef.h"


struct SysOutInfo {
  char msgClass[9];
  Parameters *params;
};


class DD : public JobCard {
 private:
  char *fileName;
  int deleteFile;
  JobInfo sysoutFileInfo;
  char sysoutFileName[13];
  SysOutInfo submits[40];
  int numOfSubmits;
  DataSetDef *dataSetDef;
  
  void splitString(char *str, char separator, char **subStrings, int *nMax);
  
 public:
  DD(char *name);
  ~DD();
  DataSetDef *getDataSetDef();
  virtual JobCard *getCopy();

  virtual char *getFileName();
  virtual int equalsType(char *type);
  virtual int execute();
  virtual int executeSpecial(Parameters *params, JobCard *SubCards);
  virtual void print(FILE *file);  
};