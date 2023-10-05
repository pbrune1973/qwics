/*******************************************************************************************/
/*   QWICS Batch Job Entry System                                                          */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 05.10.2023                                  */
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
#include <iostream>
#include "Parameters.h"

using namespace std;


Parameters::Parameters() {
  first = NULL;
  last = NULL;
  current = NULL;
  indexCount = 0;
}


Parameters::~Parameters() {
  if (first != NULL) {
    delete first;
  }
}


Parameters *Parameters::getCopy() {
  Parameter  *pos = first;
  Parameters *copy = new Parameters();

  while (pos != NULL) {
    if (pos->name != NULL) {
      copy->addNew(pos->name);
    } else {
      copy->add();
    }

    if (pos->value != NULL) {
      copy->setValue(pos->value);
    }

    if (pos->pvalue != NULL) {
      copy->setValue(pos->pvalue->getCopy());
    }

    pos = pos->next;
  }

  return copy;
}


Parameters *Parameters::getCopy(Parameters *lookupParams) {
  if (lookupParams == NULL) return this->getCopy();
  Parameter  *pos = first;
  Parameters *copy = new Parameters(),*pvalue = NULL;
  char *key = "";
  
  while (pos != NULL) {
    if (pos->name != NULL) {
      copy->addNew(pos->name);
    } else {
      copy->add();
    }

    if (pos->value != NULL) {
      if (pos->value[0] == '&') {
        char *key = &(pos->value[1]);
        copy->setValue(lookupParams->getValue(key,0));        
        pvalue = lookupParams->getPValue(key,0);
        if (pvalue != NULL) copy->setValue(pvalue->getCopy());        
      } else {      
        copy->setValue(pos->value);
      }  
    }

    if (pos->pvalue != NULL) {
      copy->setValue(pos->pvalue->getCopy(lookupParams));
    }

    pos = pos->next;
  }

  return copy;
}


Parameters *Parameters::keepSelected(char *selectSuffix) {
  Parameter *pos = first, *oldPos = NULL;
  int i,l;
  char *suffix;
    
  while (pos != NULL) {
    suffix = "";

    if ((pos->name != NULL) && (selectSuffix != NULL)) {
      i = 0; 
      l = strlen(pos->name);
      while ((pos->name[i] != '.') && (i < l)) i++;
      if (i < l) {
        suffix = &(pos->name[i+1]);
        pos->name[i] = 0x00;
      }
    }

    if (((pos->name != NULL) && (pos->value == NULL) && (pos->pvalue == NULL)) || 
        ((pos->name == NULL) && (pos->value == NULL) && (pos->pvalue != NULL))) {
      if (oldPos != NULL) {
        oldPos->next = pos->next;
        if (pos == last) {
          last = oldPos;
          current = last;
        }
        pos->next = NULL;
        delete pos;
        pos = oldPos;
        pos = pos->next;
      } else {
        first = pos->next;
        if (pos == last) {
          last = first;
          current = last;
        }
        pos->next = NULL;
        delete pos;
        pos = first;
      }  
    } else 
    if ((strcmp(pos->name,"PGM") == 0) || (strcmp(pos->name,"PROC") == 0) || 
        ((selectSuffix != NULL) && (strcmp(suffix,"") != 0) && (strcmp(suffix,selectSuffix) != 0))) {
      if (oldPos != NULL) {
        oldPos->next = pos->next;
        if (pos == last) {
          last = oldPos;
          current = last;
        }
        pos->next = NULL;
        delete pos;
        pos = oldPos;
        pos = pos->next;
      } else {
        first = pos->next;
        if (pos == last) {
          last = first;
          current = last;
        }
        pos->next = NULL;
        delete pos;
        pos = first;
      }  
    } else {
      oldPos = pos;
      pos = pos->next;
    }
  }

  return this;  
}


Parameters *Parameters::getRuntimeParams(Parameters *paramOverrides, Parameters *lookupParams, char *selectSuffix) {  
  if (paramOverrides == NULL) return this->getCopy(lookupParams);
  
  Parameter  *pos = first;
  Parameters *copy = paramOverrides->getCopy(lookupParams)->keepSelected(selectSuffix),*pvalue = NULL;
  char *key = "";

  while (pos != NULL) {
    if (pos->name != NULL) {
      copy->addNew(pos->name);
    } else {
      copy->add();
    }

    if (pos->value != NULL) {
      if (pos->value[0] == '&') {
        char *key = &(pos->value[1]);
        copy->setValue(lookupParams->getValue(key,0));        
        pvalue = lookupParams->getPValue(key,0);
        if (pvalue != NULL) copy->setValue(pvalue->getCopy());        
      } else {      
        copy->setValue(pos->value);
      }  
    }

    if (pos->pvalue != NULL) {
      copy->setValue(pos->pvalue->getCopy(lookupParams));
    }

    pos = pos->next;
  }

  return copy;
}


