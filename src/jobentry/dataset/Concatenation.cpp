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

#include "Concatenation.h"

 
Concatenation::Concatenation(DataSet *firstDataSet) : 
               PartitionedDataSet(firstDataSet->getEntry(),ACCESS_READ) {
  if (firstDataSet->isPartitionedDataSet()) 
    type = 'P';
  else 
    type = 'N';

  firstDS.dataSet = firstDataSet;
  firstDS.next = NULL;
  lastDS = &firstDS;
  currentDS = &firstDS;
  if (!firstDataSet->isPartitionedDataSet()) currentDS->dataSet->point((long)0);
}


Concatenation::~Concatenation() {
  if (firstDS.dataSet != NULL) delete firstDS.dataSet;
  if (firstDS.next != NULL) deleteDS(firstDS.next);
}


void Concatenation::deleteDS(ConcatDS *ds) {
  if (ds->next != NULL) deleteDS(ds->next);
  if (ds->dataSet != NULL) delete ds->dataSet;
  delete ds;
}


int Concatenation::addDataSet(DataSet *newDataSet) {  
  if (this->isPartitionedDataSet()) {
    if (!newDataSet->isPartitionedDataSet()) return -1;
  } else {
    if ((this->getFormat() != newDataSet->getFormat()) || 
        (this->getRecSize() != newDataSet->getRecSize()))
      return -1;
  }
  
  lastDS->next = new ConcatDS;
  lastDS = lastDS->next;
  lastDS->next = NULL;
  lastDS->dataSet = newDataSet;         
}


int Concatenation::point(long recNr) {
  if (this->isPartitionedDataSet()) return -1;

  if (recNr == 0) {
    currentDS = &firstDS;
    return currentDS->dataSet->point((long)0);
  } else 
    return -1;
}


int Concatenation::point(struct RequestParameters *rpl) {
  if (this->isPartitionedDataSet()) return -1;

  if (rpl->mode & RPMODE_NUL) {
    currentDS = &firstDS;
    return currentDS->dataSet->point(rpl);
  } else 
    return -1;
}


int Concatenation::get(unsigned char *record) {
  if (this->isPartitionedDataSet()) return -1;

  while (currentDS != NULL) {
    if (currentDS->dataSet->get(record) < 0) {
      currentDS = currentDS->next;
      if (currentDS != NULL) {
        if (currentDS->dataSet->point((long)0) < 0) return -1;
      }
    } else 
      return 0;    
  } 
  
  return -1;
}


int Concatenation::get(struct RequestParameters *rpl) {
  struct RequestParameters r;
  
  if (this->isPartitionedDataSet()) return -1;

  while (currentDS != NULL) {
    if (currentDS->dataSet->get(rpl) < 0) {
      currentDS = currentDS->next;
      if (currentDS != NULL) {
        r.mode = RPMODE_NUL | (rpl->mode & RPMODE_DRR) | (rpl->mode & RPMODE_DRP);
        if (currentDS->dataSet->point(&r) < 0) return -1;
      }
    } else 
      return 0;    
  } 
  
  return -1;
}


DataSet *Concatenation::findMember(char *name, struct PdsDirEntry *e) {
  struct ConcatDS *ds = &firstDS;
  DataSet *r;
  
  if (!this->isPartitionedDataSet()) return NULL;  
  
  while (ds != NULL) {
    r = ((PartitionedDataSet*)ds)->findMember(name,e);
    if (r != NULL) return r;
    ds = ds->next;
  }    

  return NULL;
}
