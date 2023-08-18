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
#include "JobCard.h"
#include <iostream>

using namespace std;


JobCard::JobCard(char *name) {
  this->name = new char[9];
  int l = strlen(name); if (l > 8) l= 8;
  for (int i = 0; i < l; i++) this->name[i] = name[i];
  this->name[l] = 0x00; 

  params = NULL;
  parameterOverrides = NULL;
  runtimeParams = NULL;
  conditionCode = 0;
  firstSubCard = NULL;
  lastSubCard = NULL;
  nextJobCard = NULL;
  sourceLineCount = 0;
  sourceLines = new char*[20];
}


JobCard::~JobCard() {
  delete name;
  if (params != NULL) delete params;
  if (runtimeParams != NULL) delete runtimeParams;
  if (nextJobCard != NULL) delete nextJobCard;
  if (firstSubCard != NULL) delete firstSubCard;
  
  for (int i = 0; i < sourceLineCount; i++) {
    delete sourceLines[i];
  }
  delete sourceLines;
}
  

JobCard *JobCard::getCopy() {
  return NULL;
}


int JobCard::getConditionCode() {
  return conditionCode;
}


void JobCard::getNameParts(char *first, char* second) {
  int i=0,j,l=strlen(name);

  while ((i < l) && (name[i] != '.')) {
    first[i] = name[i];
    i++;
  }
  first[i] = 0x00;

  i++; 
  j=0;
  while (i < l) {
    second[j] = name[i];
    i++;
    j++;
  }
  second[j] = 0x00;
}


JobCard *JobCard::getSubCard(char *name) {
  JobCard *card = firstSubCard;
  if (card == NULL) return NULL;
    
  do {
    if (strcmp(card->name,name) == 0) return(card);
  } while ((card = card->nextJobCard) != NULL);
    
  return NULL;
}


JobCard *JobCard::getSubCard(char *name, char *type) {
  JobCard *card = firstSubCard;
  if (card == NULL) return NULL;
    
  do {
    if ((strcmp(card->name,name) == 0) && card->equalsType(type)) 
      return(card);
  } while ((card = card->nextJobCard) != NULL);
    
  return NULL;
}


JobCard *JobCard::getFirstSubCard(char *type) {
  JobCard *card = firstSubCard;
  if (card == NULL) return NULL;
    
  do {
    if (card->equalsType(type)) return(card);
  } while ((card = card->nextJobCard) != NULL);
    
  return NULL;
}


JobCard *JobCard::getSubCard(char *type, char *param, char *value) {
  JobCard *card = firstSubCard;
  char *dval = NULL;
  if (card == NULL) return NULL;
    
  do {
    dval = card->getParameters()->getValue(param,0);

    if (card->equalsType(type) && 
        (dval != NULL) && (strcmp(dval,value) == 0)) 
       return(card);
  } while ((card = card->nextJobCard) != NULL);
    
  return NULL;
}


void JobCard::addSubCard(JobCard *subCard) {
  if (lastSubCard == NULL) {
    firstSubCard = subCard;
    lastSubCard = firstSubCard;
  } else {
    lastSubCard->nextJobCard = subCard;
    lastSubCard = subCard;    
  }  
}


void JobCard::beginTempSubCards() {
  lastRegularSubCard = lastSubCard;
}


void JobCard::removeTempSubCards() {
  if ((lastRegularSubCard != NULL) && (lastRegularSubCard->nextJobCard != NULL)) {
    delete lastRegularSubCard->nextJobCard;
  }
}


void JobCard::setParameters(Parameters *params) {
  this->params = params;
}


Parameters *JobCard::getParameters() {
  return this->params;
}


void JobCard::setParameterOverrides(Parameters *parameterOverrides) {
  this->parameterOverrides = parameterOverrides;
}


void JobCard::setSourceLineNumber(unsigned sourceLineNumber) {
  this->sourceLineNumber = sourceLineNumber;
}


void JobCard::addSourceLine(char *line) {
  int i,l = strlen(line);
  
  if (sourceLineCount >= 20) return;
  
  sourceLines[sourceLineCount] = new char[l+1];
  for (i = 0; i < l; i++) {
    sourceLines[sourceLineCount][i] = line[i];
  }
  sourceLines[sourceLineCount][i] = 0x00;
  sourceLineCount++;
}


void JobCard::setRuntimeContext(RuntimeContext *context) {
  this->context = context;
}


int JobCard::equalsType(char *type) {
  return 0;
}


char *JobCard::getFileName() {
  return NULL;
}


int JobCard::execute() {
  return conditionCode;
}


int JobCard::executeSpecial(Parameters *params, JobCard *SubCards) {
  return conditionCode;
}


void JobCard::print(FILE *file) {
  for (int i = 0; i < sourceLineCount; i++) {
    fputs(sourceLines[i],file);
    fputc('\n',file);
  } 

  JobCard *card = firstSubCard; 
  while (card != NULL) { 
    card->print(file);
    card = card->nextJobCard;
  }
}
