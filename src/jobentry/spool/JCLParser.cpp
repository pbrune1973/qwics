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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <iostream>

#include "JCLParser.h"
#include "../card/JOB.h"
#include "../card/EXEC.h"
#include "../card/DD.h"
#include "../card/PROC.h"
#include "../card/SET.h"
#include "../card/OUTPUT.h"
#include "../dataset/DataSetDef.h"
#include "SpoolingSystem.h"

using namespace std;


JCLParser::JCLParser(char *jclFileName) {
  linePos = 0;
  tokenPos = 0;
  currentJob = NULL;
  currentBlock = NULL;
  currentStep = NULL;
  currentCard = NULL;
  lineNumber = 0;
  jclFile = fopen(jclFileName,"rb");
  this->jclDataSet = NULL;  
}


JCLParser::JCLParser(FILE *jclFile) {
  linePos = 0;
  tokenPos = 0;
  currentJob = NULL;
  currentBlock = NULL;
  currentStep = NULL;
  currentCard = NULL;
  lineNumber = 0;
  this->jclFile = jclFile;
  this->jclDataSet = NULL;  
}


JCLParser::JCLParser(DataSet *jclDataSet) {
  linePos = 0;
  tokenPos = 0;
  currentJob = NULL;
  currentBlock = NULL;
  currentStep = NULL;
  currentCard = NULL;
  lineNumber = 0;
  this->jclFile = NULL;
  this->jclDataSet = jclDataSet;
  this->jclDataSet->setTranslationMode(XMODE_ASCII);
}


JCLParser::~JCLParser() {
}


char *JCLParser::getLineBuf() {
  return lineBuf;
}


void JCLParser::createSysinDataSet(char *terminationStr) {
  char dsn[45];
  Parameters *params,*subParams;
  DataSet *sysinDataSet;
  DataSetDef *sysinDef;
  int l;

  sprintf(dsn,"SYS%d",(int)SpoolingSystem::spoolingSystem->getNewId());
  if (currentBlock->name[0] != 0x00) {
    sprintf(dsn,"%s.%s",dsn,currentBlock->name);
  }
  if (currentStep->name[0] != 0x00) {
    sprintf(dsn,"%s.%s",dsn,currentStep->name);
  }
  if (currentCard->name[0] != 0x00) {
    sprintf(dsn,"%s.%s",dsn,currentCard->name);
  }
cout << "SYSIN " << dsn << endl;

  params = currentCard->getParameters();
  params->add("DSN");
  params->setValue(dsn);
  params->add("DISP");
  subParams = new Parameters();
  subParams->add();
  subParams->setValue("NEW");
  subParams->add();
  subParams->setValue("DELETE");
  subParams->add();
  subParams->setValue("DELETE");
  params->setValue(subParams);
  params->add("VOL");
  subParams = new Parameters();
  subParams->add("SER");
  subParams->setValue("WORK");
  params->setValue(subParams);
  params->addNew("DCB");
  subParams = new Parameters();
  subParams->add("LRECL");
  subParams->setValue("80");
  subParams->add("BLKSIZE");
  subParams->setValue("3200");
  subParams->add("RECFM");
  subParams->setValue("F");
  params->setValue(subParams);

  sysinDef = new DataSetDef(params);
  sysinDataSet = sysinDef->open(ACCESS_WRITE);
  if (sysinDataSet == NULL) throw CMD_EXCPT;

  l = strlen(terminationStr);
  do {
    sysinDataSet->put((unsigned char*)lineBuf);
    getNextLine();
  } while (strncmp(lineBuf,terminationStr,l) != 0);

  delete sysinDataSet;
  delete sysinDef;

  subParams = params->getPValue("DISP",0);
  delete subParams;
  params->add("DISP");
  subParams = new Parameters();
  subParams->add();
  subParams->setValue("OLD");
  subParams->add();
  subParams->setValue("DELETE");
  subParams->add();
  subParams->setValue("DELETE");
  params->setValue(subParams);  
}


