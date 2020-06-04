/*******************************************************************************************/
/*   QWICS Server COBOL Preprocessor                                                       */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 04.06.2020                                  */
/*                                                                                         */
/*   Copyright (C) 2018 - 2020 by Philipp Brune  Email: Philipp.Brune@hs-neu-ulm.de        */
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
int execCmd = 0;
int inProcDivision = 0;
int startProcDivision = 0;
int linkageSectionPresent = 0;
int wstorageSectionPresent = 0;
int execSQLCnt = 0;
int skipLinesMode = 0;
int skipFillerDefs = 0;
int fillerDefLevel = 0;


struct linkageVarDef {
    char name[33];
    int isGroup;
    int level;
    int occurs;
    int index;
} linkageVars[2048];

int numOfLinkageVars = 0;
char *cbkPath = "../copybooks";
FILE *declareTmpFile = NULL;
char declareName[255] = "";
char setptrbuf[255] = "";


void getVarInSuffix(int n, char *suffix, char *subscript) {
    int i = n;
    char prevSubscript[80]; 
    if (n < numOfLinkageVars) {
        while (i >= 0) {
            if (linkageVars[i].level > 49) {
                return;
            }
            if (linkageVars[i].isGroup && (linkageVars[i].level < linkageVars[n].level)) {
                if (linkageVars[i].index > 0) {
                    sprintf(prevSubscript,"%s",subscript);
                    if (strlen(prevSubscript) > 0) {
                        sprintf(subscript,"%d,%s",linkageVars[i].index,prevSubscript);
                    } else {
                        sprintf(subscript,"%d",linkageVars[i].index);
                    }
                }
                int l = strlen(suffix);
                if (l == 0) {
                  sprintf(&suffix[l],"%s%s"," IN ",linkageVars[i].name);
                } else {
                  sprintf(&suffix[l],"%s%s","\n            IN  ",linkageVars[i].name);
                }
                getVarInSuffix(i,suffix,subscript);
                break;
            }
            i--;
        }
    }
}


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
        int level = atoi(lbuf);
        if ((level < 1) || ((level > 49) && (level != 77))) {
          return;
        }

        if (skipFillerDefs) {
            if (level <= fillerDefLevel) {
                skipFillerDefs = 0;
            } else {
                return;
            }
        }

        if ((strstr(line,"REDEFINES") == NULL) && (strstr(line,"FILLER") == NULL)) {
            linkageVars[numOfLinkageVars].level = level;

            if (numOfLinkageVars > 0) {
                if ((linkageVars[numOfLinkageVars].level <= 49) && 
                    (linkageVars[numOfLinkageVars].level > linkageVars[numOfLinkageVars-1].level)) {
                    linkageVars[numOfLinkageVars-1].isGroup = 1;
                }
            }

            while ((line[pos] == ' ') && (pos < len)) pos++;

            i = 0;
            while ((line[pos] != ' ') && (line[pos] != '.') && (line[pos] != '\r') && (line[pos] != '\n')
                    && (pos < len) && (i < 32)) {
                linkageVars[numOfLinkageVars].name[i] = line[pos];
                pos++;
                i++;
            }
            linkageVars[numOfLinkageVars].name[i] = 0x00;
            linkageVars[numOfLinkageVars].isGroup = 0;
            linkageVars[numOfLinkageVars].occurs = 0;
            linkageVars[numOfLinkageVars].index = 0;

            char *occrs = strstr(line,"OCCURS");
            if (occrs != NULL) {
                int l = strlen(occrs);
                int pos = 6;
                while ((occrs[pos] == ' ') && (pos < l)) pos++;

                int i = 0;
                char timesStr[32];  
                while ((occrs[pos] != ' ') && (occrs[pos] != '.') && (occrs[pos] != '\r') && (occrs[pos] != '\n')
                    && (pos < l) && (i < 32)) {
                    timesStr[i] = occrs[pos];    
                    pos++;
                    i++;
                }
                timesStr[i] = 0x00;
                linkageVars[numOfLinkageVars].occurs = atoi(timesStr);
                if (linkageVars[numOfLinkageVars].occurs < 1) {
                    linkageVars[numOfLinkageVars].occurs = 1;
                }
            }

            if ((i > 0) && (!strstr(linkageVars[numOfLinkageVars].name,"FILLER"))) {
                if (strcmp(linkageVars[numOfLinkageVars].name,"DFHCOMMAREA") == 0) {
                    commAreaPresent = 1;
                }
                numOfLinkageVars++;
            }
        } else {
            skipFillerDefs = 1;
            fillerDefLevel = level;
        }
    }
}


