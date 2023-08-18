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
#include "OUTPUT.h"

using namespace std;


OUTPUT::OUTPUT(char *name) : JobCard(name) {
  delete this->name;
  int l = strlen(name); if (l > 17) l = 17;
  this->name = new char[l+1];
  for (int i = 0; i < l; i++) this->name[i] = name[i];
  this->name[l] = 0x00; 

  savedParams = NULL;
}


OUTPUT::~OUTPUT() {
  if (savedParams != NULL) delete savedParams;
}
  

JobCard *OUTPUT::getCopy() {
  OUTPUT *copy = new OUTPUT(this->name);

  if (this->params != NULL) {
    copy->setParameters(this->params->getCopy());
  }

  if (this->parameterOverrides != NULL) {
    copy->setParameterOverrides(this->parameterOverrides->getCopy());
  }

  copy->setSourceLineNumber(this->sourceLineNumber);
  for (int i = 0; i < this->sourceLineCount; i++) {
    copy->addSourceLine(this->sourceLines[i]);
  }

  copy->setRuntimeContext(this->context);
  return copy;
}


int OUTPUT::equalsType(char *type) {
  if (strcmp(type,"OUTPUT") == 0) 
    return 1;
  else 
    return 0;
}


int OUTPUT::execute() {
  context->writeLog(sourceLineNumber,sourceLines[0]);
  for (int i = 1; i < sourceLineCount; i++) {
    context->writeLog(0,sourceLines[i]);
  }

  Parameters *params = this->params->getRuntimeParams(parameterOverrides,context->getGlobalParams(),NULL);
  params->toString();
  cout << endl;
    
  this->savedParams = this->params;
  this->params = params;
  return 0;
}


int OUTPUT::executeSpecial(Parameters *params, JobCard *SubCards) {
  delete this->params;
  this->params = this->savedParams;
  this->savedParams = NULL;
  return 0;
}
