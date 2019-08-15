/*******************************************************************************************/
/*   QWICS Server C langauge Preprocessor                                                       */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 15.08.2019                                  */
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
#include <string.h>
#include <ctype.h>
#include <unistd.h>

int sqlca = 0;
int mapNameMode = 0;
int isMapIO = 0;
int mapCmd = 0; // 1: RECEIVE, 0: SEND
int isBranchLabel = 0;
int isResponseParam = 0;
int isPtrField = 0;
int isLengthField = 0;
int isErrHandlerField = 0;
char respParam[9];
char mapName[9];
char mapsetName[9];
char lcopybookNames[20][9];
int numOfLCopybooks = 0;
int eibPresent = 0;
int outputDot = 1;
int isReturn = 0;
int isXctl = 0;
int commAreaPresent = 0;
int allowIntoParam = 0;
int allowFromParam = 0;
int varType = 0;
int varcount = 0;


void DISPLAY(char *buf,char*cmdstr, char *varname, FILE *f) {
  if (strcmp(";",cmdstr) == 0) {
    return;
  }
  if (strstr(cmdstr,"LEN") != NULL) {
      varType = 1;
  }
  if (strstr(cmdstr,"RESP") != NULL) {
      varType = 2;
  }
  sprintf(buf,"%s%d%s%s%s%s%s\n","cob_field f_",varcount," = {strlen(\"",cmdstr,"\")+5,\"TPMI:",cmdstr,"\",&a_4};");
  fputs(buf,f);
  varcount++;
  if (varname != NULL) {
    if (varType == 1) {
      sprintf(buf,"%s%d%s%s%s%s%s\n","cob_field f_",varcount," = {sizeof(",varname,"),(unsigned char*)&",varname,",&a_6};");
    } else if (varType == 2) {
      sprintf(buf,"%s%d%s%s%s%s%s\n","cob_field f_",varcount," = {sizeof(",varname,"),(unsigned char*)&",varname,",&a_3};");
    } else {
      if (strcmp("dfheiptr->eibaid",varname) == 0) {
        sprintf(buf,"%s%d%s%s%s%s%s\n","cob_field f_",varcount," = {sizeof(",varname,"),(unsigned char*)&",varname,",&a_4};");
      } else {
        sprintf(buf,"%s%d%s%s%s%s%s\n","cob_field f_",varcount," = {strlen(",varname,"),(unsigned char*)",varname,",&a_4};");
      }
    }
    fputs(buf,f);
    varType = 0;
    sprintf(buf,"%s%d%s%d%s\n","cob_display(0, 1, 2, &f_",(varcount-1),", &f_",varcount,");");
    fputs(buf,f);
    varcount++;
  } else {
    sprintf(buf,"%s%d%s\n","cob_display(0, 1, 1, &f_",(varcount-1),");");
    fputs(buf,f);
  }
}


// Load and insert copybook content
int includeCbk(char *copybook, FILE *outFile) {
    char path[255];
    sprintf(path,"%s%s%s","../copybooks/",copybook,".h");
    FILE *cbk = fopen(path, "r");
    if (cbk == NULL) {
        printf("%s%s%s\n","Info: Copybook file not found: ",path," - Looking for I/O suffix files.");
        return -1;
    }

    char line[255];
    int inExec = 0;
    while (fgets(line, 255, (FILE*)cbk) != NULL) {
        if (strstr(line,"SQLCODE")) {
            sqlca = 1;
        }
        if (!eibPresent && strstr(line,"eibcalen")) {
            eibPresent = 1;
        }
        // Avoid inserting other EXEC-Macros
        if (!inExec && strstr(line,"EXEC")) {
            inExec = 1;
        }
        if (!inExec) {
            fputs(line,outFile);
        }
        if (inExec && strstr(line,";")) {
            inExec = 0;
        }
    }

    fclose(cbk);
    return 1;
}