void processLine(char *buf, FILE *fp2);


// Insert declares temp file for db cursors etc. declared not in proc division
int includeDeclares(FILE *outFile) {
    FILE *df = fopen("declare.tmp", "r");
    if (df == NULL) {
        printf("%s\n","Info: Declare tmp file not found.");
        return -1;
    }

    char line[255];
    while (fgets(line, 255, (FILE*)df) != NULL) {
      fputs(line,outFile);
    }

    fputs("\n",outFile);
    fclose(df);
    unlink("declare.tmp");
    return 1;
}


// Look for copybook file in different locations given by cbkPath
FILE *openCbkFile(char *copybook, char *suffix) {
    char path[255];
    char strbuf[255];
    FILE *cbk = NULL;
    int pathPos = 0;
    int len = strlen(cbkPath);
    while ((cbk == NULL) && (pathPos < len)) {
      int i = pathPos,j = 0;
      while ((cbkPath[i] != ':') && (cbkPath[i] != 0x00) && (i < len)) {
        strbuf[j] = cbkPath[i];
        i++;
        j++;
      }
      strbuf[j] = 0x00;
      sprintf(path,"%s%s%s%s",strbuf,"/",copybook,suffix);
      cbk = fopen(path, "r");
      pathPos = i+1;
    }
    return cbk;
}


void strrep(char *line, char *s, char *r) {
    char lineCpy[255];
    char *occ = strstr(line,s);
    if (occ != NULL) {
        int len = (int)(occ-line);
        strncpy(lineCpy,line,len);
        int l = strlen(r);
        strncpy(&lineCpy[len],r,l);
        strcpy(&lineCpy[len+l],&occ[strlen(s)]);
        lineCpy[len+l+strlen(&occ[strlen(s)])] = 0x00;
        strcpy(line,lineCpy);
        line[strlen(lineCpy)] = 0x00;
        strrep(line,s,r);
    }
}


// Load and insert copybook content
int includeCbk(char *copybook, FILE *outFile, char *findStr, char *replStr, int raw) {
    FILE *cbk = openCbkFile(copybook,".cpy");
    if (cbk == NULL) {
        printf("%s%s%s\n","Info: Copybook file not found: ",copybook," - Looking for I/O suffix files.");
        return -1;
    }

    char line[255];
//    int inExec = 0;
    sprintf(line,"%s%s%s\n","************ copybook - ",copybook," added");
    fputs(line,outFile);

    while (fgets(line, 255, (FILE*)cbk) != NULL) {
        if (strstr(line,"SQLCODE")) {
            sqlca = 1;
        }

        if (!eibPresent && strstr(line,"EIBCALEN")) {
            eibPresent = 1;
        }

        if ((findStr != NULL) && (replStr != NULL)) {
            strrep(line,findStr,replStr);
        }
        if (raw == 1) {
            fputs(line,outFile);
        } else {
            processLine(line,outFile);
        }
        // Avoid inserting other EXEC-Macros
        /*
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
        */
    }

    fputs("\n",outFile);
    fclose(cbk);
    return 1;
}


int includeCbkIO(char *copybook, FILE *outFile) {
    FILE *cbk = openCbkFile(copybook,"I.cpy");
    if (cbk == NULL) {
        printf("%s%s\n","Copybook file not found: ",copybook);
        return -1;
    }

    char line[255];
    while (fgets(line, 255, (FILE*)cbk) != NULL) {
        fputs(line,outFile);
    }

    fputs("\n",outFile);
    fclose(cbk);

    cbk = openCbkFile(copybook,"O.cpy");
    if (cbk == NULL) {
        printf("%s%s\n","Copybook file not found: ",copybook);
        return -1;
    }

    while (fgets(line, 255, (FILE*)cbk) != NULL) {
        fputs(line,outFile);
    }

    fputs("\n",outFile);
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
        FILE *cbk = openCbkFile(lcopybookNames[i],"L.cpy");
        if (cbk == NULL) {
            printf("%s%s\n","Copybook file not found: ",lcopybookNames[i]);
            continue;
        }

        char line[255];
        while (fgets(line, 255, (FILE*)cbk) != NULL) {
            fputs(line,outFile);
            parseLinkageVarDef(line);
        }

        fputs("\n",outFile);
        fclose(cbk);
    }

    return 1;
}


