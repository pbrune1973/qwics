/*******************************************************************************************/
/*   QWICS Server COBOL Batch Program Preprocessor (EXEC SQL only)                         */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 08.11.2023                                  */
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
int usingLinesMode = 0;
int skipFillerDefs = 0;
int fillerDefLevel = 0;
int xmlBlock = 0;
FILE *fpLookup = NULL;


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
int include = 0;


char *strstr_noverb(char *buf, char *s) {
   int i = 0, j = 0, m = strlen(buf), l = strlen(s);
   int verb = 0;
   if ((l == 0) || (m == 0)) {
     return NULL;
   }
   while (i <= m-l) {
     if (verb == 0) {
        for (j = 0; j < l; j++) {
           if (buf[i+j] != s[j]) {
             break;
           }
        }       
        if (j == l) {
           return &buf[i];
        }
     }
     if ((verb == 1) && (buf[i] == '\'')) {
       verb = 0;
     }
     if ((verb == 2) && (buf[i] == '"')) {
       verb = 0;
     }
     if ((verb == 3) && (buf[i] == '=') && (buf[(i+1) % m] == '=')) {
       verb = 0;
     }
     if ((verb == 0) && (buf[i] == '\'')) {
       verb = 1;
     }
     if ((verb == 0) && (buf[i] == '"')) {
       verb = 2;
     }
     if ((verb == 0) && (buf[i] == '=') && (buf[(i+1) % m] == '=')) {
       i++;
       verb = 3;
     }
     i++;
   }

   return NULL;
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


int lookupSymbolInInput(char *symbol) {
    char buf[255];
    rewind(fpLookup);
    while (fgets(buf, 255, fpLookup) != NULL) {
        if (buf[6] != '*') {
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
                return 0;
            }

            if (strstr(buf,symbol) != NULL) {
                return 1;
            }
        }
    }
    return 0;
}


void processLine(char *buf, FILE *fp2, FILE *fp);


// Insert declares temp file for db cursors etc. declared not in proc division
int includeDeclares(FILE *outFile) {
    FILE *df = fopen("declare.tmp", "r");
    if (df == NULL) {
        printf("%s\n","Info: Declare tmp file not found.");
        return -1;
    }

    char line[255],linebuf[255];
    while (fgets(line, 255, (FILE*)df) != NULL) {
      int i = strlen(line)-1;
      while (((line[i] == 10) || (line[i] == 13) || (line[i] == ' ') || (line[i] == '.')) && (i > 0)) i--;	  
      line[i+1] = 0x00; 
      sprintf(linebuf,"%s%s\n",line,getExecTerminator(0));     
      fputs(linebuf,outFile);
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


void strrepmult(char *line, char *s, char *r) {
    int sl = 0;
    while ((sl = strlen(s)) > 0) {
        strrep(line,s,r);
        s = &s[sl+1];
        r = &r[strlen(r)+1];
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
            strrepmult(line,findStr,replStr);
        }
        if (raw == 1) {
            fputs(line,outFile);
        } else {
            processLine(line,outFile,cbk);
        }
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
int processReplacingClause(char *buf, int *pos, char *pattern) {
    int pl = 0;
    while ((pl = strlen(pattern)) > 0) {
        pattern = &pattern[pl+1];
    }

    int i = *pos;
    int pseudo = 0;
    while ((buf[i] == ' ') && (i < 72)) {
        i++;
    }
    if ((i == 72) || (buf[i] == '.') || (buf[i] == '\n') || (buf[i] == '\r') || 
        (buf[i] == 0x00)) {
        i--;
        (*pos) = i;
        return 0;
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
        pattern[j+1] = 0x00;
    } else {
        int j = 0;
        while (buf[i] != ' ') {
            pattern[j] = buf[i];
            i++;
            j++;
        }
        pattern[j] = 0x00;
        pattern[j+1] = 0x00;
    }

    while ((buf[i] != ' ') && (buf[i] != '.') && (buf[i] != '\n') && 
           (buf[i] != '\r') && (buf[i] != 0x00) && (i < 72)) {
        i++;
    }
    i--;
    (*pos) = i;
    return 1;
}


// Process include/copy line
void processCopyLine(char *buf, FILE *fp2, FILE *fp) {
    char linebuf[255];
    int i,m = strlen(buf);
    char token[255];
    int tokenPos = 0;
    char cbkFile[255];
    char findStr[255];
    char replStr[255];

    findStr[0] = 0x00;
    replStr[0] = 0x00;

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
		            include = 0;
                    if (includeCbk(cbkFile,fp2,findStr,replStr,0) < 0) {
                        includeCbkIO(cbkFile,fp2);
                    }
                } else {
                    include = 0;
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
                linebuf[m-1] = '\n';
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
                        i++;
                        // Look for another REPLACING clause
                        if (!processReplacingClause(buf,&i,findStr)) {
                            if (hasDotTerminator(buf)) {                               
                                include = 4;
                            } else {
                                if (fgets(buf, 255, (FILE*)fp) != NULL) {
                                    i = 7;
                                    m = strlen(buf);
                                    if (m > 72) {
                                        m = 72;
                                    }
                                    processReplacingClause(buf,&i,findStr);
                                }
                            }
                        }
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
void processExecLine(char *buf, FILE *fp2, FILE *fp) {
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
                            processCopyLine(buf,fp2,fp);
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
                        if (i < m-1) {
                            if ((token[0] == '<') || (token[0] == '>') || (token[0] == '=')) {
                                if ((buf[i+1] == '=') || (buf[i+1] == '>')) {
                                    token[1] = buf[i+1];
                                    token[2] = 0x00;
                                    i++;
                                }
                            }
                        }
                        sprintf(execbuf,"%s%s%s\n","           DISPLAY \"TPMI:",
                                token,getExecTerminator(1));
                        if (inProcDivision) {
                          fputs(execbuf, (FILE*)fp2);
                        }
                        if (!inProcDivision && (execSQLCnt == 4)) {
                            fputs(execbuf, (FILE*)declareTmpFile);
                        }
                    } else {
                        if (buf[i] != '\"') {
	                        token[tokenPos] = buf[i];
        	                tokenPos++;
                        }
                    }
                }
            }
        }
    }
}


