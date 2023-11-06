/*******************************************************************************************/
/*   QWICS Batch Job Entry System                                                          */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 06.11.2023                                  */
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

#ifndef _OutputWriter_h
#define _OutputWriter_h

#include "Initiator.h"
#include "CardReader.h"


class OutputWriter : public Initiator {
 private:
  CardReader *reader;
  char       *jobName;
  char       *jobId;
  int        keep;
  
 public:
  OutputWriter(JobClassQueue *queue, int stop);
  OutputWriter(JobClassQueue *queue, CardReader *reader, char *jobName, 
               char *jobId, int keep);
  virtual ~OutputWriter();
  
  virtual void run();
};

#endif
