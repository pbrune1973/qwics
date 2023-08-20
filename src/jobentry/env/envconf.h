/*******************************************************************************************/
/*   QWICS Server COBOL environment config variables                                               */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 20.08.2023                                  */
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

#ifndef _envconf_h
#define _envconf_h

#define GETENV_NUMBER(v,ev,d) ((v < 0) ? (v=getEnvNumber(ev,d)) : v)
#define GETENV_STRING(v,ev,d) ((v == NULL) ? (v=getEnvString(ev,d)) : v)

int getEnvNumber(char *varname, int def);
char *getEnvString(char *varname, char *def);

#endif
