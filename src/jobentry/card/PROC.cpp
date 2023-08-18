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

#include <stdio.h>
#include <string.h>
#include <iostream>
#include "PROC.h"

using namespace std;


PROC::PROC(char *name) : JobCard(name) {
}


PROC::~PROC() {
}
  

int PROC::equalsType(char *type) {
  if (strcmp(type,"PROC") == 0) 
    return 1;
  else 
    return 0;
}


int PROC::execute() {
}


int PROC::executeSpecial(Parameters *params, JobCard *SubCards) {
  JobCard *card = firstSubCard;
  char addStep[9],first[9],second[9],*ddName = NULL;
  JobCard *addCard = SubCards;
  JobCard *execStep = NULL;
  JobCard *ddCard = NULL;
  int condCode = 0;

  params = this->params->getRuntimeParams(params,context->getGlobalParams(),NULL);
  this->runtimeParams = params;
  context->push(this);

  while (card != NULL) {
    card->setRuntimeContext(context);
    card->setParameterOverrides(params);
    card->beginTempSubCards();
    card = card->nextJobCard;
  }

  addStep[0] = 0x00;

  while (addCard != NULL) {
    addCard->getNameParts(first,second);

    if (second[0] != 0x00) {
      if (strcmp(first,addStep) != 0) {
        for (int i = 0; i < 9; i++) addStep[i] = first[i];
        execStep = getSubCard(addStep);        
      }
      ddName = second;      
    } else {
      ddName = first;
    }

    if (execStep == NULL) execStep = this->getFirstSubCard("EXEC");

    ddCard = execStep->getSubCard(ddName);
    if (ddCard != NULL) {
      ddCard->setParameterOverrides(addCard->getParameters());
    } else {
      ddCard = addCard->getCopy();
      execStep->addSubCard(ddCard);
    }

    addCard = addCard->nextJobCard;
  }

  card = firstSubCard;

  while (card != NULL) {
    int cc = card->execute();
    if (cc > condCode) condCode = cc;
    card->setParameterOverrides(NULL);
    card->removeTempSubCards();
    card = card->nextJobCard;
  }
  
  context->pop();
  this->runtimeParams = NULL;
  delete params;

  return condCode;
}


void PROC::print(FILE *file) {
  for (int i = 0; i < sourceLineCount; i++) {
    fputs(sourceLines[i],file);
    fputc('\n',file);
  } 

  JobCard *card = firstSubCard; 
  while (card != NULL) { 
    card->print(file);
    card = card->nextJobCard;
  }

  fputs("// PEND\n",file);
}
