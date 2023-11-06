/*******************************************************************************************/
/*   QWICS Batch Job Entry System                                                          */
/*                                                                                         */
/*   Author: Philipp Brune               Date: 11.06.2023                                  */
/*                                                                                         */
/*   Copyright (C) 2023 by Philipp Brune  Email: Philipp.Brune@hs-neu-ulm.de               */
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

#include "JobCard.h"
#include "../dataset/DataSetDef.h"


class EXEC : public JobCard {
 private:
  static DataSetDef *procLib;
  static DataSetDef *linkLib;
  
  int execPROC(char *proc, Parameters *params);
  int execPGM(char *pgm, Parameters *params, char *_stdin, char *_stdout, char *_stderr);
 
 public:
  EXEC(char *name);
  virtual ~EXEC();

  virtual int equalsType(char *type);
  virtual int execute();
};
