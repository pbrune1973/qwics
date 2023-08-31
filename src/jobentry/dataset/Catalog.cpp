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

#include <string.h>
#include "Catalog.h"


Catalog *Catalog::masterCatalog = NULL;
char *Catalog::defaultVolume = NULL;


void Catalog::create(char *volume) {
  if (masterCatalog == NULL) {
    TOC *toc;
  
    toc = TOC::getToc(volume);
    if (toc == NULL) {
      toc = TOC::addToc(volume);
    }

    masterCatalog = new Catalog(toc);
  }
}


Catalog::Catalog(TOC *toc) {
  catalogDataSet = toc->allocate((unsigned char*)"SYS.MCAT",'F',
                                 sizeof(struct CatalogEntry),
                                 100*sizeof(struct CatalogEntry),SPACETYPE_BLK,
                                 100,100,0,DISP_MOD,OPEN_RDWR);
  catalogDataSet->setWriteImmediate(1);
}


Catalog::~Catalog() {
  if (catalogDataSet != NULL) delete catalogDataSet;
}


int Catalog::catalog(char *dsName, char *volume) {
  struct CatalogEntry e;
  
  catalogDataSet->lock();

  if (catalogDataSet->point((long)0) < 0) { catalogDataSet->unlock(); return -1; } 
  while (catalogDataSet->get((unsigned char*)&e) >= 0) {
    if (e.isDeleted) {
      if (catalogDataSet->point(-1) < 0) { catalogDataSet->unlock(); return -1; }
      break;
    }
  }

  sprintf(e.dsName,"%s",dsName);
  sprintf(e.volume,"%s",volume);
  e.isDeleted = 0;
  if (catalogDataSet->put((unsigned char*)&e) < 0) { catalogDataSet->unlock(); return -1; }

  catalogDataSet->unlock();
  return 0;  
}


int Catalog::uncatalog(char *dsName) {
  struct CatalogEntry e;

  catalogDataSet->lock();  

  if (catalogDataSet->point((long)0) < 0) { catalogDataSet->unlock(); return -1; }
  while (catalogDataSet->get((unsigned char*)&e) >= 0) {
    if (!e.isDeleted && (strncmp(dsName,e.dsName,44) == 0)) {
      if (catalogDataSet->point(-1) < 0) { catalogDataSet->unlock(); return -1; }
      e.isDeleted = 1;
      if (catalogDataSet->put((unsigned char*)&e) < 0) { catalogDataSet->unlock(); return -1; }
      catalogDataSet->unlock();      
      return 0;
    }
  }

  catalogDataSet->unlock();  
  return 0;  
}


char *Catalog::getVolume(char *dsName) {
  struct CatalogEntry e;
  
  catalogDataSet->lock();  

  if (catalogDataSet->point((long)0) < 0) { catalogDataSet->unlock(); return NULL; }
  while (catalogDataSet->get((unsigned char*)&e) >= 0) {
    if (!e.isDeleted && (strncmp(dsName,e.dsName,44) == 0)) {
      catalogDataSet->unlock();  
      return e.volume;
    }
  }
  
  catalogDataSet->unlock();  
  return NULL;
}