int includeCbkIO(char *copybook, FILE *outFile) {
    char path[255];
    sprintf(path,"%s%s%s","../copybooks/",copybook,"I.h");
    FILE *cbk = fopen(path, "r");
    if (cbk == NULL) {
        printf("%s%s\n","Copybook file not found: ",path);
        return -1;
    }

    char line[255];
    while (fgets(line, 255, (FILE*)cbk) != NULL) {
        fputs(line,outFile);
    }

    fclose(cbk);

    sprintf(path,"%s%s%s","../copybooks/",copybook,"O.h");
    cbk = fopen(path, "r");
    if (cbk == NULL) {
        printf("%s%s\n","Copybook file not found: ",path);
        return -1;
    }

    while (fgets(line, 255, (FILE*)cbk) != NULL) {
        fputs(line,outFile);
    }

    fclose(cbk);

    if (numOfLCopybooks < 20) {
        sprintf(lcopybookNames[numOfLCopybooks],"%s",copybook);
        numOfLCopybooks++;
    }
    return 1;
}


int includeMapDisplays(char *mapset, char *map, FILE *outFile, int input) {
    char path[255];
    if (input) {
        sprintf(path,"%s%s%s","../copybooks/",mapset,"I.dsp");
    } else {
        sprintf(path,"%s%s%s","../copybooks/",mapset,"O.dsp");
    }
    FILE *cbk = fopen(path, "r");
    if (cbk == NULL) {
        printf("%s%s\n","Copybook file not found: ",path);
        return -1;
    }

    if (input) { // Read in attention identifier value for RECEIVE
        char buf[80];
        DISPLAY(buf,"EIBAID","dfheiptr->eibaid",outFile);
    }

    int outOn = 0;
    char line[255];
    while (fgets(line, 255, (FILE*)cbk) != NULL) {
        if ((line[0] == '*') && (outOn == 1)) {
            outOn = 2;
        }
        if (outOn == 1) {
            fputs(line,outFile);
        }
        if ((line[0] == '*') && (outOn == 0) && (strstr(line,map) != NULL)) {
            outOn = 1;
        }
    }

    fclose(cbk);
    return 1;
}


// Process EXEC ... END-EXEC statement line in the data division
void processDataExecLine(char *buf, FILE *fp2) {
    char execbuf[255];
    int i,m = strlen(buf);
    char token[255];
    int tokenPos = 0;
    int include = 0;
    for (i = 0; i < m; i++) {
        if ((buf[i] == ' ') || (buf[i] == '\n') ||
            (buf[i] == '\r') || (buf[i] == '.') ||
            (buf[i] == ',') || (buf[i] == ';')) {
            if (tokenPos > 0) {
                token[tokenPos] = 0x00;
                if ((strstr(token,"INCLUDE") != NULL) || (strstr(token,"COPY") != NULL)) {
                    include = 1;
                } else {
                    if (include) {
                        if (includeCbk(token,fp2) < 0) {
                            includeCbkIO(token,fp2);
                        }
                    }
                    include = 0;
                }
                tokenPos = 0;
            }
        } else {
            token[tokenPos] = toupper(buf[i]);
            tokenPos++;
        }
    }
}


