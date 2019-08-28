/*******************************************************************************************/
/*   QWICS Server COBOL environment config variables                                               */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 28.08.2019                                  */
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


int getEnvNumber(char *varname, int def) {
  char *p = getenv(varname);
  int v = def;
  if (p != NULL) {
    v = atoi(p);
  }
  printf("%s%s%s%d\n","Using ",varname," = ",v);
  return v;
}


char *getEnvString(char *varname, char *def) {
  char *p = getenv(varname);
  if (p == NULL) {
    p = def;
  }
  printf("%s%s%s%s\n","Using ",varname," = ",p);
  return p;
}