void processLine(char *buf, FILE *fp2, FILE *fp) {
  // Handle comments
  if (buf[6] == '*') {
    fputs(buf,(FILE*)fp2);
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
       // Remove tabs
       if (buf[i] == '\t') {
         buf[i] = ' ';
       }
    }
  }

  if (include > 0) {
     processCopyLine(buf,fp2,fp);
     return;
  }

  if ((strstr(buf,"PROCEDURE") != NULL) && (strstr(buf,"DIVISION") != NULL)) {
    fclose(declareTmpFile);

    inLinkageSection = 0;
    inProcDivision = 1;
    startProcDivision = 1;
  }

  if (execCmd == 0) {
      char *cmd = strstr_noverb(buf,"EXEC");
      if (cmd != NULL) {
         cmd[0] = '\n';
         cmd[1] = 0x00;
         fputs(buf,(FILE*)fp2);
         cmd[0] = 'E';
         cmd[1] = 'X';
         memset(buf,' ',(int)(cmd-buf));
      }
      if ((cmd != NULL) && strstr_noverb(buf,"CICS")) {
          printf("EXEC CICS not supported in batch program!\n");
          exit(1);
      }
      if ((cmd != NULL) && strstr_noverb(buf,"SQL")) {
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
          fputs(buf,(FILE*)fp2);

          if (inProcDivision) {
               if (!isEmptyLine(buf)) {
                   if ((buf[6] != '*') && hasDotTerminator(buf)) {
                       outputDot = 1;
                   } else {
                       outputDot = 0;
                   }
               }
          }

          if ((outputDot == 1) && startProcDivision) {
              char buf[80];
              if (sqlca) {
                  sprintf(buf,"%s%s%s\n","           DISPLAY \"TPMI:SET SQLCODE\" ",
                         "SQLCODE",getExecTerminator(0));
                  fputs(buf,(FILE*)fp2);
              }

              includeDeclares((FILE*)fp2);
              startProcDivision = 0;
          }
      }
      if (strlen(copyBuf) > 0) {
          fputs("\n",fp2);
          processCopyLine(copyBuf,fp2,fp);
      }
  } else {
      char *cmd = strstr_noverb(buf,"END-EXEC");
      if (cmd != NULL) {
        if (hasDotTerminator(buf)) {
            outputDot = 1;
        } else {
            outputDot = 0;
        }
      }

      processExecLine(buf,fp2,fp);

      if (cmd != NULL) {
          execCmd = 0;
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
   fpLookup = fopen(argv[1], "r");

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
      processLine(buf,fp2,fp);
   }

   fclose(fp);
   fclose(fpLookup);
   fclose(fp2);
   return 0;
}
