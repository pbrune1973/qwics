/*******************************************************************************************/
/*   QWICS Server COBOL embedded SQL executor                                              */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 22.08.2023                                  */
/*                                                                                         */
/*   Copyright (C) 2018 - 2020 by Philipp Brune  Email: Philipp.Brune@qwics.org            */
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
#include <libcob.h>
#ifdef _IN_JOBENTRY_
#include "../jobentry/card/DD.h"    

extern JobCard *thisEXEC;
#endif


int open(char *ddName, int in, int out, FCD3 *fcd) {
    return 0;
}


int close(char *ddName) {
    return 0;
}

#ifdef _IN_JOBENTRY_
extern "C" 
#endif
int datasetfh(unsigned char *opcode, FCD3 *fcd) {
    if ((fcd->fileOrg == ORG_LINE_SEQ || fcd->fileOrg == ORG_SEQ ||
         fcd->fileOrg == ORG_RELATIVE) && 
        (fcd->accessFlags == ACCESS_SEQ || fcd->accessFlags == ACCESS_RANDOM)) {
        // Retrieve DDNAME
        char ddname[9];
        int l = (int)fcd->fnameLen[1];
        if (l > 8) {
            l = 8;
        }
        memcpy(ddname,fcd->fnamePtr,l);
        ddname[l] = 0x00;

        // Handle operation
        switch (LDCOMPX2(opcode)) {
            case OP_OPEN_INPUT :
                if (fcd->openMode != OPEN_NOT_OPEN) {
                    return -1;
                }
                if (open(ddname,1,0,fcd) < 0) {
                    return -1;
                }
                fcd->openMode = OPEN_INPUT;
                break;

            case OP_OPEN_OUTPUT :
                if (fcd->openMode != OPEN_NOT_OPEN) {
                    return -1;
                }
                if (open(ddname,0,1,fcd) < 0) {
                    return -1;
                }
                fcd->openMode = OPEN_OUTPUT;
                break;

            case OP_OPEN_IO :
                if (fcd->openMode != OPEN_NOT_OPEN) {
                    return -1;
                }
                if (open(ddname,1,1,fcd) < 0) {
                    return -1;
                }
                fcd->openMode = OPEN_IO;
                break;

            case OP_CLOSE :
                if (fcd->openMode == OPEN_NOT_OPEN) {
                    return -1;
                }
                if (close(ddname) < 0) {
                    return -1;
                }
                fcd->openMode = OPEN_NOT_OPEN;
                break;
        }
        printf("RecLen %d\n",LDCOMPX4(fcd->curRecLen));
        return 0;
    }
    if (fcd->fileOrg == ORG_INDEXED) {
        // Retrieve DDNAME
        char ddname[9];
        int l = (int)fcd->fnameLen[1];
        if (l > 8) {
            l = 8;
        }
        memcpy(ddname,fcd->fnamePtr,l);
        ddname[l] = 0x00;

        // Resolve DSNAME of VSAM file        
    } 
    return EXTFH(opcode,fcd);
}
