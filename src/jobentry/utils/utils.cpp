/*******************************************************************************************/
/*   QWICS Server COBOL environment standard dataset service program replacements          */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 28.09.2023                                  */
/*                                                                                         */
/*   Copyright (C) 2018 by Philipp Brune  Email: Philipp.Brune@hs-neu-ulm.de               */
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
#include <stdlib.h>
#include <unistd.h>

#include "../card/DD.h"    


int open(char *ddName, int in, int out, JobCard *exec) {
    printf("open %s\n",ddName);
    DD* dd = (DD*)exec->getSubCard(ddName);
    if (dd != NULL) {
        int mode = 0;
        if (out == 0) {
            mode = OPEN_RDONLY;
        } else {
            mode = OPEN_RDWR;            
        }
        DataSet *ds = dd->getDataSetDef()->open(mode);
        if (ds == NULL) {
            return -1;
        }
    }
    return 0;
}


int close(char *ddName, JobCard *exec) {
    printf("close %s\n",ddName);
    DD* dd = (DD*)exec->getSubCard(ddName);
    if (dd != NULL) {
        DataSet *ds = dd->getDataSetDef()->getDataSet();
        if (ds == NULL) {
            return -1;
        }
        delete ds;
    }
    return 0;
}


int put(char *ddName, unsigned char *data, JobCard *exec) {
    DD* dd = (DD*)exec->getSubCard(ddName);
    if (dd != NULL) {
        DataSet *ds = dd->getDataSetDef()->getDataSet();
        if (ds == NULL) {
            return -1;
        }
        if (ds->put(data) < 0) {
            return -1;
        }
    }
    return 0;
}


int get(char *ddName, unsigned char *data, JobCard *exec) {
    DD* dd = (DD*)exec->getSubCard(ddName);
    if (dd != NULL) {
        DataSet *ds = dd->getDataSetDef()->getDataSet();
        if (ds == NULL) {
            return -1;
        }
        if (ds->get(data) < 0) {
            return -1;
        }
    }
    return 0;
}


int idcams(JobCard *exec) {
    if (open("SYSIN",1,0,exec) < 0) {
        printf("MAXCC=12 Could not open SYSIN\n.");
        return 12;
    }



    if (close("SYSIN",exec) < 0) {
        printf("MAXCC=12 Could not close SYSIN\n.");
        return 12;
    }

    return 0;
}


int sdsf(JobCard *exec) {
    return 0;
}
