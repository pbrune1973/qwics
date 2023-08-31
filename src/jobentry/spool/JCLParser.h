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

#ifndef _JCLParser_h
#define _JCLParser_h

#include <stdio.h>
#include "../card/JobCard.h"
#include "../dataset/DataSet.h"

#define EOL_EXCPT 0x00000001;
#define MC_EXCPT  0x00000002;
#define IJC_EXCPT 0x00000004;
#define EOF_EXCPT 0x00000008;
#define CMD_EXCPT 0x00000010;


class JCLParser {
 private:
  char       tokenBuf[81];
  char       lineBuf[81];
  int        tokenPos;
  int        linePos;
  long       lineRecIndex;
  unsigned   lineNumber;
  JobCard    *currentJob;
  JobCard    *currentBlock;
  JobCard    *currentStep;
  JobCard    *currentCard;
  FILE       *jclFile;
  DataSet    *jclDataSet;
    
 public:
  JCLParser(char *jclFileName);
  JCLParser(FILE *jclFile);
  JCLParser(DataSet *jclDataSet);
  ~JCLParser();
  
  char *getLineBuf();
  void createSysinDataSet(char *terminationStr);
  virtual void getNextLine();
  void getNextValidLine();
  char *getNextToken();
  Parameters *parseParameters();
  void parseCard();
  JobCard *parse();
};

#endif