int includeMapDisplays(char *mapset, char *map, FILE *outFile, int input) {
    FILE *cbk = NULL;
    if (input) {
        cbk = openCbkFile(mapset,"I.dsp");
    } else {
        cbk = openCbkFile(mapset,"O.dsp");
    }
    if (cbk == NULL) {
        printf("%s%s\n","Copybook file not found: ",mapset);
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

    fputs("\n",outFile);
    fclose(cbk);
    return 1;
}


int hasDotTerminator(char *buf) {
    int l = strlen(buf);
    if (l > 72) {
      l = 72;
    }
    int i = l-1;
    while ((i > 7) && ((buf[i] == ' ') || (buf[i] == 10) || (buf[i] == 13))) {
        i--;
    }
    if (buf[i] == '.') {
        return 1;
    }
    return 0;
}


int isEmptyLine(char *buf) {
    int l = strlen(buf);
    if (l > 72) {
      l = 72;
    }
    int i = l-1;
    while ((i > 7) && ((buf[i] == ' ') || (buf[i] == 10) || (buf[i] == 13))) {
        i--;
    }
    if (i <= 7)
        return 1;

    return 0;
}


// Extract search/replace string
void processReplacingClause(char *buf, int *pos, char *pattern) {
    int i = *pos;
    int pseudo = 0;
    while (buf[i] == ' ') {
        i++;
    }
    int j = 0;
    while ((buf[i] == '=') && (j < 2)) {
        i++;
        j++;
    }
    if (j == 2) {
        pseudo = 1;
    }
    if (pseudo) {
        int j = 0;
        while ((buf[i] != '=') || (buf[i+1] != '=')) {
            pattern[j] = buf[i];
            i++;
            j++;
        }
        pattern[j] = 0x00;
    } else {
        int j = 0;
        while (buf[i] != ' ') {
            pattern[j] = buf[i];
            i++;
            j++;
        }
        pattern[j] = 0x00;
    }
    while ((buf[i] != ' ') && (buf[i] != '.')) {
        i++;
    }
    i--;
    (*pos) = i;
}


// Process include/copy line
void processCopyLine(char *buf, FILE *fp2) {
    char linebuf[255];
    int i,m = strlen(buf);
    char token[255];
    int tokenPos = 0;
    int include = 0;
    char cbkFile[255];
    char findStr[255];
    char replStr[255];

    if (m > 72) {
      // Ignore reserved characters
      m = 72;
    }
    for (i = 7; i < m; i++) {
        if ((buf[i] == ' ') || (buf[i] == '\n') ||
            (buf[i] == '\r') || (buf[i] == '.') ||
            (buf[i] == ',')) {
            if ((include >= 2) && (buf[i] == '.')) {
                if (include >= 4) {
                    if (includeCbk(cbkFile,fp2,findStr,replStr,0) < 0) {
                        includeCbkIO(cbkFile,fp2);
                    }
                } else {
                    if (includeCbk(cbkFile,fp2,NULL,NULL,0) < 0) {
                        includeCbkIO(cbkFile,fp2);
                   }
                }

                i++;
                int j = 0;
                for (j = 0; j < i; j++) {
                    linebuf[j] = ' ';
                }
                while (i < m) {
                  linebuf[i] = buf[i];
                  i++;
                }
                linebuf[m] = 0x00;
                fputs(linebuf,fp2);
                if (hasDotTerminator(linebuf)) {
                    outputDot = 1;
                } else {
                    outputDot = 0;
                }
                continue;
            }
            if (tokenPos > 0) {
                token[tokenPos] = 0x00;
                if (include == 3) {
                    if (strstr(token,"BY") != NULL) {
                        processReplacingClause(buf,&i,replStr);
                        include = 4;
                    }
                }
                if (include == 2) {
                    if (strstr(token,"REPLACING") != NULL) {
                        processReplacingClause(buf,&i,findStr);
                        include = 3;
                    }
                }
                if (include == 1) {
                    sprintf(cbkFile,"%s",token);
                    include = 2;
                    i--;
                }
                if ((include == 0) && ((strstr(token,"INCLUDE") != NULL) || (strstr(token,"COPY") != NULL))) {
                    include = 1;
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


void prepareSqlHostVar(char *token) {
  char varbuf[255];
  int i,j,pos = 0,m = 0,n = 0;
  m = strlen(token);
  n = m;
  for (i = m-1; i >= 0; i--) {
    if ((i == 0) || (token[i] == '.')) {
      if (pos > 0) {
        varbuf[pos] = ' '; pos++;
        varbuf[pos] = 'I'; pos++;
        varbuf[pos] = 'N'; pos++;
        varbuf[pos] = ' '; pos++;
      }
      if (i > 0) {
        j = i+1;
      } else {
        j = i;
      }
      while (j < n) {
        varbuf[pos] = token[j];
        pos++;
        j++;
      }
      n = i;
    }
  }
  varbuf[pos] = 0x00;
  sprintf(token,"%s",varbuf);
}


// Process EXEC ... END-EXEC statement line in the procedure division
void processExecLine(char *buf, FILE *fp2) {
    char execbuf[255];
    int i,m = strlen(buf);
    char token[255];
    int tokenPos = 0;
    int value = 0;
    int verbatim = 0;
    if (m > 72) {
      // Ignore reserved characters
      m = 72;
    }
    for (i = 7; i < m; i++) {
        if ((buf[i] == '\'') && !verbatim) {
            if (tokenPos > 0) {
                token[tokenPos] = 0x00;
                sprintf(execbuf,"%s%s%s\n","           DISPLAY \"TPMI:",
                        token,getExecTerminator(1));
                if ((execCmd == 1) || ((execCmd == 2) && (execSQLCnt == 3))) {
                  fputs(execbuf, (FILE*)fp2);
                }
                if (!inProcDivision && (execCmd == 2) && (execSQLCnt == 4)) {
                  fputs(execbuf, (FILE*)declareTmpFile);
                }
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
                if ((execCmd == 1) || ((execCmd == 2) && (execSQLCnt == 3))) {
                  fputs(execbuf, (FILE*)fp2);
                }
                if (!inProcDivision && (execCmd == 2) && (execSQLCnt == 4)) {
                  fputs(execbuf, (FILE*)declareTmpFile);
                }
            }
            if (mapNameMode == 1) {
                token[strlen(token)-1] = 0x00;
                sprintf(mapName,"%s",token+1);
                sprintf(execbuf,"%s%s%s\n","           DISPLAY \"TPMI:MAP=",
                        mapName,getExecTerminator(1));
                if ((execCmd == 1) || ((execCmd == 2) && (execSQLCnt == 3))) {
                  fputs(execbuf, (FILE*)fp2);
                }
                if (!inProcDivision && (execCmd == 2) && (execSQLCnt == 4)) {
                  fputs(execbuf, (FILE*)declareTmpFile);
                }
                mapNameMode = 0;
            }
            if (mapNameMode == 2) {
                token[strlen(token)-1] = 0x00;
                sprintf(mapsetName,"%s",token+1);
                mapNameMode = 0;
                sprintf(execbuf,"%s%s%s\n","           DISPLAY \"TPMI:MAPSET=",
                        mapsetName,getExecTerminator(1));
                if ((execCmd == 1) || ((execCmd == 2) && (execSQLCnt == 3))) {
                  fputs(execbuf, (FILE*)fp2);
                }
                if (!inProcDivision && (execCmd == 2) && (execSQLCnt == 4)) {
                  fputs(execbuf, (FILE*)declareTmpFile);
                }
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
                int old_i = i;

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
                    if (strstr(token,"START") != NULL) {
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
                          sprintf(setptrbuf,"%s%s%s%s\n","           SET ADDRESS OF ",
                                  token," TO QWICSPTR",getExecTerminator(0));
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

                if (i == old_i) {
                    tokenPos = 0;
                    int bracketLevel = 0;
                    i++;
                    if (buf[i] == '\'') {
                        i--;
                        continue;
                    }
                    while ((buf[i] != ')') || (bracketLevel > 0)) {
                        if (buf[i] == '(') bracketLevel++;
                        if (buf[i] == ')') bracketLevel--;
                        if (buf[i] != ' ') {
                            token[tokenPos] = toupper(buf[i]);
                            tokenPos++;
                        }
                        i++;
                    }
                    i--;
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
                            if (strstr(token,"START") != NULL) {
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
                    if (inProcDivision && (execSQLCnt == 3)) {
                        fputs(execbuf, (FILE*)fp2);
                    }
                    if (!inProcDivision && (execSQLCnt == 4)) {
                        fputs(execbuf, (FILE*)declareTmpFile);
                    }
                    tokenPos = 0;
                }
                value = 1;
            } else {
                if ((buf[i] == ' ') || (buf[i] == '\n') ||
                    (buf[i] == '\r')) {
                    if (tokenPos > 0) {
                        token[tokenPos] = 0x00;
                        if (execSQLCnt == 98) {
                          if (strcmp("CURSOR",token) == 0) {
                            if (!inProcDivision) {
                              execSQLCnt = 4;
                              sprintf(execbuf,"%s%s\n","           DISPLAY \"TPMI:EXEC\"",getExecTerminator(0));
                              fputs(execbuf, (FILE*)declareTmpFile);
                              sprintf(execbuf,"%s%s\n","           DISPLAY \"TPMI:SQL\"",getExecTerminator(0));
                              fputs(execbuf, (FILE*)declareTmpFile);
                              sprintf(execbuf,"%s%s\n","           DISPLAY \"TPMI:DECLARE\"",getExecTerminator(0));
                              fputs(execbuf, (FILE*)declareTmpFile);
                              sprintf(execbuf,"%s%s%s%s\n","           DISPLAY \"TPMI:",declareName,"\"",getExecTerminator(0));
                              fputs(execbuf, (FILE*)declareTmpFile);
                            } else {
                              execSQLCnt = 3;
                              sprintf(execbuf,"%s%s\n","           DISPLAY \"TPMI:EXEC\"",getExecTerminator(0));
                              fputs(execbuf, (FILE*)fp2);
                              sprintf(execbuf,"%s%s\n","           DISPLAY \"TPMI:SQL\"",getExecTerminator(0));
                              fputs(execbuf, (FILE*)fp2);
                              sprintf(execbuf,"%s%s\n","           DISPLAY \"TPMI:DECLARE\"",getExecTerminator(0));
                              fputs(execbuf, (FILE*)fp2);
                              sprintf(execbuf,"%s%s%s%s\n","           DISPLAY \"TPMI:",declareName,"\"",getExecTerminator(0));
                              fputs(execbuf, (FILE*)fp2);
                            }
                          } else {
                              if (strlen(declareName) == 0) {
                                sprintf(declareName,"%s",token);
                              }
                          }
                        }
                        if (execSQLCnt == 2) {
                          if (strcmp("INCLUDE",token) == 0) {
                            execCmd = 0;
                            processCopyLine(buf,fp2);
                            execCmd = 2;
                            execSQLCnt = 99;
                          } else
                          if (strcmp("DECLARE",token) == 0) {
                            execSQLCnt = 98;
                            declareName[0] = 0x00;
                          } else
                          if (strcmp("BEGIN",token) == 0) {
                            execSQLCnt = 99;
                          } else
                          if (strcmp("END",token) == 0) {
                            execSQLCnt = 99;
                          } else {
                            sprintf(execbuf,"%s%s\n","           DISPLAY \"TPMI:EXEC\"",getExecTerminator(0));
                            if (inProcDivision) {
                              fputs(execbuf, (FILE*)fp2);
                            }
                            sprintf(execbuf,"%s%s\n","           DISPLAY \"TPMI:SQL\"",getExecTerminator(0));
                            if (inProcDivision) {
                              fputs(execbuf, (FILE*)fp2);
                            }
                            execSQLCnt++;
                          }
                        }
                        if (execSQLCnt == 3) {
                            if (value == 1) {
                                prepareSqlHostVar(token);
                                sprintf(execbuf,"%s%s%s\n","           DISPLAY \"TPMI:\" ",
                                        token,getExecTerminator(0));
                            } else {
                                sprintf(execbuf,"%s%s%s\n","           DISPLAY \"TPMI:",
                                        token,getExecTerminator(1));
                            }
                            if (inProcDivision) {
                              fputs(execbuf, (FILE*)fp2);
                            }
                        }
                        if (execSQLCnt == 4) {
                            if (value == 1) {
                                prepareSqlHostVar(token);
                                sprintf(execbuf,"%s%s%s\n","           DISPLAY \"TPMI:\" ",
                                        token,getExecTerminator(0));
                            } else {
                                sprintf(execbuf,"%s%s%s\n","           DISPLAY \"TPMI:",
                                        token,getExecTerminator(1));
                            }
                            fputs(execbuf, (FILE*)declareTmpFile);
                        }
                        if (strcmp("EXEC",token) == 0) {
                          execSQLCnt++;
                        }
                        if (strcmp("SQL",token) == 0) {
                          execSQLCnt++;
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
                                prepareSqlHostVar(token);
                                sprintf(execbuf,"%s%s%s\n","           DISPLAY \"TPMI:\" ",
                                        token,getExecTerminator(0));
                            } else {
                                sprintf(execbuf,"%s%s%s\n","           DISPLAY \"TPMI:",
                                        token,getExecTerminator(1));
                            }
                            if (inProcDivision) {
                              fputs(execbuf, (FILE*)fp2);
                            }
                            if (!inProcDivision && (execSQLCnt == 4)) {
                                fputs(execbuf, (FILE*)declareTmpFile);
                            }
                            tokenPos = 0;
                        }
                        value = 0;
                        token[0] = buf[i];
                        token[1] = 0x00;
                        sprintf(execbuf,"%s%s%s\n","           DISPLAY \"TPMI:",
                                token,getExecTerminator(1));
                        if (inProcDivision) {
                          fputs(execbuf, (FILE*)fp2);
                        }
                        if (!inProcDivision && (execSQLCnt == 4)) {
                            fputs(execbuf, (FILE*)declareTmpFile);
                        }
                    } else {
                        token[tokenPos] = buf[i];
                        tokenPos++;
                    }
                }
            }
        }
    }
}


void outputLinkageVar(int *idx, FILE *fp2) {
    char buf[255];
    int i,n = *idx,m;
    char inSuffix[255];
    int max = linkageVars[n].occurs;
    if (max < 1) max = 1;
    char subscript[80];
    char subscript2[80];

    for (i = 0; i < max; i++) {
        subscript[0] = 0x00;
        if (linkageVars[n].occurs > 0) {
            linkageVars[n].index = i+1;
            sprintf(subscript,"%d",i+1);
        }
        inSuffix[0] = 0x00;
        getVarInSuffix(n,inSuffix,subscript);
        subscript2[0] = 0x00;
        if (strlen(subscript) > 0) {
            sprintf(subscript2,"(%s)",subscript);
        }

        if (linkageVars[n].isGroup) {
            sprintf(buf,"%s%d%s%s%s%s%s%s%s%s%s\n","           DISPLAY \"TPMI:SETL1 ",
                    linkageVars[n].level," ",
                    linkageVars[n].name,subscript2,"\" \n",
                    "      -     ",linkageVars[n].name,inSuffix,subscript2,getExecTerminator(0));
        } else {
            sprintf(buf,"%s%d%s%s%s%s%s%s%s%s%s\n","           DISPLAY \"TPMI:SETL0 ",
                    linkageVars[n].level," ",
                    linkageVars[n].name,subscript2,"\" \n",
                    "      -     ",linkageVars[n].name,inSuffix,subscript2,getExecTerminator(0));
        }
        fputs(buf,(FILE*)fp2);

        m = n+1;
        if (linkageVars[n].isGroup) {            
            while ((linkageVars[m].level <= 49) && (linkageVars[m].level > linkageVars[n].level) && (m < numOfLinkageVars)) {
                outputLinkageVar(&m,fp2);
                m++;
            }
        }
        m--;
    }
    *idx = m;
}

void processLine(char *buf, FILE *fp2) {
  // Handle comments
  if (buf[6] == '*') {
    fputs(buf,(FILE*)fp2);
    return;
  }
  if (skipLinesMode == 1) {
      if (hasDotTerminator(buf)) {
          skipLinesMode = 0;
          outputDot = 1;
      }
      return;
  }

  // Turn everything to caps
  int verb = 0;
  int bl = strlen(buf);
  for (int i = 0; i < bl; i++) {
    if ((verb == 1) && (buf[i] == '\'')) {
        verb = 0;
    }
    if ((verb == 2) && (buf[i] == '"')) {
        verb = 0;
    }
    if ((verb == 3) && (buf[i] == '=') && (buf[(i+1) % bl] == '=')) {
        verb = 0;
    }
    if ((verb == 0) && (buf[i] == '\'')) {
        verb = 1;
    }
    if ((verb == 0) && (buf[i] == '"')) {
        verb = 2;
    }
    if ((verb == 0) && (buf[i] == '=') && (buf[(i+1) % bl] == '=')) {
        i++;
        verb = 3;
    }
    if (verb == 0) {
       buf[i] = toupper(buf[i]);
    }
  }

  if ((strstr(buf,"PROCEDURE") != NULL) && (strstr(buf,"DIVISION") != NULL)) {
       fclose(declareTmpFile);

       inLinkageSection = 0;
       inProcDivision = 1;
       startProcDivision = 1;

       if (!linkageSectionPresent) {
           if (!wstorageSectionPresent) {
             fputs("       WORKING-STORAGE SECTION.\n",(FILE*)fp2);
             fputs("       77  QWICSPTR USAGE IS POINTER.\n",(FILE*)fp2);
             fputs("       77  QWICSLEN PIC 9(9).\n",(FILE*)fp2);
             fputs("       77  QWICSJMP PIC X(200).\n",(FILE*)fp2);
             fputs("       77  DFHRESP-NORMAL PIC S9(8) COMP VALUE 0.\n",(FILE*)fp2);
             fputs("       77  DFHRESP-INVREQ PIC S9(8) COMP VALUE 16.\n",(FILE*)fp2);
             fputs("       77  DFHRESP-LENGERR PIC S9(8) COMP VALUE 22.\n",(FILE*)fp2);
             fputs("       77  DFHRESP-QIDERR PIC S9(8) COMP VALUE 44.\n",(FILE*)fp2);
             fputs("       77  DFHRESP-NOTFND PIC S9(8) COMP VALUE 13.\n",(FILE*)fp2);
             fputs("       77  DFHRESP-PGMIDERR PIC S9(8) COMP VALUE 27.\n",(FILE*)fp2);
           }
           if (!eibPresent) {
               includeCbk("DFHEIBLK",(FILE*)fp2,NULL,NULL,1);
           }
           fputs("       LINKAGE SECTION.\n",(FILE*)fp2);
           inLinkageSection = 1;
           includeCbkL((FILE*)fp2);
           inLinkageSection = 0;
       }
       if (!commAreaPresent) {
           fputs("       01  DFHCOMMAREA PIC X.\n",(FILE*)fp2);
       }
       if (!(strstr(buf,".") >= &buf[7])) {
           // USING clause is multi-line
           skipLinesMode = 1;
       } 
       sprintf(buf,"%s","       PROCEDURE DIVISION USING DFHCOMMAREA");
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
       outputDot = 1;
  }

  if (execCmd == 0) {
      char *cmd = strstr(buf,"EXEC");
      if ((cmd != NULL) && strstr(buf,"CICS")) {
          allowIntoParam = 0;
          allowFromParam = 0;
          execCmd = 1;
          isReturn = 0;
          isXctl = 0;
          execSQLCnt = 0;
          setptrbuf[0] = 0;
      }
      if ((cmd != NULL) && strstr(buf,"SQL")) {
          allowIntoParam = 0;
          allowFromParam = 0;
          execCmd = 2;
          execSQLCnt = 0;
          setptrbuf[0] = 0;
      }
  }

  if (execCmd == 0) {
      char copyBuf[255];
      int copyOnly = 0;
      copyBuf[0] = 0x00;
      if ((strstr(buf,". COPY ") != NULL) || (strstr(buf,"  COPY ") != NULL) || (strstr(buf,"COPY ") == (char*)&buf[7])) {
          copyOnly = 1;
          int m = (int)(strstr(buf,"COPY")-buf); 
          memset(copyBuf,' ',255);
          strncpy(&copyBuf[7],&buf[m],72-m);
          copyBuf[80] = 0x00; 
          buf[m] = 0x00;
          int n = m-1;
          while (n >= 7) {
              if (buf[n] != ' ') {
                  copyOnly = 0;
                  break;
              }
              n--;
          }
      }
      if (!copyOnly) {
          if ((strstr(buf,"LINKAGE") != NULL) && (strstr(buf,"SECTION") != NULL)) {
            if (!wstorageSectionPresent) {
              fputs("       WORKING-STORAGE SECTION.\n",(FILE*)fp2);
              fputs("       77  QWICSPTR USAGE IS POINTER.\n",(FILE*)fp2);
              fputs("       77  QWICSLEN PIC 9(9).\n",(FILE*)fp2);
              fputs("       77  QWICSJMP PIC X(200).\n",(FILE*)fp2);
              fputs("       77  DFHRESP-NORMAL PIC S9(8) COMP VALUE 0.\n",(FILE*)fp2);
              fputs("       77  DFHRESP-INVREQ PIC S9(8) COMP VALUE 16.\n",(FILE*)fp2);
              fputs("       77  DFHRESP-LENGERR PIC S9(8) COMP VALUE 22.\n",(FILE*)fp2);
              fputs("       77  DFHRESP-QIDERR PIC S9(8) COMP VALUE 44.\n",(FILE*)fp2);
              fputs("       77  DFHRESP-NOTFND PIC S9(8) COMP VALUE 13.\n",(FILE*)fp2);
              fputs("       77  DFHRESP-PGMIDERR PIC S9(8) COMP VALUE 27.\n",(FILE*)fp2);
            }
            if (!eibPresent) {
                includeCbk("DFHEIBLK",(FILE*)fp2,NULL,NULL,1);
            }
          }

          char *f = strstr(buf,"DFHRESP");
          if (f != NULL) {
            f[7] = '-';
            int i = 8;
            while ((f[i] != ')') && (f[i] != 0x00)) {
              i++;
            }
            if (f[i] == ')') {
              f[i] = ' ';
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
                  outputLinkageVar(&n,fp2);
              }

              includeDeclares((FILE*)fp2);
              startProcDivision = 0;
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
      }
      if (strlen(copyBuf) > 0) {
          fputs("\n",fp2);
          processCopyLine(copyBuf,fp2);
      }
  } else {
      char *cmd = strstr(buf,"END-EXEC");
      if (cmd != NULL) {
        if (hasDotTerminator(buf)) {
            outputDot = 1;
        } else {
            outputDot = 0;
        }
      }

      processExecLine(buf,fp2);

      if (cmd != NULL) {
          execCmd = 0;
          if (isReturn || isXctl) {
              char gb[30];
              sprintf(gb,"%s%s\n","           GOBACK",getExecTerminator(0));
              fputs(gb,(FILE*)fp2);
              isReturn = 0;
              isXctl = 0;
          }
          if (strlen(setptrbuf) > 0) {
            fputs(setptrbuf,(FILE*)fp2);            
          }
      }
  }

  if ((strstr(buf,"LINKAGE") != NULL) && (strstr(buf,"SECTION") != NULL)) {
       inLinkageSection = 1;
       linkageSectionPresent = 1;
       includeCbkL((FILE*)fp2);
  }

  if ((strstr(buf,"WORKING-STORAGE") != NULL) && (strstr(buf,"SECTION") != NULL)) {
       wstorageSectionPresent = 1;
       fputs("       77  QWICSPTR USAGE IS POINTER.\n",(FILE*)fp2);
       fputs("       77  QWICSLEN PIC 9(9).\n",(FILE*)fp2);
       fputs("       77  QWICSJMP PIC X(200).\n",(FILE*)fp2);
       fputs("       77  DFHRESP-NORMAL PIC S9(8) COMP VALUE 0.\n",(FILE*)fp2);
       fputs("       77  DFHRESP-INVREQ PIC S9(8) COMP VALUE 16.\n",(FILE*)fp2);
       fputs("       77  DFHRESP-LENGERR PIC S9(8) COMP VALUE 22.\n",(FILE*)fp2);
       fputs("       77  DFHRESP-QIDERR PIC S9(8) COMP VALUE 44.\n",(FILE*)fp2);
       fputs("       77  DFHRESP-NOTFND PIC S9(8) COMP VALUE 13.\n",(FILE*)fp2);
       fputs("       77  DFHRESP-PGMIDERR PIC S9(8) COMP VALUE 27.\n",(FILE*)fp2);
  }
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

   declareTmpFile = fopen("declare.tmp", "w");
   if (declareTmpFile == NULL) {
	    printf("%s\n","Could not write declare tmp file.");
	    return -1;
   }

   char *p = getenv("QWICS_CBKPATH");
   if (p != NULL) {
     cbkPath = p;
   }
   printf("%s%s\n","Searching for copybooks in ",cbkPath);

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

   sprintf(oname,"%s%s%s","exec_",progname,".cob");
   fp2 = fopen(oname,"w");
   if (fp2 == NULL) {
        printf("%s%s\n","Could not create output file: ",oname);
        return -1;
   }

   while (fgets(buf, 255, (FILE*)fp) != NULL) {
      processLine(buf,fp2);
   }

   fclose(fp);
   fclose(fp2);
   return 0;
}
