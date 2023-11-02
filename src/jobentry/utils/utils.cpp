/*******************************************************************************************/
/*   QWICS Server COBOL environment standard dataset service program replacements          */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 02.11.2023                                  */
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

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../card/DD.h"    
#include "./parser.h"    
#include "../env/envconf.h"
extern "C" {
#include "../../tpmserver/dataset/isamdb.h"
}

typedef struct {
    int len;
    int pos;
} keyDef;


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
    printf("IDCAMS started\n");
    int maxcc = 0;

    if (open("SYSIN",1,0,exec) < 0) {
        printf("MAXCC=12 Could not open SYSIN\n.");
        return 12;
    }

    char *tokens[MAX_TOKENS];
    int n;
    for (n = 0; n < MAX_TOKENS; n++) {
        tokens[n] = (char*)malloc(50);
    }
    int tokenNum = 0;
    char linebuf[256];
    char *isamDatasetDir;

    startIsamDB(GETENV_STRING(isamDatasetDir,"QWICS_DATASET_DIR","../dataset"));

    while ((get("SYSIN",(unsigned char*)&linebuf,exec) == 0) && (maxcc == 0)){
        linebuf[80] = 0x00;
        //printf("%s\n",linebuf);

        if (tokenize(linebuf,tokens,&tokenNum)) {
            continue;
        } else {
            int i;
            for (i = 0; i < tokenNum; i++) {
                printf("%s\n",tokens[i]);
            }

            if (tokenNum > 0) {
                if (strcmp(tokens[0],"DEFINE") == 0) {
                    if (getTokenIndex("CLUSTER",tokens,tokenNum) == 1) {
                        int idx1 = getTokenIndex("NAME",tokens,tokenNum);
                        if ((idx1 > 1) && (idx1+2 < tokenNum)) {
                            void *txptr = beginTransaction();
                            char *dsn = tokens[idx1+2];
                        
                            void *ds = openDataset(dsn);
                            if (ds != NULL) {
                                closeDataset(ds);

                                ds = openDataset("SYS.KSDS.KEYS");
                                if (ds != NULL) {
                                    idx1 = getTokenIndex("KEYS",tokens,tokenNum);
                                    if ((idx1 > 1) && (idx1+3 < tokenNum))  {
                                        keyDef key;
                                        key.len = atoi(tokens[idx1+2]);
                                        if (key.len <= 0) {
                                            key.len = 0;
                                        } 
                                        key.pos = atoi(tokens[idx1+3]);

                                        put(ds,txptr,NULL,(unsigned char*)dsn,strlen(dsn),(unsigned char*)&key,sizeof(key));
                                    }
                                    closeDataset(ds);
                                }
                            } else {
                                maxcc = 12;
                            }

                            if (endTransaction(txptr,1) != 0) {
                                maxcc = 12;
                            }
                        }
                    }
                }

                if (strcmp(tokens[0],"DELETE") == 0) {
                    if (tokenNum > 1) {
                        void *txptr = beginTransaction();
                        char *dsn = tokens[1];

                        void *ds = openDataset("SYS.KSDS.KEYS");
                        if (ds != NULL) {
                            del(ds,txptr,(unsigned char*)dsn,strlen(dsn));
                            closeDataset(ds);
                        }

                        removeDataset(dsn,txptr);

                        if (endTransaction(txptr,1) != 0) {
                            maxcc = 12;
                        }
                    }
                }

                if (strcmp(tokens[0],"REPRO") == 0) {
                    int idx1 = getTokenIndex("INFILE",tokens,tokenNum);
                    if (idx1 < 0) {
                        printf("MAXCC=12 Missing parameter INDD\n.");
                        maxcc = 12;
                    }                    
                }
            }
        }
    }

    for (n = 0; n < MAX_TOKENS; n++) {
        free(tokens[n]);
    }

    stopIsamDB();

    if (close("SYSIN",exec) < 0) {
        printf("MAXCC=12 Could not close SYSIN\n.");
        return 12;
    }

    if (maxcc > 0) {
        printf("MAXCC=%d error\n.",maxcc);
    }

    printf("IDCAMS finished\n");
    return maxcc;
}


int sdsf(JobCard *exec) {
    printf("SDSF started\n");
    return 0;
}