void JCLParser::getNextLine() {
  int i,r;
  char c;

  if (jclDataSet == NULL) {
    if (feof(jclFile)) throw EOF_EXCPT;

    do {
      lineRecIndex = ftell(jclFile);
      c = fgetc(jclFile);
    } while ((c == 10) || (c == 13));  
    i = 0;

    while ((c != 10) && (c != 13) && (c != (char)EOF)) {
      if (i < 80) {
        lineBuf[i] = c;
        i++;
      }
      c = fgetc(jclFile);
    }
    if (c == (char)EOF) {
      throw EOF_EXCPT;
    }
  } else {  
    r = jclDataSet->get((unsigned char*)lineBuf);
    i = 80;
    if (r == -2) {
      lineBuf[0] = '/';
      lineBuf[1] = '/';
      i = 2;
    } else 
    if (r < 0) {
      throw EOF_EXCPT;
    }
  } 

  for (int j = i; j < 80; j++) lineBuf[j] = ' '; 
  lineBuf[80] = 0x00;
 printf("%s\n",lineBuf); 
  linePos = 2;
  lineNumber++;
}

  
void JCLParser::getNextValidLine() {
  char head3[4];
  char head2[3];
  
  do {
    getNextLine();
    if (lineBuf[0] == '*') {
      throw CMD_EXCPT;
    }
    
    if ((lineBuf[0] != '/') && (currentCard != NULL)) {
      if (strcmp(currentCard->getParameters()->getValue(0),"*") == 0) {
        createSysinDataSet("//");
      } else {
        throw CMD_EXCPT;
      }
    }
    
    strncpy(head3,lineBuf,3);
    head3[3] = 0x00;
    strncpy(head2,lineBuf,2);
    head2[2] = 0x00;
  } while ((strcmp(head3,"//*") == 0) || (strcmp(head2,"/*") == 0) || (strcmp(head2,"//") != 0));  
}

  
char *JCLParser::getNextToken() {
  tokenPos = 0;
  int quoteMode = 0;
  
  while ((linePos < 72) && (lineBuf[linePos] == ' ')) linePos++;

  if (linePos >= 72) {
    if (strcmp(tokenBuf,",") == 0) {
      getNextLine();
      if ((lineBuf[0] != '/') || (lineBuf[1] != '/') || (lineBuf[2] != ' ')) {
        throw MC_EXCPT;
      }
    
      while ((linePos < 72) && (lineBuf[linePos] == ' ')) linePos++;

      if (currentCard != NULL) {
        currentCard->addSourceLine(lineBuf);
      }
    } else {
      throw EOL_EXCPT;
    }
  }
  
  do {
    if (lineBuf[linePos] == '\'') {
      linePos++; 
      
      while ((linePos < 72) && 
             ((lineBuf[linePos] != '\'') || 
              ((linePos < 71) && (lineBuf[linePos] == '\'') && (lineBuf[linePos+1] == '\'')))) {
        tokenBuf[tokenPos] = lineBuf[linePos];
        if (lineBuf[linePos] == '\'') linePos++;
        linePos++;
        tokenPos++;
      }
    } else
    if ((tokenPos > 1) && (lineBuf[linePos+1] == '(')) {
      do {
        tokenBuf[tokenPos] = toupper(lineBuf[linePos]);
        tokenPos++;
        linePos++;
      } while ((linePos < 72) && (lineBuf[linePos] != ')'));

      tokenBuf[tokenPos] = toupper(lineBuf[linePos]);
      tokenPos++;
    } else {
      tokenBuf[tokenPos] = toupper(lineBuf[linePos]);
      tokenPos++;
    }
    
    linePos++;    
  } while ((linePos < 72) && 
           (lineBuf[linePos] != ',') && (lineBuf[linePos] != '=') && (lineBuf[linePos] != '(') && 
           (lineBuf[linePos] != ')') /* && (lineBuf[linePos] != '*') */ && (lineBuf[linePos] != ' ') && 
           (tokenBuf[0] != ',') && (tokenBuf[0] != '=') && (tokenBuf[0] != '(') && 
           (tokenBuf[0] != ')') /* && (tokenBuf[0] != '*') */);

  tokenBuf[tokenPos] = 0x00;
//cout << "token " << tokenBuf << "\n";
  return tokenBuf;
}


Parameters *JCLParser::parseParameters() {
  Parameters *params = new Parameters();
  char *token = "";
  
  try {
    do {
      token = getNextToken();
      if (strcmp(token,"(") == 0) {
        params->add();
        params->setValue(parseParameters());

        token = getNextToken();
      } else
      if ((strcmp(token,",") != 0) && (strcmp(token,")") != 0)) {
        params->add(token);
  
        token = getNextToken();
        while (strcmp(token,"=") == 0) {
          token = getNextToken();
          if (strcmp(token,"(") == 0) {
            params->setValue(parseParameters());
            token = getNextToken();
          } else
          if (strcmp(token,",") != 0) {
            params->setValue(token);
            token = getNextToken();
          }
        }
      }
    } while (strcmp(token,",") == 0);
  } catch (const int &e) {
//    if (e == CMD_EXCPT) throw CMD_EXCPT;
    if (e == 8) {
      throw CMD_EXCPT;
    }
  }
  
  return params;
}