// Process EXEC ... END-EXEC statement line in the procedure division
void processExecLine(int execCmd, char *buf, FILE *fp2) {
    char execbuf[255];
    int i,m = strlen(buf);
    char token[255];
    int tokenPos = 0;
    int value = 0;
    int verbatim = 0;
    for (i = 0; i < m; i++) {
        if ((buf[i] == '\"') && !verbatim) {
            buf[i] = '\'';
            if (tokenPos > 0) {
                token[tokenPos] = 0x00;
                DISPLAY(execbuf,token,NULL,fp2);
                tokenPos = 0;
            }
            verbatim = 1;
            token[tokenPos] = buf[i];
            tokenPos++;
            continue;
        }
        if ((buf[i] == '\"') && verbatim) {
            buf[i] = '\'';
            token[tokenPos] = buf[i];
            tokenPos++;
            token[tokenPos] = 0x00;
            if (mapNameMode == 0) {
                DISPLAY(execbuf,token,NULL,fp2);
            }
            if (mapNameMode == 1) {
                token[strlen(token)-1] = 0x00;
                sprintf(mapName,"%s",token+1);
                char cmd[80];
                sprintf(cmd,"%s%s","MAP=",token);
                DISPLAY(execbuf,cmd,NULL,fp2);
                mapNameMode = 0;
            }
            if (mapNameMode == 2) {
                token[strlen(token)-1] = 0x00;
                sprintf(mapsetName,"%s",token+1);
                mapNameMode = 0;
                char cmd[80];
                sprintf(cmd,"%s%s","MAPSET=",token);
                DISPLAY(execbuf,cmd,NULL,fp2);
                includeMapDisplays(mapsetName,mapName,(FILE*)fp2,mapCmd);
            }
            tokenPos = 0;
            verbatim = 0;
            continue;
        }
        if ((buf[i] != '\"') && verbatim) {
            token[tokenPos] = buf[i];
            tokenPos++;
            continue;
        }

        if (execCmd == 1) {  // EXEC CICS
            if (buf[i] == '(') {
                i++;
                while (buf[i] == ' ') {
                  i++;
                }
                i--;

                if (tokenPos > 0) {
                    token[tokenPos] = 0x00;
                    if (strstr(token,"QIDERR") != NULL) {
                        isErrHandlerField = 44;
                    }
                    if (strstr(token,"LENGERR") != NULL) {
                        isErrHandlerField = 22;
                    }
                    if (strstr(token,"INVREQ") != NULL) {
                        isErrHandlerField = 16;
                    }
                    if (strstr(token,"ENQBUSY") != NULL) {
                        isErrHandlerField = 55;
                    }
                    if (strstr(token,"CWA") != NULL) {
                        isPtrField = 1;
                    }
                    if (strstr(token,"TWA") != NULL) {
                        isPtrField = 1;
                    }
                    if (strstr(token,"TCTUA") != NULL) {
                        isPtrField = 1;
                    }
                    if (strstr(token,"EIB") != NULL) {
                        isPtrField = 1;
                    }
                    if (strstr(token,"SET") != NULL) {
                        isPtrField = 1;
                    }
                    if (strstr(token,"DATA") != NULL) {
                        isPtrField = 1;
                    }
                    if (strstr(token,"DATAPOINTER") != NULL) {
                        isPtrField = 1;
                    }
                    if (strstr(token,"LENGTH") != NULL) {
                        isLengthField = 1;
                    }
                    if (strstr(token,"FLENGTH") != NULL) {
                        isLengthField = 1;
                    }
                    if (strstr(token,"RECEIVE") != NULL) {
                        mapCmd = 1;
                        allowIntoParam = 1;
                    }
                    if (strstr(token,"SEND") != NULL) {
                        mapCmd = 0;
                    }
                    if (strstr(token,"RETRIEVE") != NULL) {
                        allowIntoParam = 1;
                    }
                    if (strstr(token,"GET") != NULL) {
                        allowIntoParam = 1;
                    }
                    if (strstr(token,"PUT") != NULL) {
                        allowFromParam = 1;
                    }
                    if (strstr(token,"READQ") != NULL) {
                        allowIntoParam = 1;
                    }
                    if (strstr(token,"WRITEQ") != NULL) {
                        allowFromParam = 1;
                    }
                    if (strstr(token,"MAP") != NULL) {
                        mapNameMode = 1;
                    }
                    if (strstr(token,"MAPSET") != NULL) {
                        mapNameMode = 2;
                    }
                    if (!allowFromParam && (strstr(token,"FROM") != NULL)) {
                        isMapIO = 1;
                    }
                    if (!allowIntoParam && (strstr(token,"INTO") != NULL)) {
                        isMapIO = 1;
                    }
                    if (strstr(token,"ERROR") != NULL) {
                        isBranchLabel = 1;
                    }
                    if (strstr(token,"MAPFAIL") != NULL) {
                        isBranchLabel = 1;
                    }
                    if (strstr(token,"NOTFND") != NULL) {
                        isBranchLabel = 1;
                    }
                    if (strcmp(token,"RESP") == 0) {
                        isResponseParam = 1;
                        sprintf(respParam,"%s",token);
                    }
                    if (strcmp(token,"RESP2") == 0) {
                        isResponseParam = 1;
                        sprintf(respParam,"%s",token);
                    }
                    if (!isResponseParam) {
                        DISPLAY(execbuf,token,NULL,fp2);
                    }
                    tokenPos = 0;

                    if (isErrHandlerField > 0) {
                        int para = 0;
                        do {
                          i++;
                          if ((buf[i] != ' ') && (buf[i] != ')')) {
                            token[tokenPos] = buf[i];
                            tokenPos++;
                          } else {
                            if (tokenPos > 0) {
                              token[tokenPos] = 0x00;
                              tokenPos = 0;
                              para = 1;
                            }
                          }
                        } while (buf[i] != ')');

                        if (para == 1) {
                          sprintf(execbuf,"%s\n","if (setjmp(&QWICSJMP) > 0) {");
                          fputs(execbuf, (FILE*)fp2);
                          sprintf(execbuf,"%s%s%s\n","  goto ",token,";");
                          fputs(execbuf, (FILE*)fp2);
                          sprintf(execbuf,"%s\n","} else {");
                          fputs(execbuf, (FILE*)fp2);
                          sprintf(execbuf,"%s%d%s\n","  setJmpAbend(",isErrHandlerField,",(char*)&QWICSJMP);");
                          fputs(execbuf, (FILE*)fp2);
                          sprintf(execbuf,"%s\n","}");
                          fputs(execbuf, (FILE*)fp2);
                        } else {
                          DISPLAY(execbuf,"",token,fp2);
                        }
                        isErrHandlerField = 0;
                        tokenPos = 0;
                    }

                    if (isPtrField) {
                        int adrOf = 0;
                        do {
                          i++;
                          if ((buf[i] != ' ') && (buf[i] != ')')) {
                            token[tokenPos] = buf[i];
                            tokenPos++;
                          } else {
                            if (tokenPos > 0) {
                              token[tokenPos] = 0x00;
                              tokenPos = 0;
                            }
                            if ((adrOf == 0) && (strcmp(token,"ADDRESS") == 0)) {
                                adrOf = 1;
                            }
                            if ((adrOf == 1) && (strcmp(token,"OF") == 0)) {
                                adrOf = 2;
                            }
                          }
                        } while (buf[i] != ')');

                        if (adrOf == 2) {
                          DISPLAY(execbuf,"","QWICSPTR",fp2);
                          sprintf(execbuf,"%s%s\n",token,"=QWICSPTR;");
                          fputs(execbuf, (FILE*)fp2);
                        } else {
                          DISPLAY(execbuf,"",token,fp2);
                        }
                        isPtrField = 0;
                        tokenPos = 0;
                    }

                    if (isLengthField) {
                        int lenOf = 0;
                        do {
                          i++;
                          if ((buf[i] != ' ') && (buf[i] != '(') && (buf[i] != ')')) {
                            token[tokenPos] = buf[i];
                            tokenPos++;
                          } else {
                            if (tokenPos > 0) {
                              token[tokenPos] = 0x00;
                              tokenPos = 0;
                            }
                            if ((lenOf == 0) && (strcmp(token,"strlen") == 0)) {
                                lenOf = 1;
                            }
                          }
                        } while (buf[i] != ')');

                        if (lenOf == 1) {
                          sprintf(execbuf,"%s%s%s\n","QWICSLEN = strlen(",token,");");
                          fputs(execbuf, (FILE*)fp2);
                          sprintf(execbuf,"%s\n","QWICSLEN = COB_BSWAP_64(QWICSLEN);");
                          fputs(execbuf, (FILE*)fp2);
                          DISPLAY(execbuf,"","QWICSLEN",fp2);
                        } else {
                          DISPLAY(execbuf,"",token,fp2);
                        }
                        isLengthField = 0;
                        tokenPos = 0;
                    }
                }
                value = 1;
            } else {
                if ((buf[i] == ' ') || (buf[i] == '\n') ||
                    (buf[i] == '\r') || (buf[i] == ')') ||
                    (buf[i] == ';')) {
                    if (tokenPos > 0) {
                        token[tokenPos] = 0x00;
                        if (value == 1) {
                            if (isMapIO) {
                                // Insert map attribute display statements
                                if (token[strlen(token)-1] == 'I') {
                                    includeMapDisplays(mapsetName,mapName,(FILE*)fp2,1);
                                } else {
                                    includeMapDisplays(mapsetName,mapName,(FILE*)fp2,0);
                                }
                                isMapIO = 0;
                            } else
                            if (isBranchLabel) {
                                DISPLAY(execbuf,token,NULL,fp2);
                                isBranchLabel = 0;
                            } else
                            if (isResponseParam) {
                                DISPLAY(execbuf,respParam,token,fp2);
                                isResponseParam = 0;
                            } else {
                                DISPLAY(execbuf,"",token,fp2);
                            }
                        } else {
                            while (buf[i] == ' ') {
                              i++;
                            }
                            if (buf[i] == '(') {
                              i--;
                              continue;
                            }
                            i--;

                            if (strstr(token,"RECEIVE") != NULL) {
                                mapCmd = 1;
                                allowIntoParam = 1;
                            }
                            if (strstr(token,"SEND") != NULL) {
                                mapCmd = 0;
                            }
                            if (strstr(token,"RETRIEVE") != NULL) {
                                allowIntoParam = 1;
                            }
                            if (strstr(token,"GET") != NULL) {
                                allowIntoParam = 1;
                            }
                            if (strstr(token,"PUT") != NULL) {
                                allowFromParam = 1;
                            }
                            if (strstr(token,"READQ") != NULL) {
                                allowIntoParam = 1;
                            }
                            if (strstr(token,"WRITEQ") != NULL) {
                                allowFromParam = 1;
                            }
                            if (strstr(token,"MAP") != NULL) {
                                mapNameMode = 1;
                            }
                            if (strstr(token,"MAPSET") != NULL) {
                                mapNameMode = 2;
                            }
                            if (!allowFromParam && (strstr(token,"FROM") != NULL)) {
                                isMapIO = 1;
                            }
                            if (!allowIntoParam && (strstr(token,"INTO") != NULL)) {
                                isMapIO = 1;
                            }
                            if (strstr(token,"ERROR") != NULL) {
                                isBranchLabel = 1;
                            }
                            if (strstr(token,"MAPFAIL") != NULL) {
                                isBranchLabel = 1;
                            }
                            if (strstr(token,"NOTFND") != NULL) {
                                isBranchLabel = 1;
                            }
                            if (strstr(token,"RETURN") != NULL) {
                                isReturn = 1;
                            }
                            if (strstr(token,"XCTL") != NULL) {
                                isXctl = 1;
                            }
                            if (strcmp(token,"RESP") == 0) {
                                isResponseParam = 1;
                                sprintf(respParam,"%s",token);
                            }
                            if (strcmp(token,"RESP2") == 0) {
                                isResponseParam = 1;
                                sprintf(respParam,"%s",token);
                            }

                            if (!isResponseParam) {
                              DISPLAY(execbuf,token,NULL,fp2);
                            }
                        }
                        tokenPos = 0;
                    }
                    value = 0;
                } else {
                    token[tokenPos] = buf[i];
                    tokenPos++;
                }
            }
        }
        if (execCmd == 2) {  // EXEC SQL
            if (buf[i] == ':') {
                if (tokenPos > 0) {
                    token[tokenPos] = 0x00;
                    DISPLAY(execbuf,token,NULL,fp2);
                    tokenPos = 0;
                }
                value = 1;
            } else {
                if ((buf[i] == ' ') || (buf[i] == '\n') ||
                    (buf[i] == '\r') || (buf[i] == ';')) {
                    if (tokenPos > 0) {
                        token[tokenPos] = 0x00;
                        if (value == 1) {
                          DISPLAY(execbuf,"",token,fp2);
                        } else {
                          DISPLAY(execbuf,token,NULL,fp2);
                        }
                        tokenPos = 0;
                    }
                    value = 0;
                } else {
                    if ((buf[i] == ',') || (buf[i] == '=') || (buf[i] == '(') || (buf[i] == ')') ||
                        (buf[i] == '>') || (buf[i] == '<') || (buf[i] == '*')) {
                        if (tokenPos > 0) {
                            token[tokenPos] = 0x00;
                            if (value == 1) {
                              DISPLAY(execbuf,"",token,fp2);
                            } else {
                              DISPLAY(execbuf,token,NULL,fp2);
                            }
                            tokenPos = 0;
                        }
                        value = 0;
                        token[0] = buf[i];
                        token[1] = 0x00;
                        DISPLAY(execbuf,token,NULL,fp2);
                    } else {
                        token[tokenPos] = buf[i];
                        tokenPos++;
                    }
                }
            }
        }
    }
}


