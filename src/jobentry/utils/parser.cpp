/*******************************************************************************************/
/*   QWICS Server COBOL environment standard dataset service program replacements          */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 12.10.2023                                  */
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
#include <string.h>
#include <unistd.h>

#define MAX_TOKENS 200

int comment = 0;


int tokenize(char *line, char **tokens, int *tokenNum) {
    int l = strlen(line),i;
    int tokenPos = 0;
    int verbatim = 0;

    for (i = 0; i < l; i++) {
        if (i < l-1) {
            if ((line[i] == '/') && (line[i+1] == '*')) {
                comment = 1;
                i++;
                continue;
            }
            if ((line[i] == '*') && (line[i+1] == '/')) {
                comment = 0;
                i++;
                continue;
            }
        }
        if (comment > 0) {
            continue;
        }

        if (line[i] == '\'') {
            if (verbatim > 0) {
                if (tokenPos > 0) {
                    tokens[*tokenNum][tokenPos] = 0x00;
                    tokenPos = 0;
                    (*tokenNum)++;
                    if (*tokenNum >= MAX_TOKENS) {
                        return 0;
                    }
                }                
                verbatim = 0;
            } else {
                verbatim = 1;                
            }
            continue;
        }

        if (verbatim > 0) {
            tokens[*tokenNum][tokenPos] = line[i];
            tokenPos++;
            if (tokenPos >= 50) {
                return 0;
            }
            continue;
        }

        if ((line[i] == ' ') || (line[i] == '(') || (line[i] == ')') || 
            (line[i] == '=') || (line[i] == ',') || (line[i] == '-')) {
            if (tokenPos > 0) {
                tokens[*tokenNum][tokenPos] = 0x00;
                tokenPos = 0;
                (*tokenNum)++;
                if (*tokenNum >= MAX_TOKENS) {
                    return 0;
                }
            }

            if (line[i] != ' ') {
                tokens[*tokenNum][tokenPos] = line[i];
                tokenPos++;
                if (tokenPos >= 50) {
                    return 0;
                }
                tokens[*tokenNum][tokenPos] = 0x00;
                tokenPos = 0;
                (*tokenNum)++;
                if (*tokenNum >= MAX_TOKENS) {
                    return 0;
                }
            }

            if (line[i] != '-') {
                // Has continuation line
                return 1;
            }
        } else {
            tokens[*tokenNum][tokenPos] = line[i];
            tokenPos++;
            if (tokenPos >= 50) {
                return 0;
            }
        }
    }

    return 0;
}


int getTokenIndex(char *name, char **tokens, int tokenNum) {
    int i;

    for (i = 0; i < tokenNum; i++) {
        if (strcmp(tokens[i],name) == 0) {
            return i;
        }
    }

    return -1;
}
