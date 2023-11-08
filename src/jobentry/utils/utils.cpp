/*******************************************************************************************/
/*   QWICS Server COBOL environment standard dataset service program replacements          */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 07.11.2023                                  */
/*                                                                                         */
/*   Copyright (C) 2018-2023 by Philipp Brune  Email: Philipp.Brune@qwics.de               */
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
    int recl;
    int len;
    int pos;
} keyDef;

char *isamDatasetDir = NULL;


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

    startIsamDB(GETENV_STRING(isamDatasetDir,"QWICS_DATASET_DIR","../dataset"));

    while ((get("SYSIN",(unsigned char*)&linebuf,exec) == 0) && (maxcc == 0)){
        linebuf[80] = 0x00;
        //printf("%s\n",linebuf);

        if (tokenize(linebuf,tokens,&tokenNum)) {
            continue;
        } else {
            int i;
/*
            for (i = 0; i < tokenNum; i++) {
                printf("%s\n",tokens[i]);
            }
*/
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
                                    int recl = 80;
                                    idx1 = getTokenIndex("RECORDSIZE",tokens,tokenNum);
                                    if ((idx1 > 1) && (idx1+3 < tokenNum))  {
                                        recl = atoi(tokens[idx1+3]);
                                        if (recl <= 0) {
                                            recl = 80;
                                        }
                                    }
                                    
                                    idx1 = getTokenIndex("KEYS",tokens,tokenNum);
                                    if ((idx1 > 1) && (idx1+3 < tokenNum))  {
                                        keyDef key;
                                        key.recl = recl;
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
                        idx1 = getTokenIndex("IFILE",tokens,tokenNum);
                    }
                    if (idx1 < 0) {
                        printf("MAXCC=12 Missing parameter INFILE\n.");
                        maxcc = 12;
                    }                    

                    int idx2 = getTokenIndex("OUTFILE",tokens,tokenNum);
                    if (idx2 < 0) {
                        idx2 = getTokenIndex("OFILE",tokens,tokenNum);
                    }
                    if (idx2 < 0) {
                        printf("MAXCC=12 Missing parameter OUTFILE\n.");
                        maxcc = 12;
                    }                    

                    if ((idx1 > 0) && (idx1+2 < tokenNum) && (idx2 > 0) && (idx2+2 < tokenNum)) {
                        DD *dd1 = (DD*)exec->getSubCard(tokens[idx1+2]);
                        DD *dd2 = (DD*)exec->getSubCard(tokens[idx2+2]);
                        char *dsn1 = NULL, *dsn2 = NULL;

                        if (dd1 != NULL && dd2 != NULL) {
                            dsn1 = dd1->getDataSetDef()->getDsn();
                            dsn2 = dd2->getDataSetDef()->getDsn();
                        }

                        if (dsn1 != NULL && dsn2 != NULL) {
                            void *txptr = beginTransaction();

                            void *ds = openDataset("SYS.KSDS.KEYS");
                            if (ds != NULL) {
                                int isVSAM1 = 1, isVSAM2 = 1;
                                keyDef key1;
                                if (get(ds,txptr,NULL,(unsigned char*)dsn1,strlen(dsn1),(unsigned char*)&key1,sizeof(key1),MODE_SET) != 0) {
                                    isVSAM1 = 0;
                                }
                                keyDef key2;
                                if (get(ds,txptr,NULL,(unsigned char*)dsn2,strlen(dsn2),(unsigned char*)&key2,sizeof(key2),MODE_SET) != 0) {
                                    isVSAM2 = 0;
                                }

                                closeDataset(ds);
cout << "REPRO " << isVSAM1 << " " << isVSAM2 << endl;

                                DataSet *ds1 = NULL, *ds2 = NULL;
                                void *vds1 = NULL, *vds2 = NULL;
                                unsigned char *data = NULL;
                                unsigned char *data2 = NULL;
                                int l1 = 0, l2 = 0;

                                if (isVSAM1) {
                                    vds1 = openDataset(dsn1);
                                    if (vds1 == NULL) {
                                        printf("MAXCC=12 Could not open infile dataset.\n");
                                        maxcc = 12;
                                    } else {
                                        l1 = key1.recl;
                                        data = (unsigned char*)malloc(key1.recl);
                                        if (data == NULL) {
                                            closeDataset(vds1);
                                            vds1 = NULL;
                                        }                                        
                                    }
                                } else {
                                    ds1 = dd1->getDataSetDef()->open(OPEN_RDONLY);
                                    if (ds1 == NULL) {
                                        printf("MAXCC=12 Could not open infile dataset.\n");
                                        maxcc = 12;
                                    } else {
                                        l1 = ds1->getRecSize();
                                        data = (unsigned char*)malloc(ds1->getRecSize());
                                        if (data == NULL) {
                                            delete ds1;
                                            ds1 = NULL;
                                        }
                                    }
                                }

                                if (isVSAM2) {
                                    vds2 = openDataset(dsn2);
                                    if (vds2 == NULL) {
                                        printf("MAXCC=12 Could not open outfile dataset.\n");
                                        maxcc = 12;
                                    } else {
                                        l2 = key2.recl;
                                        data2 = (unsigned char*)malloc(key2.recl);
                                        if (data2 == NULL) {
                                            closeDataset(vds2);
                                            vds2 = NULL;
                                        }                                        
                                    }
                                } else {
                                    ds2 = dd2->getDataSetDef()->open(OPEN_RDWR);
                                    if (ds2 == NULL) {
                                        printf("MAXCC=12 Could not open outfile dataset.\n");
                                        maxcc = 12;
                                    } else {
                                        l2 = ds2->getRecSize();
                                        data2 = (unsigned char*)malloc(ds2->getRecSize());
                                        if (data2 == NULL) {
                                            delete ds2;
                                            ds2 = NULL;
                                        }
                                    }
                                }

                                int n = l1;
                                if (l2 < n) {
                                    n = l2;
                                }

                                int hasNext = 1;
                                while (hasNext) {
                                    if (isVSAM1) {

                                    } else {
                                        if (ds1 != NULL) {
                                            if (ds1->get(data) < 0) {
                                               hasNext = 0;
                                               continue;
                                            }
                                        } else {
                                            hasNext = 0;
                                            continue;
                                        }
                                    }

                                    memcpy(data2,data,n);
                                    
                                    char line[73];
                                    memcpy(line,data,72);
                                    line[72] = 0x00;
                                    printf("%s\n",line);

                                    if (isVSAM2) {
                                        if (vds2 != NULL) {
                                           if (put(vds2,txptr,NULL,&data2[key2.pos],key2.len,data2,n) != 0) {
                                                printf("MAXCC=12 Could not write record to outfile dataset.\n");
                                                maxcc = 12;
                                                break;
                                            }
                                        }
                                    } else {
                                        if (ds2 != NULL) {
                                            if (ds2->put(data2) < 0) {
                                                printf("MAXCC=12 Could not write record to outfile dataset.\n");
                                                maxcc = 12;
                                                break;
                                            }
                                        }
                                    }
                                }

                                if (isVSAM1) {
                                    if (vds1 != NULL) {
                                        free(data);
                                        closeDataset(vds1);
                                    }                                        
                                } else {
                                    if (ds1 != NULL) {
                                        free(data);
                                        delete ds1;  
                                    }                                        
                                }

                                if (isVSAM2) {
                                    if (vds2 != NULL) {
                                        free(data2);
                                        closeDataset(vds2);
                                    }                                        
                                } else {
                                    if (ds2 != NULL) {
                                        free(data2);
                                        delete ds2;  
                                    }                                        
                                }
                            }

                            if (maxcc == 0) {
                                if (endTransaction(txptr,1) != 0) {
                                    maxcc = 12;
                                }
                            } else {
                                if (endTransaction(txptr,0) != 0) {
                                    maxcc = 12;
                                }
                            }
                        } else {
                            printf("MAXCC=12 Missing or wrong DD statements.\n");
                            maxcc = 12;
                        }
                    } else {
                        printf("MAXCC=12 Missing or wrong DD statements.\n");
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


int iebgener(JobCard *exec) {
    printf("IEBGENER started\n");
    int maxcc = 0;
    DD *dd1 = (DD*)exec->getSubCard("SYSUT1");
    DD *dd2 = (DD*)exec->getSubCard("SYSUT2");

    DataSet *ds1 = dd1->getDataSetDef()->open(OPEN_RDONLY);
    if (ds1 == NULL) {
        printf("MAXCC=12 Could not open SYSUT1 dataset.\n");
        maxcc = 12;
    } 

    DataSet *ds2 = dd2->getDataSetDef()->open(OPEN_RDWR);
    if (ds2 == NULL) {
        printf("MAXCC=12 Could not open SYSUT2 dataset.\n");
        maxcc = 12;
    } 

    if (ds1 != NULL & ds2 != NULL) {
        int n = ds1->getRecSize();
        if (ds2->getRecSize() < n) {
            n = ds2->getRecSize();
        }
            
        unsigned char *data = (unsigned char*)malloc(ds1->getRecSize());
        unsigned char *data2 = (unsigned char*)malloc(ds2->getRecSize());

        if (data != NULL && data2 != NULL) {
            int hasNext = 1;
            while (hasNext) {
                if (ds1->get(data) < 0) {
                    hasNext = 0;
                    continue;
                } 
                memcpy(data2,data,n);
                char line[73];
                memcpy(line,data,72);
                line[72] = 0x00;
                printf("%s\n",line);
                ds2->put(data2);
            }

            free(data);
            free(data2);
        }

        delete ds1;         
        delete ds2;     
    }

    printf("IEBGENER finished\n");
    return maxcc;
}