void Parameters::add() {
//  cout << "Parameters::add() unnamed" << endl;
  
  if (last != NULL) {
    last->next = new Parameter(indexCount);
    last = last->next;
    indexCount++;
  } else {
    first = new Parameter(indexCount);
    last = first;
    indexCount++;
  }
  
  current = last;
}


void Parameters::add(char *param) {
//  cout << "Parameters::add() " << param << endl;
  
  Parameter *pos = first;

  while (pos != NULL) {
    if ((pos->name != NULL) && (strcmp(pos->name,param) == 0)) {
      current = pos;
      return;  
    }
    
    pos = pos->next;
  }

  if (last != NULL) {
    last->next = new Parameter(indexCount,param);
    last = last->next;
    indexCount++;
  } else {
    first = new Parameter(indexCount,param);
    last = first;
    indexCount++;
  }

  current = last;
}


void Parameters::addNew(char *param) {
//  cout << "Parameters::addNew() " << param << endl;
  
  if (last != NULL) {
    last->next = new Parameter(indexCount,param);
    last = last->next;
    indexCount++;
  } else {
    first = new Parameter(indexCount,param);
    last = first;
    indexCount++;
  }

  current = last;
}


void Parameters::setValue(char *value) {
//  cout << "Parameters::setValue() " << value << endl;

  if ((current != NULL) && (value != NULL)) {
    if (current->value != NULL) {
      addNew(current->name);      
    }

    int i,l = strlen(value);
    current->value = new char[l+1];
    for (i = 0; i < l; i++) current->value[i] = value[i];
    current->value[i] = 0x00;
  } else 
  if (current != NULL) {
    if (current->value != NULL) delete current->value;
    current->value = NULL;
  }
}


void Parameters::setValue(Parameters *value) {
//  cout << "Parameters::setValue() parameters" << endl;

  if (current != NULL) {
    if (current->pvalue != NULL) {
      addNew(current->name);      
    }
    
    current->pvalue = value;
  }
}


void Parameters::addToParams(Parameters *params) {
  Parameter *pos = first;

  while (pos != NULL) {
    if (pos->name != NULL) {
      params->add(pos->name);
    } else {
      params->add();
    }

    if (pos->value != NULL) {
      params->setValue((char*)NULL);
      params->setValue(pos->value);
    }

    if (pos->pvalue != NULL) {
      params->setValue((Parameters*)NULL);
      params->setValue(pos->pvalue->getCopy());
    }

    pos = pos->next;
  }
}


char *Parameters::getValue(unsigned index) {
  Parameter *pos = first;

  while (pos != NULL) {
    if (pos->index == index) {
      if (pos->value != NULL) {
        return pos->value;
      } else
      if ((pos->name != NULL) && (pos->pvalue == NULL)) {
        return pos->name;
      }
    }
    
    pos = pos->next;
  }
  
  return NULL;
}


char *Parameters::getValue(char *name, unsigned numOfMatch) {
  Parameter *pos = first;
  unsigned matchCount = 0;
  
  while (pos != NULL) {
    if ((pos->name != NULL) && (strcmp(pos->name,name) == 0)) {
      if (numOfMatch == matchCount) return pos->value;
      matchCount++;
    }
    
    pos = pos->next;
  }
  
  return NULL;
}


Parameters *Parameters::getPValue(unsigned index) {
  Parameter *pos = first;

  while (pos != NULL) {
    if (pos->index == index) {
      return pos->pvalue;
    }
    
    pos = pos->next;
  }
  
  return NULL;
}


Parameters *Parameters::getPValue(char *name, unsigned numOfMatch) {
  Parameter *pos = first;
  unsigned matchCount = 0;

  while (pos != NULL) {
    if ((pos->name != NULL) && (strcmp(pos->name,name) == 0)) {
      if (numOfMatch == matchCount) return pos->pvalue;
      matchCount++;
    }
    
    pos = pos->next;
  }
  
  return NULL;
}


char *Parameters::toString() {
  Parameter *pos = first;

  while (pos != NULL) {
    if (pos->name != NULL) {
      cout << pos->name;
    }

    if (pos->pvalue != NULL) {
      if (pos->name != NULL) cout << "=";
      cout << "(";
      pos->pvalue->toString();
      cout << ")";
    } else 
    if (pos->value != NULL) {
      if (pos->name != NULL) cout << "=";
      cout << pos->value;
    }
    
    pos = pos->next;
    if (pos != NULL) cout << ",";
  }

  return NULL;  
}


void Parameters::print(FILE *file, int startNew) {
  Parameter *pos = first;

  while (pos != NULL) {
    if (startNew) {
      fputs("// ",file);
    }

    if (pos->name != NULL) {
      fputs(pos->name,file);
    }

    if (pos->pvalue != NULL) {
      if (pos->name != NULL) fputc('=',file);
      fputc('(',file);
      pos->pvalue->print(file,0);
      fputc(')',file);
    } else 
    if (pos->value != NULL) {
      if (pos->name != NULL) fputc('=',file);
      fputs(pos->value,file);
    }
    
    pos = pos->next;
    if (pos != NULL) fputs(",\n",file);
    startNew = 1;
  }
}
