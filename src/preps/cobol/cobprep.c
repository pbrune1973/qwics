/*******************************************************************************************/
/*   QWICS Server COBOL Preprocessor                                                       */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 09.08.2019                                  */
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
int inLinkageSection = 0;


struct linkageVarDef {
    char name[33];
    int isGroup;
    int level;
} linkageVars[255];

int numOfLinkageVars = 0;


void parseLinkageVarDef(char *line) {
    char lbuf[4];
    int len = strlen(line);
    if ((line[6] != '*') && (line[6] != '-')) {
        int pos = 7;
        while ((line[pos] == ' ') && (pos < len)) pos++;

        int i = 0;
        while ((line[pos] != ' ') && (pos < len) && (i < 3)) {
            lbuf[i] = line[pos];
            pos++;
            i++;
        }
        lbuf[i] = 0x00;
        linkageVars[numOfLinkageVars].level = atoi(lbuf);

        if (numOfLinkageVars > 0) {
            if (linkageVars[numOfLinkageVars].level > linkageVars[numOfLinkageVars-1].level) {
                linkageVars[numOfLinkageVars-1].isGroup = 1;
            }
        }

        while ((line[pos] == ' ') && (pos < len)) pos++;

        i = 0;
        while ((line[pos] != ' ') && (line[pos] != '.') && (pos < len) && (i < 32)) {
            linkageVars[numOfLinkageVars].name[i] = line[pos];
            pos++;
            i++;
        }
        linkageVars[numOfLinkageVars].name[i] = 0x00;
        linkageVars[numOfLinkageVars].isGroup = 0;
        if ((i > 0) && (!strstr(linkageVars[numOfLinkageVars].name,"filler"))) {
            if (strcmp(linkageVars[numOfLinkageVars].name,"DFHCOMMAREA") == 0) {
                commAreaPresent = 1;
            }
            numOfLinkageVars++;
        }
    }
}


// Load and insert copybook content
int includeCbk(char *copybook, FILE *outFile) {
    char path[255];
    sprintf(path,"%s%s%s","../copybooks/",copybook,".cpy");
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
        if (!eibPresent && strstr(line,"EIBCALEN")) {
            eibPresent = 1;
        }
        // Avoid inserting other EXEC-Macros
        if (!inExec && strstr(line,"EXEC")) {
            inExec = 1;
        }
        if (!inExec) {
            fputs(line,outFile);
            if (inLinkageSection) {
              parseLinkageVarDef(line);
            }
        }
        if (inExec && strstr(line,"END-EXEC")) {
            inExec = 0;
        }
    }

    fclose(cbk);
    return 1;
}


