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

#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include "CardReader.h"
#include "JCLParser.h"
#include "SpoolingSystem.h"

using namespace std;


CardReader::CardReader(int inFd, int outFd) {
  this->inFd = inFd;
  this->outFd = outFd;
  inFile = fdopen(inFd,"rb");
  outFile = fdopen(outFd,"wb");
}


CardReader::~CardReader() {
  if (inFile != NULL) fclose(inFile);
  if (outFile != NULL) fclose(outFile);
}


int CardReader::readLine() {
  fgets(line,80,inFile);
  if (ferror(inFile) < 0) return -1; 
}


int CardReader::writeLine(char *line) {
  fputs(line,outFile);
  fflush(outFile);
  return ferror(outFile);
}


void CardReader::run() {
  int stop = 0, r = 0;
  JCLParser *jclParser = NULL;
  JobCard *job;
  char *cmd, jobId[9], line[81], *jobNamePtr = NULL, *jobIdPtr = NULL;

  jclParser = new JCLParser(inFile);  

  do {
    job = jclParser->parse();

    if (job != NULL) {
      if (SpoolingSystem::spoolingSystem->submit(job,jobId) < 0) {
        delete job;
        writeLine("ERROR DURING JOB SUBMISSION\n");
      } else {
        sprintf(line,"JOB %s SUBMITTED\n",jobId);
        writeLine(line);
      }
    }
  } while (!stop);

  close(inFd);
  if (inFd != outFd) {
      close(outFd);
  }

  delete jclParser;  
}