/*******************************************************************************************/
/*   QWICS Server Mapset Definition Preprocessor                                           */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 20.09.2018                                  */
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

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char currentName[9];
char currentMacro[9];

char currentMapSet[9];

struct mapDef {
    char name[9];
    int lines;
    int cols;
    int line;
    int col;
    int justify;
    int ctrl;
} currentMap;

struct fieldDef {
    char name[9];
    char value[255];
    char picin[255];
    char picout[255];
    int length;
    int x,y;
    int justify;
    int attr;
} currentField;

int currentFieldSet = 0;
int currentMapIsSet = 0;
int firstFieldOfMap = 1;
int firstMapOfSet   = 1;


// Field Definition Status
#define NONE      0
#define LENGTH    1
#define POS       2
#define TYPE      3
#define INITIAL   4
#define ATTRIB    5
#define PICOUT    6
#define PICIN     7
#define JUSTIFY   8
#define SIZE      9
#define LINE     10
#define COLUMN   11
#define CTRL     12

// ATTRB flags
#define ATTR_ASKIP   1
#define ATTR_PROT    2
#define ATTR_UNPROT  4
#define ATTR_NUM     8
#define ATTR_NORM   16
#define ATTR_BRT    32
#define ATTR_DRK    64
#define ATTR_IC    128
#define ATTR_FSET  256

// CTRL flags
#define CTRL_FREEKB  1
#define CTRL_FRSET   2
#define CTRL_ALARM   4
#define CTRL_PRINT   8


int status = NONE;
int valCount = 0;


void processToken(char *token) {
    printf("%s\n",token);

    // Map definition params
    if (status == LINE) {
        currentMap.line = atoi(token);
        status = NONE;
        valCount = 0;
        return;
    }
    
    if (status == COLUMN) {
        currentMap.col = atoi(token);
        status = NONE;
        valCount = 0;
        return;
    }
    
    if ((status == SIZE) && (valCount == 0)) {
        currentMap.lines = atoi(token);
        valCount++;
        return;
    }
    
    if ((status == SIZE) && (valCount > 0)) {
        currentMap.cols = atoi(token);
        status = NONE;
        valCount = 0;
        return;
    }

    if (status == CTRL) {
        if (strcmp(token,"FREEKB") == 0) {
            currentMap.ctrl = currentMap.ctrl | CTRL_FREEKB;
        }
        if (strcmp(token,"FRSET") == 0) {
            currentMap.ctrl = currentMap.ctrl | CTRL_FRSET;
        }
        if (strcmp(token,"ALARM") == 0) {
            currentMap.ctrl = currentMap.ctrl | CTRL_ALARM;
        }
        if (strcmp(token,"PRINT") == 0) {
            currentMap.ctrl = currentMap.ctrl | CTRL_PRINT;
        }
    }
    

    // Field definition params
    if (status == LENGTH) {
        currentField.length = atoi(token);
        status = NONE;
        valCount = 0;
        return;
    }

    if ((status == POS) && (valCount == 0)) {
        currentField.y = atoi(token);
        valCount++;
        return;
    }
    
    if ((status == POS) && (valCount > 0)) {
        currentField.x = atoi(token);
        status = NONE;
        valCount = 0;
        return;
    }
    
    if (status == INITIAL) {
        if (strlen(token) > 1) {
            if (token[0] == '\'') {
                token[0] = '"';
            }
            if (token[strlen(token)-1] == '\'') {
                token[strlen(token)-1] = '"';
            }
        }
        sprintf(currentField.value,"%s",token);
        status = NONE;
        valCount = 0;
        return;
    }
    
    if (status == PICOUT) {
        if (strlen(token) > 1) {
            if (token[0] == '\'') {
                token[0] = '"';
            }
            if (token[strlen(token)-1] == '\'') {
                token[strlen(token)-1] = '"';
            }
        }
        sprintf(currentField.picout,"%s",token);
        status = NONE;
        valCount = 0;
        return;
    }
    
    if (status == PICIN) {
        if (strlen(token) > 1) {
            if (token[0] == '\'') {
                token[0] = '"';
            }
            if (token[strlen(token)-1] == '\'') {
                token[strlen(token)-1] = '"';
            }
        }
        sprintf(currentField.picin,"%s",token);
        status = NONE;
        valCount = 0;
        return;
    }
    
    if (status == ATTRIB) {
        if (strcmp(token,"ASKIP") == 0) {
            currentField.attr = currentField.attr | ATTR_ASKIP;
        }
        if (strcmp(token,"UNPROT") == 0) {
            currentField.attr = currentField.attr | ATTR_UNPROT;
        }
        if (strcmp(token,"PROT") == 0) {
            currentField.attr = currentField.attr | ATTR_PROT;
        }
        if (strcmp(token,"NUM") == 0) {
            currentField.attr = currentField.attr | ATTR_NUM;
        }
        if (strcmp(token,"NORM") == 0) {
            currentField.attr = currentField.attr | ATTR_NORM;
        }
        if (strcmp(token,"BRT") == 0) {
            currentField.attr = currentField.attr | ATTR_BRT;
        }
        if (strcmp(token,"DRK") == 0) {
            currentField.attr = currentField.attr | ATTR_DRK;
        }
        if (strcmp(token,"IC") == 0) {
            currentField.attr = currentField.attr | ATTR_IC;
        }
        if (strcmp(token,"FSET") == 0) {
            currentField.attr = currentField.attr | ATTR_FSET;
        }
    }
    
    if (status != NONE) {
        valCount++;
    }
    
    if (strcmp(token,"LENGTH") == 0) {
        status = LENGTH;
        valCount = 0;
    }
    if (strcmp(token,"POS") == 0) {
        status = POS;
        valCount = 0;
    }
    if (strcmp(token,"TYPE") == 0) {
        status = TYPE;
        valCount = 0;
    }
    if (strcmp(token,"INITIAL") == 0) {
        status = INITIAL;
        valCount = 0;
    }
    if (strcmp(token,"ATTRB") == 0) {
        status = ATTRIB;
        currentField.attr = 0;
        valCount = 0;
    }
    if (strcmp(token,"PICOUT") == 0) {
        status = PICOUT;
        valCount = 0;
    }
    if (strcmp(token,"PICIN") == 0) {
        status = PICIN;
        valCount = 0;
    }
    if (strcmp(token,"JUSTIFY") == 0) {
        status = JUSTIFY;
        valCount = 0;
    }
    if (strcmp(token,"SIZE") == 0) {
        status = SIZE;
        valCount = 0;
    }
    if (strcmp(token,"LINE") == 0) {
        status = LINE;
        valCount = 0;
    }
    if (strcmp(token,"COLUMN") == 0) {
        status = COLUMN;
        valCount = 0;
    }
    if (strcmp(token,"CTRL") == 0) {
        status = CTRL;
        valCount = 0;
    }
}


