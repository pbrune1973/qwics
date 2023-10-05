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

#ifndef _Parameters_h
#define _Parameters_h

#include <string.h>


class Parameters {
 private:
 
  class Parameter {
    public:
     unsigned   index;
     char       *name;
     char       *value;
     Parameters *pvalue;
     Parameter  *next;

     
     Parameter(unsigned index, char *name) {
       this->index = index;
       value = NULL;
       pvalue = NULL;
       next = NULL;
       
       int i,l = strlen(name);
       this->name = new char[l+1];
       for (i = 0; i < l; i++) this->name[i] = name[i];
       this->name[i] = 0x00;
     }


     Parameter(unsigned index) {
       this->index = index;
       name = NULL;
       value = NULL;
       pvalue = NULL;
       next = NULL;       
     }
     
     
     ~Parameter() {
       if (name != NULL) {
          delete name;
       }
       if (value != NULL) {
          delete value;
       }
       /*
       if (pvalue != NULL) {
          delete pvalue;
       }
       */
       if (next != NULL) {
          delete next;
       }       
     }
  };

  Parameter *first,*last,*current;
  unsigned  indexCount;
  
 public:
  Parameters();
  ~Parameters();

  Parameters *getCopy();
  Parameters *getCopy(Parameters *lookupParams);
  Parameters *keepSelected(char *selectSuffix);
  Parameters *getRuntimeParams(Parameters *paramOverrides, Parameters *lookupParams, char *selectSuffix);

  void add();
  void add(char *param);
  void addNew(char *param);
  void setValue(char *value);
  void setValue(Parameters *value);
  void addToParams(Parameters *params);
  
  char *getValue(unsigned index);
  char *getValue(char *name, unsigned numOfMatch);
  Parameters *getPValue(unsigned index);
  Parameters *getPValue(char *name, unsigned numOfMatch);
  
  char *toString();
  virtual void print(FILE *file, int startNew);
};

#endif
