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
#include <iostream>
#include "OutputClassQueue.h"

using namespace std;


OutputClassQueue::OutputClassQueue(char *spoolDir, char *name, int memQueued, 
                                   int switchLimit) : 
                  JobClassQueue(spoolDir,name,memQueued,switchLimit)
{
}


OutputClassQueue::~OutputClassQueue() {
}


JobInfo OutputClassQueue::loadJob(char *name) {
 JobInfo job;
 FILE *jf = NULL;

 if ((jf = fopen(getJobFileName(name),"r")) != NULL) {
   fseek(jf,0L,SEEK_SET);
   fread(&job,sizeof(JobInfo),1,jf);
   fseek(jf,(long)sizeof(JobInfo),SEEK_SET);
   job.fileName = new char[256];
   fgets(job.fileName,255,jf);
   job.fileName[strlen(job.fileName)-1] = 0x00;
   JCLParser *p = new JCLParser(jf);
   job.params = NULL;
   try {
     p->getNextValidLine();
     job.params = p->parseParameters();
   } catch(int e) {
   }
   if (job.params == NULL) {
     job.params = new Parameters();
   }
   delete p;   
   fclose(jf);
 }

 return job;   
}

  
int OutputClassQueue::saveJob(JobInfo job) {
 FILE *jf = NULL;

 if ((jf = fopen(getJobFileName(job.jobId),"w")) != NULL) {
   fwrite(&job,sizeof(JobInfo),1,jf);
   fseek(jf,(long)sizeof(JobInfo),SEEK_SET);
   fputs(job.fileName,jf);
   fputs("\n",jf);
   job.params->print(jf,1);
   fclose(jf);
   if (ferror(jf) != 0) return -1;
 } else {
   return -1;
 }

 return 0;   
}