void JCLParser::parseCard() {
  char name[81] = "";
  char *token = getNextToken();
  
  if ((strcmp(token,"EXEC") != 0) && (strcmp(token,"DD") != 0) && (strcmp(token,"SET") != 0) && 
      (strcmp(token,"PROC") != 0) && (strcmp(token,"PEND") != 0)) {
    int l = strlen(token);
    for (int i = 0; i < l; i++) name[i] = token[i];
    name[l] = 0x00; 
  
    token = getNextToken();
  }

  if (strcmp(token,"JOB") == 0) {
    currentJob = new JOB(name);
    currentCard = currentJob;
    currentCard->setSourceLineNumber(lineNumber);
    currentCard->addSourceLine(lineBuf);
    currentJob->setParameters(parseParameters());
    currentBlock = currentJob;
    currentStep = NULL;
  } else 
  if (strcmp(token,"PROC") == 0) {
    currentBlock = new PROC(name);
    currentCard = currentBlock;
    currentCard->setSourceLineNumber(lineNumber);
    currentCard->addSourceLine(lineBuf);
    currentBlock->setParameters(parseParameters());
    if (currentJob != NULL) {
      currentJob->addSubCard(currentBlock);
    }
    currentStep = NULL;
  } else 
  if (strcmp(token,"PEND") == 0) {
    currentBlock = currentJob;
    currentStep = NULL;
  } else 
  if (strcmp(token,"SET") == 0) {
    currentStep = new SET(name);
    currentCard = currentStep;
    currentCard->setSourceLineNumber(lineNumber);
    currentCard->addSourceLine(lineBuf);
    currentStep->setParameters(parseParameters());
    currentBlock->addSubCard(currentStep);
    currentStep = NULL;
  } else 
  if (strcmp(token,"OUTPUT") == 0) {
    if (currentStep == NULL) {
      currentStep = new OUTPUT(name);
      currentCard = currentStep;
      currentCard->setSourceLineNumber(lineNumber);
      currentCard->addSourceLine(lineBuf);
      currentStep->setParameters(parseParameters());
      currentBlock->addSubCard(currentStep);
      currentStep = NULL;
    } else {
      OUTPUT *currentOUTPUT = new OUTPUT(name);
      currentCard = currentOUTPUT;
      currentCard->setSourceLineNumber(lineNumber);
      currentCard->addSourceLine(lineBuf);
      currentOUTPUT->setParameters(parseParameters());
      currentStep->addSubCard(currentOUTPUT);
    }
  } else 
  if (strcmp(token,"EXEC") == 0) {
    currentStep = new EXEC(name);
    currentCard = currentStep;
    currentCard->setSourceLineNumber(lineNumber);
    currentCard->addSourceLine(lineBuf);
    currentStep->setParameters(parseParameters());
    currentBlock->addSubCard(currentStep);
  } else 
  if (strcmp(token,"DD") == 0) {
    if (currentStep == NULL) {
      currentStep = new DD(name);
      currentCard = currentStep;
      currentCard->setSourceLineNumber(lineNumber);
      currentCard->addSourceLine(lineBuf);
      currentStep->setParameters(parseParameters());
      currentBlock->addSubCard(currentStep);
      currentStep = NULL;
    } else {
      DD *currentDD = new DD(name);
      currentCard = currentDD;
      currentCard->setSourceLineNumber(lineNumber);
      currentCard->addSourceLine(lineBuf);
      currentDD->setParameters(parseParameters());
      currentStep->addSubCard(currentDD);
    }
  } else {
    throw IJC_EXCPT;
  }
}


JobCard *JCLParser::parse() {
  lineNumber = 0;

  currentJob = NULL;
  currentBlock = NULL;
  
  try {
    getNextValidLine();

    do {    
      parseCard();
      getNextValidLine();
    } while (strcmp(lineBuf,"//                                                                              ") != 0 &&
             strcmp(lineBuf,"//*                                                                             ") != 0);  
  } catch (const int &ex) {
    if (ex != 8) {
      if (currentJob != NULL) {
        delete currentJob; 
      } else
      if (currentBlock != NULL) {
        delete currentBlock; 
      }    
/*
      if (excpt != CMD_EXCPT) {
        cout << "Fehler : " << excpt << "\n";
      }
*/
      return NULL;
    }
  }
 
  if (currentJob != NULL) {
    return currentJob;
  } else
  if (currentBlock != NULL) {
    return currentBlock;
  }

  return NULL;
} 