char token[2048];
int tokenPos = 0;
int verbatim = 0;

void tokenize(char *buf, int start) {
    int i,m = strlen(buf);
    if (m > 72) m = 72;
    if (buf[0] == '*') {
        //Comment line
        return;
    }
    for (i = start; i < m-1; i++) {
        if ((buf[i] == '\'') && !verbatim) {
            if (tokenPos > 0) {
                token[tokenPos] = 0x00;
                processToken(token);
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
            processToken(token);
            tokenPos = 0;
            verbatim = 0;
            continue;
        }
        if ((buf[i] != '\'') && verbatim) {
            token[tokenPos] = buf[i];
            tokenPos++;
            continue;
        }
        
        if ((buf[i] == '(') || (buf[i] == ')') || (buf[i] == ',') || (buf[i] == '=') || (buf[i] == '/') ||
            (buf[i] == ' ') || (buf[i] == '\n') || (buf[i] == '\r')) {
            if (tokenPos > 0) {
                token[tokenPos] = 0x00;
                processToken(token);
                tokenPos = 0;
            }
        } else {
            token[tokenPos] = buf[i];
            tokenPos++;
        }
    }
}
    
    
int processMapDef(char *buf, FILE *fp2, FILE *fp3, FILE *fp4, FILE *fp5, FILE *fp6, FILE *fp7) { 
    if (buf[0] == '*') {
        //Comment line
        return -1;
    }
    int i = 0;
    while ((i < 8) && (buf[i] != ' ')) {
        currentName[i] = buf[i];
        i++;
    }
    currentName[i] = 0x00;
    while ((buf[i] == ' ') && (i < 72)) {
        i++;
    }
    int offset = i;
    i = 0;
    while ((i < 8) && (buf[i+offset] != ' ')) {
        currentMacro[i] = buf[i+offset];
        i++;
    }
    currentMacro[i] = 0x00;
    printf("%s%s%s\n",currentName,":",currentMacro);

    if (currentMapIsSet) {
        // Output map definition
        if (!firstMapOfSet) {
            fprintf(fp2,"%s","]},");
        }
        fprintf(fp2,"%s%s%s","\"",currentMap.name,"\":{");
        fprintf(fp2,"%s%d%s","\"line\":",currentMap.line,",");
        fprintf(fp2,"%s%d%s","\"col\":",currentMap.col,",");
        fprintf(fp2,"%s%d%s","\"lines\":",currentMap.lines,",");
        fprintf(fp2,"%s%d%s","\"cols\":",currentMap.cols,",");
        fprintf(fp2,"%s%d%s","\"ctrl\":",currentMap.ctrl,",");
        fprintf(fp2,"%s","\"fields\":[");
        
        fprintf(fp3,"%s%s%s\n","       01  ",currentMap.name,"I.");
        fprintf(fp4,"%s%s%s\n","       01  ",currentMap.name,"O.");
        fprintf(fp7,"%s%s%s\n","       01  ",currentMap.name,"L.");
        fprintf(fp5,"%s%s\n","*",currentMap.name);
        //fprintf(fp5,"%s%s%s%s%s\n","           DISPLAY \"TPMI:",currentMap.name,"\" ",currentMap.name,"I.");
        fprintf(fp6,"%s%s\n","*",currentMap.name);
        //fprintf(fp6,"%s%s%s%s%s\n","           DISPLAY \"TPMI:",currentMap.name,"\" ",currentMap.name,"O.");
        firstMapOfSet = 0;
    }
    currentMapIsSet = 0;
    
    if (currentFieldSet) {
        // Output field definition
        if (!firstFieldOfMap) {
            fprintf(fp2,"%s",",");
        }
        fprintf(fp2,"%s%s%s","{\"name\":\"",currentField.name,"\",");
        fprintf(fp2,"%s%s%s","\"value\":",currentField.value,",");
        fprintf(fp2,"%s%s%s","\"picout\":",currentField.picout,",");
        fprintf(fp2,"%s%s%s","\"picin\":",currentField.picin,",");
        fprintf(fp2,"%s%d%s","\"length\":",currentField.length,",");
        fprintf(fp2,"%s%d%s","\"x\":",currentField.x,",");
        fprintf(fp2,"%s%d%s","\"y\":",currentField.y,",");
        fprintf(fp2,"%s%d%s","\"justify\":",currentField.justify,",");
        fprintf(fp2,"%s%d%s","\"attr\":",currentField.attr,"}");

        if (strlen(currentField.name) > 0) {
            if ((currentField.attr & ATTR_NUM) > 0) {
                fprintf(fp3,"%s%s%s%d%s",
                            "           05  ",currentField.name,"I PIC 9(",currentField.length,")");
                fprintf(fp4,"%s%s%s%d%s",
                            "           05  ",currentField.name,"O PIC 9(",currentField.length,")");
                fprintf(fp7,"%s%s%s%d%s",
                            "           05  ",currentField.name,"L PIC 9(",currentField.length,")");
            } else {
                fprintf(fp3,"%s%s%s%d%s",
                        "           05  ",currentField.name,"I PIC X(",currentField.length,")");
                fprintf(fp4,"%s%s%s%d%s",
                        "           05  ",currentField.name,"O PIC X(",currentField.length,")");
                fprintf(fp7,"%s%s%s%d%s",
                        "           05  ",currentField.name,"L PIC X(",currentField.length,")");
            }
            if (strlen(currentField.value) > 2) {
                fprintf(fp3,"%s%s"," VALUE ",currentField.value);
                fprintf(fp4,"%s%s"," VALUE ",currentField.value);
            }
            fprintf(fp3,"%s\n",".");
            fprintf(fp4,"%s\n",".");
            fprintf(fp7,"%s\n",".");
            fprintf(fp5,"%s%s%s%s%s\n","           DISPLAY \"TPMI:",currentField.name,"\" ",currentField.name,"I.");
            fprintf(fp6,"%s%s%s%s%s\n","           DISPLAY \"TPMI:",currentField.name,"\" ",currentField.name,"O.");
        }
        firstFieldOfMap = 0;
    }
    currentFieldSet = 0;
    
    if (strstr(currentMacro,"DFHMSD") != NULL) {
        sprintf(currentMapSet,"%s",currentName);
        firstMapOfSet = 1;
        return -2;
    }
    if (strstr(currentMacro,"DFHMDI") != NULL) {
        sprintf(currentMap.name,"%s",currentName);
        currentMap.line  = 1;
        currentMap.col   = 1;
        currentMap.lines = 24;
        currentMap.cols  = 80;
        currentMap.ctrl  = 0;
        currentMapIsSet = 1;
        firstFieldOfMap = 1;
    }
    if (strstr(currentMacro,"DFHMDF") != NULL) {
        sprintf(currentField.name,"%s",currentName);
        sprintf(currentField.picout,"%s","\"\"");
        sprintf(currentField.picin,"%s","\"\"");
        sprintf(currentField.value,"%s","\"\"");
        currentField.length = 0;
        currentField.x = 0;
        currentField.y = 0;
        currentField.justify = 0;
        currentField.attr = 0;
        currentFieldSet = 1;
    }
    return i+offset;
}


int main(int argc, char **argv) {
   FILE *fp,*fp2,*fp3,*fp4,*fp5,*fp6,*fp7;
   char buf[255],oname[255];

   buf[0] = 0x00;

   if (argc < 2) {
	printf("%s\n","Usage: mapprep <Mapset Definition>");
	return -1;
   }

   fp = fopen(argv[1], "r");
   if (fp == NULL) {
	printf("%s%s\n","No input file: ",argv[1]);
	return -1;
   }	
   
   sprintf(oname,"%s%s%s","../copybooks/",argv[1],".js");
   fp2 = fopen(oname,"w");
   if (fp2 == NULL) {
        printf("%s%s\n","Could not create output file: ",oname);
        return -1;
   }
    
   fprintf(fp2,"%s","{");

   sprintf(oname,"%s%s%s","../copybooks/",argv[1],"I.cpy");
   fp3 = fopen(oname,"w");
   if (fp3 == NULL) {
       printf("%s%s\n","Could not create output file: ",oname);
       return -1;
   }
    
   sprintf(oname,"%s%s%s","../copybooks/",argv[1],"O.cpy");
   fp4 = fopen(oname,"w");
   if (fp4 == NULL) {
       printf("%s%s\n","Could not create output file: ",oname);
       return -1;
   }

   sprintf(oname,"%s%s%s","../copybooks/",argv[1],"I.dsp");
   fp5 = fopen(oname,"w");
   if (fp5 == NULL) {
       printf("%s%s\n","Could not create output file: ",oname);
       return -1;
   }
    
   sprintf(oname,"%s%s%s","../copybooks/",argv[1],"O.dsp");
   fp6 = fopen(oname,"w");
   if (fp6 == NULL) {
       printf("%s%s\n","Could not create output file: ",oname);
       return -1;
   }
    
   sprintf(oname,"%s%s%s","../copybooks/",argv[1],"L.cpy");
   fp7 = fopen(oname,"w");
   if (fp7 == NULL) {
       printf("%s%s\n","Could not create output file: ",oname);
       return -1;
   }
    
   while (fgets(buf, 255, (FILE*)fp) != NULL) {
       int pos = processMapDef(buf,fp2,fp3,fp4,fp5,fp6,fp7);
       if (pos >= 0) {
           tokenize(buf,pos);
           while ((((strlen(buf) >= 72) && ((buf[71] == 'X') || (buf[71] == '-'))) || (buf[0] == '*')) && (fgets(buf, 255, (FILE*)fp) != NULL)) {
               tokenize(buf,15);
           }
       }
       if (pos == -2) {
           while ((((strlen(buf) >= 72) && ((buf[71] == 'X') || (buf[71] == '-'))) || (buf[0] == '*')) && (fgets(buf, 255, (FILE*)fp) != NULL));
       }
   }

   fprintf(fp2,"%s","]}}");
 
   fclose(fp);
   fclose(fp2);
   fclose(fp3);
   fclose(fp4);
   fclose(fp5);
   fclose(fp6);
   fclose(fp7);
   return 0;
}