int hasDotTerminator(char *buf) {
    int l = strlen(buf);
    int i = l-1;
    while ((buf[i] == ' ') || (buf[i] == 10) || (buf[i] == 13)) {
        i--;
    }
    if (buf[i] == '.') {
        return 1;
    }
    return 0;
}


int main(int argc, char **argv) {
   FILE *fp,*fp2;
   char buf[255],oname[255];

   buf[0] = 0x00;

   if (argc < 2) {
	    printf("%s\n","Usage: cprep <C-File>");
	    return -1;
   }

   fp = fopen(argv[1], "r");
   if (fp == NULL) {
	    printf("%s%s\n","No input file: ",argv[1]);
	    return -1;
   }

   char *progname = argv[1];
   int i,l = strlen(progname);
   if (l > 8) l = 8;
   for (i = 0; i < l; i++) {
     if (progname[i] == '.') {
       progname[i] = 0x00;
       break;
     }
     progname[i] = toupper(progname[i]);
   }
   progname[i] = 0x00;

   sprintf(oname,"%s%s%s","exec_",progname,".c");
   fp2 = fopen(oname,"w");
   if (fp2 == NULL) {
        printf("%s%s\n","Could not create output file: ",oname);
        return -1;
   }

   int execCmd = 0;
   int inProcDivision = 0;
   int startProcDivision = 0;
   int commAreaPresent = 0;

   while (fgets(buf, 255, (FILE*)fp) != NULL) {
       if (!commAreaPresent && strstr(buf,"pCOMM_AREA") != NULL) {
         commAreaPresent = 1;
       }
       if (strstr(buf," main(") != NULL) {
          fputs("#define COB_KEYWORD_INLINE __inline\n",(FILE*)fp2);
          fputs("#include <libcob.h>\n",(FILE*)fp2);
          fputs("#include <setjmp.h>\n\n",(FILE*)fp2);
          includeCbk("DFHEIBLK",(FILE*)fp2);
          fputs("unsigned char dfheiblkbuf[200];\n",(FILE*)fp2);
          fputs("cob_field_attr a_1 =       {0x21,   0,   0, 0x1000, NULL};\n",(FILE*)fp2);
          fputs("cob_field_attr a_2 =       {0x01,   0,   0, 0x0000, NULL};\n",(FILE*)fp2);
          fputs("cob_field_attr a_3 =       {0x11,   4,   0, 0x0821, NULL};\n",(FILE*)fp2);
          fputs("cob_field_attr a_4 =       {0x21,   0,   0, 0x0000, NULL};\n",(FILE*)fp2);
          fputs("cob_field_attr a_5 =       {0x12,   7,   0, 0x0001, NULL};\n",(FILE*)fp2);
          fputs("cob_field_attr a_6 =       {0x11,   8,   0, 0x0821, NULL};\n",(FILE*)fp2);
          fputs("cob_field_attr a_7 =       {0x10,   2,   0, 0x1000, NULL};\n",(FILE*)fp2);
          fputs("cob_field_attr a_8 =       {0x11,   9,   0, 0x0821, NULL};\n",(FILE*)fp2);
          fputs("void *QWICSPTR;\n",(FILE*)fp2);
          fputs("long QWICSLEN;\n",(FILE*)fp2);
          fputs("jmp_buf QWICSJMP;\n",(FILE*)fp2);
          fputs("jmp_buf RETURNJMP;\n",(FILE*)fp2);

          while (strstr(buf,"{") == NULL) {
            fgets(buf, 255, (FILE*)fp);
          }

          inProcDivision = 1;
          startProcDivision = 1;

          sprintf(buf,"%s%s%s\n","int ",progname,"(cob_u8_t *captr) {");
          fputs(buf,(FILE*)fp2);
          fputs("dfheiptr = (DFHEIBLK*)&dfheiblkbuf;\n",(FILE*)fp2);
//          fputs("cob_field SQLCODE = {sizeof(dfheiptr->eibaid),&dfheiptr->eibaid,&a_2};\n",(FILE*)fp2);
          fputs("if (setjmp(RETURNJMP) > 0) { return 0; }\n",(FILE*)fp2);
          buf[0] = 0x00;
       }

       if (execCmd == 0) {
           char *cmd = strstr(buf,"EXEC");
           if ((cmd != NULL) && strstr(buf,"CICS")) {
               allowIntoParam = 0;
               allowFromParam = 0;
               execCmd = 1;
               isReturn = 0;
               isXctl = 0;
           }
           if ((cmd != NULL) && strstr(buf,"SQL")) {
               allowIntoParam = 0;
               allowFromParam = 0;
               execCmd = 2;
           }
       }

       if (execCmd == 0) {
           if (!inProcDivision && (strstr(buf,"  COPY ") != NULL)) {
               processDataExecLine(buf,fp2);
           } else {
               fputs(buf,(FILE*)fp2);

               if (startProcDivision) {
                   if (sqlca) {
                       char buf[80];
                       DISPLAY(buf,"SET SQLCODE","SQLCODE",fp2);
                   }
                   DISPLAY(buf,"SET EIBCALEN","dfheiptr->eibcalen",fp2);
                   DISPLAY(buf,"SET EIBAID","dfheiptr->eibaid",fp2);

                   startProcDivision = 0;
               }
           }
       } else {
           if (inProcDivision) {
               processExecLine(execCmd,buf,fp2);
           } else {
               processDataExecLine(buf,fp2);
           }

           char *cmd = strstr(buf,";");
           if (cmd != NULL) {
               char buf2[80];
               DISPLAY(buf2,"END-EXEC.",NULL,fp2);

               execCmd = 0;
               if (isReturn || isXctl) {
                   char gb[30];
                   sprintf(gb,"%s\n","  longjmp(RETURNJMP,1);");
                   fputs(gb,(FILE*)fp2);
                   isReturn = 0;
                   isXctl = 0;
               }
           }
       }
  }

  fclose(fp);
  fclose(fp2);
  return 0;

  // argv[0| = "cobc";
  // return execv("cobc",argv);
}