int includeCbkIO(char *copybook, FILE *outFile) {
    char path[255];
    sprintf(path,"%s%s%s","../copybooks/",copybook,"I.cpy");
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

    sprintf(path,"%s%s%s","../copybooks/",copybook,"O.cpy");
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


int includeCbkL(FILE *outFile) {
    char path[255];
    int i;
    for (i = 0; i < numOfLCopybooks; i++) {
        sprintf(path,"%s%s%s","../copybooks/",lcopybookNames[i],"L.cpy");
        FILE *cbk = fopen(path, "r");
        if (cbk == NULL) {
            printf("%s%s\n","Copybook file not found: ",path);
            continue;
        }

        char line[255];
        while (fgets(line, 255, (FILE*)cbk) != NULL) {
            fputs(line,outFile);
            parseLinkageVarDef(line);
        }

        fclose(cbk);
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
        fputs("           DISPLAY \"TPMI:EIBAID\" EIBAID.\n",outFile);
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
            (buf[i] == ',')) {
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


char *getExecTerminator(int quotes) {
    if (outputDot) {
        if (quotes) {
            return "\".";
        } else {
            return ".";
        }
    }

    if (quotes) {
        return "\"";
    } else {
        return "";
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
        if ((buf[i] == '\'') && !verbatim) {
            if (tokenPos > 0) {
                token[tokenPos] = 0x00;
                sprintf(execbuf,"%s%s%s\n","           DISPLAY \"TPMI:",
                        token,getExecTerminator(1));
                fputs(execbuf, (FILE*)fp2);
                tokenPos = 0;
            }
            verbatim = 1;
            token[tokenPos] = buf[i];
            tokenPos++;
            continue;
        }
        if ((buf[i] == '\'') && verbatim) {
            token[tokenPos] = buf[i];
            tokenPos++;
            token[tokenPos] = 0x00;
            if (mapNameMode == 0) {
                sprintf(execbuf,"%s%s%s\n","           DISPLAY \"TPMI:",
                        token,getExecTerminator(1));
                fputs(execbuf, (FILE*)fp2);
            }
            if (mapNameMode == 1) {
                token[strlen(token)-1] = 0x00;
                sprintf(mapName,"%s",token+1);
                sprintf(execbuf,"%s%s%s\n","           DISPLAY \"TPMI:MAP=",
                        mapName,getExecTerminator(1));
                fputs(execbuf, (FILE*)fp2);
                mapNameMode = 0;
            }
            if (mapNameMode == 2) {
                token[strlen(token)-1] = 0x00;
                sprintf(mapsetName,"%s",token+1);
                mapNameMode = 0;
                sprintf(execbuf,"%s%s%s\n","           DISPLAY \"TPMI:MAPSET=",
                        mapsetName,getExecTerminator(1));
                fputs(execbuf, (FILE*)fp2);
                includeMapDisplays(mapsetName,mapName,(FILE*)fp2,mapCmd);
            }
            tokenPos = 0;
            verbatim = 0;
            continue;
        }
        if ((buf[i] != '\'') && verbatim) {
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
                        sprintf(execbuf,"%s%s%s\n","           DISPLAY \"TPMI:",
                                token,getExecTerminator(1));
                        fputs(execbuf, (FILE*)fp2);
                    }
                    tokenPos = 0;

                    if (isErrHandlerField > 0) {
                        int para = 0;
                        do {
                          i++;
                          if ((buf[i] != ' ') && (buf[i] != ')')) {
                            token[tokenPos] = toupper(buf[i]);
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
                          sprintf(execbuf,"%s%s\n","           CALL \"setjmp\" USING QWICSJMP",getExecTerminator(0));
                          fputs(execbuf, (FILE*)fp2);
                          sprintf(execbuf,"%s\n","           IF RETURN-CODE > 0 THEN");
                          fputs(execbuf, (FILE*)fp2);
                          sprintf(execbuf,"%s%s\n","             PERFORM ",token);
                          fputs(execbuf, (FILE*)fp2);
                          sprintf(execbuf,"%s\n","           ELSE");
                          fputs(execbuf, (FILE*)fp2);
                          sprintf(execbuf,"%s%d%s\n","             CALL \"setJmpAbend\" USING ",isErrHandlerField,",QWICSJMP");
                          fputs(execbuf, (FILE*)fp2);
                          sprintf(execbuf,"%s%s\n","           END-IF",getExecTerminator(0));
                          fputs(execbuf, (FILE*)fp2);
                        } else {
                          sprintf(execbuf,"%s%s%s\n","           DISPLAY \"TPMI:\" ",
                                  token,getExecTerminator(0));
                          fputs(execbuf, (FILE*)fp2);
                        }
                        isErrHandlerField = 0;
                        tokenPos = 0;
                    }

                    if (isPtrField) {
                        int adrOf = 0;
                        do {
                          i++;
                          if ((buf[i] != ' ') && (buf[i] != ')')) {
                            token[tokenPos] = toupper(buf[i]);
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
                          sprintf(execbuf,"%s%s%s\n","           DISPLAY \"TPMI:\" ",
                                  "QWICSPTR",getExecTerminator(0));
                          fputs(execbuf, (FILE*)fp2);
                          sprintf(execbuf,"%s%s%s%s\n","           SET ADDRESS OF ",
                                  token," TO QWICSPTR",getExecTerminator(0));
                          fputs(execbuf, (FILE*)fp2);
                        } else {
                          sprintf(execbuf,"%s%s%s\n","           DISPLAY \"TPMI:\" ",
                                  token,getExecTerminator(0));
                          fputs(execbuf, (FILE*)fp2);
                        }
                        isPtrField = 0;
                        tokenPos = 0;
                    }

                    if (isLengthField) {
                        int lenOf = 0;
                        do {
                          i++;
                          if ((buf[i] != ' ') && (buf[i] != ')')) {
                            token[tokenPos] = toupper(buf[i]);
                            tokenPos++;
                          } else {
                            if (tokenPos > 0) {
                              token[tokenPos] = 0x00;
                              tokenPos = 0;
                            }
                            if ((lenOf == 0) && (strcmp(token,"LENGTH") == 0)) {
                                lenOf = 1;
                            }
                            if ((lenOf == 1) && (strcmp(token,"OF") == 0)) {
                                lenOf = 2;
                            }
                          }
                        } while (buf[i] != ')');

                        if (lenOf == 2) {
                          sprintf(execbuf,"%s%s%s%s\n","           MOVE LENGTH OF ",
                                  token," TO QWICSLEN",getExecTerminator(0));
                          fputs(execbuf, (FILE*)fp2);
                          sprintf(execbuf,"%s%s%s\n","           DISPLAY \"TPMI:\" ",
                                  "QWICSLEN",getExecTerminator(0));
                          fputs(execbuf, (FILE*)fp2);
                        } else {
                          sprintf(execbuf,"%s%s%s\n","           DISPLAY \"TPMI:\" ",
                                  token,getExecTerminator(0));
                          fputs(execbuf, (FILE*)fp2);
                        }
                        isLengthField = 0;
                        tokenPos = 0;
                    }
                }
                value = 1;
            } else {
                if ((buf[i] == ' ') || (buf[i] == '\n') ||
                    (buf[i] == '\r') || (buf[i] == ')')) {
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
                                sprintf(execbuf,"%s%s%s\n","           DISPLAY \"TPMI:",
                                        token,getExecTerminator(1));
                                fputs(execbuf, (FILE*)fp2);
                                isBranchLabel = 0;
                            } else
                            if (isResponseParam) {
                                sprintf(execbuf,"%s%s%s%s%s\n","           DISPLAY \"TPMI:",
                                        respParam,"\" ",token,getExecTerminator(0));
                                fputs(execbuf, (FILE*)fp2);
                                isResponseParam = 0;
                            } else {
                                sprintf(execbuf,"%s%s%s\n","           DISPLAY \"TPMI:\" ",
                                        token,getExecTerminator(0));
                                fputs(execbuf, (FILE*)fp2);
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
                                sprintf(execbuf,"%s%s%s\n","           DISPLAY \"TPMI:",
                                        token,getExecTerminator(1));
                                fputs(execbuf, (FILE*)fp2);
                            }
                        }
                        tokenPos = 0;
                    }
                    value = 0;
                } else {
                    token[tokenPos] = toupper(buf[i]);
                    tokenPos++;
                }
            }
        }
        if (execCmd == 2) {  // EXEC SQL
            if (buf[i] == ':') {
                if (tokenPos > 0) {
                    token[tokenPos] = 0x00;
                    sprintf(execbuf,"%s%s%s\n","           DISPLAY \"TPMI:",
                            token,getExecTerminator(1));
                    fputs(execbuf, (FILE*)fp2);
                    tokenPos = 0;
                }
                value = 1;
            } else {
                if ((buf[i] == ' ') || (buf[i] == '\n') ||
                    (buf[i] == '\r')) {
                    if (tokenPos > 0) {
                        token[tokenPos] = 0x00;
                        if (value == 1) {
                            sprintf(execbuf,"%s%s%s\n","           DISPLAY \"TPMI:\" ",
                                    token,getExecTerminator(0));
                        } else {
                            sprintf(execbuf,"%s%s%s\n","           DISPLAY \"TPMI:",
                                    token,getExecTerminator(1));
                        }
                        fputs(execbuf, (FILE*)fp2);
                        tokenPos = 0;
                    }
                    value = 0;
                } else {
                    if ((buf[i] == ',') || (buf[i] == '=') || (buf[i] == '(') || (buf[i] == ')') ||
                        (buf[i] == '>') || (buf[i] == '<') || (buf[i] == '*')) {
                        if (tokenPos > 0) {
                            token[tokenPos] = 0x00;
                            if (value == 1) {
                                sprintf(execbuf,"%s%s%s\n","           DISPLAY \"TPMI:\" ",
                                        token,getExecTerminator(0));
                            } else {
                                sprintf(execbuf,"%s%s%s\n","           DISPLAY \"TPMI:",
                                        token,getExecTerminator(1));
                            }
                            fputs(execbuf, (FILE*)fp2);
                            tokenPos = 0;
                        }
                        value = 0;
                        token[0] = buf[i];
                        token[1] = 0x00;
                        sprintf(execbuf,"%s%s%s\n","           DISPLAY \"TPMI:",
                                token,getExecTerminator(1));
                        fputs(execbuf, (FILE*)fp2);
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


int isEmptyLine(char *buf) {
    int l = strlen(buf);
    int i = l-1;
    while ((i > 0) && ((buf[i] == ' ') || (buf[i] == 10) || (buf[i] == 13))) {
        i--;
    }
    if (i <= 0)
        return 1;

    return 0;
}


int main(int argc, char **argv) {
   FILE *fp,*fp2;
   char buf[255],oname[255];

   buf[0] = 0x00;

   if (argc < 2) {
	    printf("%s\n","Usage: cobprep <COBOL-File>");
	    return -1;
   }

   fp = fopen(argv[1], "r");
   if (fp == NULL) {
	    printf("%s%s\n","No input file: ",argv[1]);
	    return -1;
   }

   sprintf(oname,"%s%s","exec_",argv[1]);
   fp2 = fopen(oname,"w");
   if (fp2 == NULL) {
        printf("%s%s\n","Could not create output file: ",oname);
        return -1;
   }

   int execCmd = 0;
   int inProcDivision = 0;
   int startProcDivision = 0;
   int linkageSectionPresent = 0;
   int wstorageSectionPresent = 0;

   while (fgets(buf, 255, (FILE*)fp) != NULL) {
       // Handle comments
       if (buf[6] == '*') {
         fputs(buf,(FILE*)fp2);
         continue;
       }
       // Turn everything to caps
       int verb = 0;
       for (int i = 0; i < strlen(buf); i++) {
         if ((verb == 1) && (buf[i] == '\'')) {
             verb = 0;
         }
         if ((verb == 2) && (buf[i] == '"')) {
             verb = 0;
         }
         if ((verb == 0) && (buf[i] == '\'')) {
             verb = 1;
         }
         if ((verb == 0) && (buf[i] == '"')) {
             verb = 2;
         }
         if (verb == 0) {
            buf[i] = toupper(buf[i]);
         }
       }

       if (strstr(buf,"PROCEDURE DIVISION") != NULL) {
            inLinkageSection = 0;
            inProcDivision = 1;
            startProcDivision = 1;

            if (!linkageSectionPresent) {
                if (!wstorageSectionPresent) {
                  fputs("       WORKING-STORAGE SECTION.\n",(FILE*)fp2);
                  fputs("       77  QWICSPTR USAGE IS POINTER.\n",(FILE*)fp2);
                  fputs("       77  QWICSLEN PIC 9(9).\n",(FILE*)fp2);
                  fputs("       77  QWICSJMP PIC X(200).\n",(FILE*)fp2);
                }
                fputs("       LINKAGE SECTION.\n",(FILE*)fp2);
                inLinkageSection = 1;
                if (!eibPresent) {
                    includeCbk("DFHEIBLK",(FILE*)fp2);
                }
                includeCbkL((FILE*)fp2);
                inLinkageSection = 0;
            }
            if (!commAreaPresent) {
                fputs("       01  DFHCOMMAREA PIC X.\n",(FILE*)fp2);
            }
            sprintf(&buf[25],"%s"," USING DFHCOMMAREA");
            fputs(buf,(FILE*)fp2);
            int n = 0;
            for (n = 0; n < numOfLinkageVars; n++) {
                if (((linkageVars[n].level == 1) || (linkageVars[n].level > 49))
                    && (strcmp(linkageVars[n].name,"DFHCOMMAREA") != 0)) {
                    // Top level or elementary variable
                    fputs(",\n",(FILE*)fp2);
                    sprintf(buf,"%s%s","      -     ",linkageVars[n].name);
                    fputs(buf,(FILE*)fp2);
                }
            }
            fputs(".\n",(FILE*)fp2);
            buf[0] = 0x00;
            linkageSectionPresent = 0;
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
               if (strstr(buf,"LINKAGE SECTION") != NULL) {
                 if (!wstorageSectionPresent) {
                   fputs("       WORKING-STORAGE SECTION.\n",(FILE*)fp2);
                   fputs("       77  QWICSPTR USAGE IS POINTER.\n",(FILE*)fp2);
                   fputs("       77  QWICSLEN PIC 9(9).\n",(FILE*)fp2);
                   fputs("       77  QWICSJMP PIC X(200).\n",(FILE*)fp2);
                 }
               }
               fputs(buf,(FILE*)fp2);
               if (linkageSectionPresent) {
                   parseLinkageVarDef(buf);
               }
               if (startProcDivision) {
                   if (sqlca) {
                       char buf[80];
                       sprintf(buf,"%s%s%s\n","           DISPLAY \"TPMI:SET SQLCODE\" ",
                              "SQLCODE",getExecTerminator(0));
                       fputs(buf,(FILE*)fp2);
                   }
                   sprintf(buf,"%s%s%s\n","           DISPLAY \"TPMI:SET DFHEIBLK\" ",
                           "DFHEIBLK",getExecTerminator(0));
                   fputs(buf,(FILE*)fp2);
                   sprintf(buf,"%s%s%s\n","           DISPLAY \"TPMI:SET EIBCALEN\" ",
                           "EIBCALEN",getExecTerminator(0));
                   fputs(buf,(FILE*)fp2);
                   sprintf(buf,"%s%s%s\n","           DISPLAY \"TPMI:SET EIBAID\" ",
                           "EIBAID",getExecTerminator(0));
                   fputs(buf,(FILE*)fp2);

                   int n = 0;
                   for (n = 0; n < numOfLinkageVars; n++) {
                       if (linkageVars[n].isGroup) {
                           sprintf(buf,"%s%d%s%s%s%s%s\n","           DISPLAY \"TPMI:SETL1 ",
                                   linkageVars[n].level," ",
                                   linkageVars[n].name,"\" ",linkageVars[n].name,getExecTerminator(0));
                       } else {
                           sprintf(buf,"%s%d%s%s%s%s%s\n","           DISPLAY \"TPMI:SETL0 ",
                                   linkageVars[n].level," ",
                                   linkageVars[n].name,"\" ",linkageVars[n].name,getExecTerminator(0));
                       }
                       fputs(buf,(FILE*)fp2);
                   }

                   startProcDivision = 0;
               }
           }
           if (inProcDivision) {
                if (!isEmptyLine(buf)) {
                    if ((buf[6] != '*') && hasDotTerminator(buf)) {
                        outputDot = 1;
                    } else {
                        outputDot = 0;
                    }
                }
           }
       } else {
           if (inProcDivision) {
               processExecLine(execCmd,buf,fp2);
           } else {
               processDataExecLine(buf,fp2);
           }

           char *cmd = strstr(buf,"END-EXEC");
           if (cmd != NULL) {
               execCmd = 0;
               if (isReturn || isXctl) {
                   char gb[30];
                   sprintf(gb,"%s%s\n","           GOBACK",getExecTerminator(0));
                   fputs(gb,(FILE*)fp2);
                   isReturn = 0;
                   isXctl = 0;
               }
           }
       }

       if (strstr(buf,"LINKAGE SECTION") != NULL) {
            inLinkageSection = 1;
            linkageSectionPresent = 1;
            if (!eibPresent) {
                includeCbk("DFHEIBLK",(FILE*)fp2);
            }
            includeCbkL((FILE*)fp2);
       }

       if (strstr(buf,"WORKING-STORAGE SECTION") != NULL) {
            wstorageSectionPresent = 1;
            fputs("       77  QWICSPTR USAGE IS POINTER.\n",(FILE*)fp2);
            fputs("       77  QWICSLEN PIC 9(9).\n",(FILE*)fp2);
            fputs("       77  QWICSJMP PIC X(200).\n",(FILE*)fp2);
       }
  }

  fclose(fp);
  fclose(fp2);
  return 0;

  // argv[0| = "cobc";
  // return execv("cobc",argv);
}
