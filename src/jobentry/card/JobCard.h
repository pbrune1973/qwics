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

#ifndef _JobCard_h
#define _JobCard_h

#include "RuntimeContext.h"

#define ABORT_EXCPT 0x0001;


class JobCard {
 protected:
  Parameters     *params;
  Parameters     *parameterOverrides;
  Parameters     *runtimeParams;
  int            conditionCode;
  JobCard        *firstSubCard;
  JobCard        *lastSubCard;
  JobCard        *lastRegularSubCard;
  unsigned       sourceLineNumber;
  unsigned       sourceLineCount;
  char           **sourceLines;
  RuntimeContext *context;
    
 public:
  char       *name;
  JobCard    *nextJobCard;

  JobCard(char *name);
  ~JobCard();
  virtual JobCard *getCopy();
  
  int getConditionCode();
  void getNameParts(char *first, char* second);
  JobCard *getSubCard(char *name);
  JobCard *getSubCard(char *name, char *type);
  JobCard *getFirstSubCard(char *type);
  JobCard *getSubCard(char *type, char *param, char *value);
  void addSubCard(JobCard *subCard);
  void beginTempSubCards();
  void removeTempSubCards();
  void setParameters(Parameters *params);
  Parameters *getParameters();
  void setParameterOverrides(Parameters *parameterOverrides);
  void setSourceLineNumber(unsigned sourceLineNumber);
  void addSourceLine(char *line);
  void setRuntimeContext(RuntimeContext *context);
  virtual int equalsType(char *type);
  virtual char *getFileName();
  virtual int execute();
  virtual int executeSpecial(Parameters *params, JobCard *SubCards);
  virtual void print(FILE *file);
};

#endif
